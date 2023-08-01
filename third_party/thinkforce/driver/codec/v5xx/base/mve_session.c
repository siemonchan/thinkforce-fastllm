/*
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#ifdef EMULATOR
#include "emulator_userspace.h"
#include <unistd.h>
#else
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/debugfs.h>
#endif

#include "mve_session.h"
#include "mve_fw.h"
#include "mve_driver.h"
#include "mve_com.h"
#include "mve_mem_region.h"
#include "mve_session_buffer.h"

#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_mem_backend.h"

#include "mve_rsrc_irq.h"
#include "mve_rsrc_register.h"
#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_pm.h"
#include "mve_rsrc_log.h"

#include <host_interface_v3/mve_protocol_def.h>

/* List containing all allocated, but not freed, sessions */
static LIST_HEAD(sessions);

/* Semaphore protecting the sessions list */
static struct semaphore sessions_sem;

/* Number of sessions currently created */
static int num_sessions;

/* This define controls how many frames each job will process in a multisession
 * scenario. */
#define MULTI_SESSION_FRAME_COUNT 1

/* Max string name. */
#define MAX_STRINGNAME_SIZE 128

/* Max message size in 4byte words */
#define MAX_MESSAGE_SIZE_IN_WORDS 192
/**
 * Request to fill or empty a buffer.
 */
struct session_enqueue_request
{
    struct list_head list;
    struct mve_base_buffer_details param;
};

#ifndef DISABLE_WATCHDOG
/* Maximum pings before considering the hardware is dead */
#define MAX_NUM_PINGS 3

/* Watchdog ping interval in millisecond */
#define WATCHDOG_PING_INTERVAL 3000

#if (1 == MVE_MEM_DBG_TRACKMEM)
#ifdef EMULATOR
extern struct semaphore watchdog_sem;
#endif
#endif

/**
 * @brief Info about the watchdog.
 */
struct watchdog_info
{
    struct task_struct *watchdog_thread; /**< Pointer to the watchdog thread.*/
    struct semaphore killswitch;         /**< Signal this to immediately terminate
                                          *   the watchdog thread (up(killswitch)) */
    atomic_t refcount;                   /**< Reference-count for the watchdog */
};

static struct watchdog_info watchdog;

#ifdef UNIT
static bool watchdog_ignore_pong = false;
#endif

#endif /* DISABLE_WATCHDOG */

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
#define DVFS_DEBUG_MODE 1
#else
#define DVFS_DEBUG_MODE 0
#endif

#if (1 == DVFS_DEBUG_MODE)
static atomic_t dvfs_b_enable = ATOMIC_INIT(1);
#endif

/**
 * Checks whether the supplied session is valid or not. Note that this function
 * is not thread safe.
 * @param session The session to check validity of.
 * @return True if the session is valid, false otherwise.
 */
static bool is_session_valid(struct mve_session *session)
{
    struct list_head *pos;

    if (NULL == session)
    {
        return false;
    }

    list_for_each(pos, &sessions)
    {
        struct mve_session *ptr = container_of(pos, struct mve_session, list);

        if (session == ptr)
        {
            return true;
        }
    }

    return false;
}

/**
 * Acquires the session lock for a valid session. Note that the client has to
 * release the session's semaphore when the session no longer has to be
 * protected from concurrent access.
 * @param session The session to lock.
 * @return 0 if the semaphore was successfully acquired, non-zero on failure.
 */
static int lock_session(struct mve_session *session)
{
    return down_interruptible(&session->semaphore);
}

/**
 * Unlock the session lock.
 * @param session The session to unlock.
 */
static void unlock_session(struct mve_session *session)
{
    up(&session->semaphore);
}

/**
 * Frees all resources allocated for the session. This is an internal
 * function called by kref_put when the reference counter for a session
 * reaches 0.
 * @param ref Pointer to the kref member of the mve_session structure.
 */
static void free_session(struct kref *ref)
{
    bool ret;
    struct mve_mem_virt_region region;

    struct mve_session *session = container_of(ref,
                                               struct mve_session,
                                               refcount);

    lock_session(session);

    /* Unmap all external buffers */
    mve_session_buffer_unmap_all(session);

    mve_queue_destroy(session->response_events);
    mve_queue_destroy(session->mve_events);

    mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_IN_QUEUE, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_OUT_QUEUE, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_IN, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_OUT, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_IN, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_OUT, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_mem_virt_region_get(VIRT_MEM_REGION_RPC_QUEUE, &region);
    mve_mmu_unmap_pages(session->mmu_ctx, region.start);

    mve_rsrc_dma_mem_free(session->msg_in_queue);
    mve_rsrc_dma_mem_free(session->msg_out_queue);
    mve_rsrc_dma_mem_free(session->buf_input_in);
    mve_rsrc_dma_mem_free(session->buf_input_out);
    mve_rsrc_dma_mem_free(session->buf_output_in);
    mve_rsrc_dma_mem_free(session->buf_output_out);
    mve_rsrc_dma_mem_free(session->rpc_area);

    MVE_RSRC_MEM_FREE(session->role);
    ret = mve_fw_unload(session->mmu_ctx, session->fw_inst);
    if (false == ret)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_session, MVE_LOG_DEBUG, "Firmware unload failed.");
    }
    mve_mmu_destroy_ctx(session->mmu_ctx);

    MVE_RSRC_MEM_FREE(session);
}

/**
 * Checks whether the supplied reference is a reference to a valid session. If
 * the session is valid, the session's reference counter is incremented by one.
 * You must call release_session when the reference to the session is dropped.
 * @param session The session to check validity of.
 * @return True if the session is valid, false otherwise.
 */
static bool acquire_session(struct mve_session *session)
{
    bool is_valid = false;
    int sem_taken;

    sem_taken = down_interruptible(&sessions_sem);

    is_valid = is_session_valid(session);
    if (true == is_valid)
    {
        kref_get(&session->refcount);
    }

    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }

    return is_valid;
}

/**
 * Release the reference to the session. If the session's reference counter
 * reaches 0, then the session is freed.
 * @param session The session to release.
 */
static void release_session(struct mve_session *session)
{
    int sem_taken = down_interruptible(&sessions_sem);

    kref_put(&session->refcount, free_session);

    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }
}

/**
 * Convert MVE protocol response codes to event codes.
 * @param response_code The MVE protocol response code to convert
 * @returns The correspondig event code
 */
static enum mve_base_event_code response_code_to_mve_event(uint32_t response_code)
{
    switch (response_code)
    {
        case MVE_RESPONSE_CODE_INPUT:
            return MVE_BASE_EVENT_INPUT;
        case MVE_RESPONSE_CODE_OUTPUT:
            return MVE_BASE_EVENT_OUTPUT;
        case MVE_RESPONSE_CODE_EVENT_PROCESSED:
            return MVE_BASE_EVENT_PROCESSED;
        case MVE_RESPONSE_CODE_STREAM_ERROR:
            return MVE_BASE_EVENT_STREAM_ERROR;
        case MVE_RESPONSE_CODE_SWITCHED_OUT:
            return MVE_BASE_EVENT_SWITCHED_OUT;
        case MVE_RESPONSE_CODE_SWITCHED_IN:
            return MVE_BASE_EVENT_SWITCHED_IN;
        case MVE_RESPONSE_CODE_ERROR:
            return MVE_BASE_EVENT_FW_ERROR;
        case MVE_RESPONSE_CODE_PONG:
            return MVE_BASE_EVENT_PONG;
        case MVE_RESPONSE_CODE_STATE_CHANGE:
            return MVE_BASE_EVENT_STATE_CHANGED;
        case MVE_RESPONSE_CODE_SET_OPTION_CONFIRM:
        case MVE_RESPONSE_CODE_SET_OPTION_FAIL:
            return MVE_BASE_EVENT_SET_PARAMCONFIG;
        case MVE_RESPONSE_CODE_INPUT_FLUSHED:
            return MVE_BASE_EVENT_INPUT_FLUSHED;
        case MVE_RESPONSE_CODE_OUTPUT_FLUSHED:
            return MVE_BASE_EVENT_OUTPUT_FLUSHED;
        case MVE_RESPONSE_CODE_DUMP:
            return MVE_BASE_EVENT_CODE_DUMP;
        case MVE_RESPONSE_CODE_JOB_DEQUEUED:
            return MVE_BASE_EVENT_JOB_DEQUEUED;
        case MVE_RESPONSE_CODE_IDLE:
            return MVE_BASE_EVENT_IDLE;
        case MVE_RESPONSE_CODE_FRAME_ALLOC_PARAM:
            return MVE_BASE_EVENT_ALLOC_PARAMS;
        case MVE_RESPONSE_CODE_SEQUENCE_PARAMETERS:
            return MVE_BASE_EVENT_SEQUENCE_PARAMS;
        case MVE_BUFFER_CODE_PARAM:
            return MVE_BASE_EVENT_BUFFER_PARAM;
        case MVE_RESPONSE_CODE_REF_FRAME_UNUSED:
            return MVE_BASE_EVENT_REF_FRAME_RELEASED;
        case MVE_RESPONSE_CODE_TRACE_BUFFERS:
            return MVE_BASE_EVENT_FW_TRACE_BUFFERS;
        default:
            WARN_ON(true);
            break;
    }

    return -1;
}

/**
 * Returns a string description of the supplied RPC code. Used when
 * building debug binaries where each received RPC request is printed
 * to the dmesg.
 * @param code RPC code.
 * @return A textual description of the RPC code.
 */
static char *print_rpc_code(uint32_t code)
{
    switch (code)
    {
        case MVE_RPC_FUNCTION_DEBUG_PRINTF:
            return "MVE_RPC_FUNCTION DEBUG_PRINTF";
        case MVE_RPC_FUNCTION_MEM_ALLOC:
            return "MVE_RPC_FUNCTION_MEM_ALLOC";
        case MVE_RPC_FUNCTION_MEM_RESIZE:
            return "MVE_RPC_FUNCTION_MEM_RESIZE";
        case MVE_RPC_FUNCTION_MEM_FREE:
            return "MVE_RPC_FUNCTION_MEM_FREE";
        default:
            return "UNKNOWN RPC";
    }
}

/* State machine description table */
static const bool state_table[7][7] =
{
    /* SWITCHED_OUT   SWITCHING_IN   SWITCHING_OUT   WAITING_FOR_RESPONSE   ACTIVE_JOB    TO / FROM */

    {         true,          true,          false,           false,         true },  /* SWITCHED_OUT */
    {        false,         false,          false,            true,         true },  /* SWITCHING_IN */
    {         true,         false,          false,           false,        false },  /* SWITCHING_OUT */
    {         true,         false,          false,           false,         true },  /* WAITING_FOR_RESPONSE */
    {         true,         false,           true,            true,         true },  /* ACTIVE_JOB */
};

/* State to string. */
static const char *state_to_char[] =
{
    "Switched out",
    "Switching in",
    "Switching out",
    "Waiting for response",
    "Active job",
    "Quick flush"
};

/**
 * Change the state of the session to the supplied state. The state is not changed
 * if the transition is not supported.
 * @param session  The session to change state on.
 * @param to_state The state to change to.
 * @return True if the state transition is valid, false otherwise.
 */
static bool change_session_state(struct mve_session *session, enum MVE_SESSION_STATE to_state)
{
    bool ret;

    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "Changing state. '%s' -> '%s'.", state_to_char[session->state.state], state_to_char[to_state]);

    ret = state_table[session->state.state][to_state];
    if (false != ret)
    {
        session->state.state = to_state;
    }
    else
    {
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_ERROR, session, "Invalid state change. '%s' -> '%s'.", state_to_char[session->state.state], state_to_char[to_state]);
    }

    return ret;
}

/**
 * This function is called when a message response have been received and
 * that the session state should be changed because of this. Note that
 * this session assumes that the supplied session is valid!
 * @param session The session that received the message response.
 */
static void response_received(struct mve_session *session)
{
    bool res = change_session_state(session, STATE_ACTIVE_JOB);
    WARN_ON(false == res);
}

/**
 * Returns the buffer descriptor (if one exists) for a user allocated buffer
 * mapping matching the supplied MVE handle.
 * @param session The session.
 * @param mve_handle  MVE handle of the user allocated buffer.
 * @param port_index The port index.
 * @return The buffer descriptor if the supplied user allocated buffer has been
 *         mapped. NULL if no such descriptor exists.
 */
static struct mve_buffer_client *get_external_descriptor_by_mve_handle(struct mve_session *session,
                                                                       uint16_t mve_handle, enum mve_port_index port_index)
{
    struct list_head *pos;

    list_for_each(pos, &session->external_buffers)
    {
        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

        if (mve_handle == ptr->mve_handle && port_index == ptr->port_index)
        {
            return ptr;
        }
    }

    return NULL;
}

/**
 * Returns the buffer descriptor (if one exists) for a user allocated buffer mapping
 * matching the supplied MVE address.
 * @param session The session.
 * @param mve_addr The address in MVE virtual address space where the buffer is mapped.
 * @return The buffer scriptor for the matching buffer is one exists, NULL otherwise.
 */
static struct mve_buffer_client *get_external_descriptor_by_mve_address(struct mve_session *session,
                                                                        mve_addr_t mve_addr)
{
    struct list_head *pos;

    list_for_each(pos, &session->external_buffers)
    {
        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

        if (mve_addr == ptr->buffer->mapping.region.start + ptr->buffer->mapping.offset_in_page)
        {
            return ptr;
        }
    }

    return NULL;
}

/**
 * Enqueues a message in the input queue of the supplied session. This function
 * assumes that the session is valid and the session mutex has been locked.
 * @param session The session that wants to enqueue a message.
 * @param code    MVE message code (see the protocol definition).
 * @param size    Size in bytes of the data attached to the message.
 * @param data    Pointer to the attached data.
 * @return MVE_BASE_ERROR_NONE on success or a suitable error code on failure.
 */
static mve_base_error enqueue_message(struct mve_session *session,
                                      uint32_t code,
                                      uint32_t size,
                                      void *data)
{
    mve_base_error ret;

    WARN_ON(code > MVE_REQUEST_CODE_IDLE_ACK && code != MVE_BUFFER_CODE_PARAM);
    WARN_ON(size > MAX_MESSAGE_SIZE_IN_WORDS * sizeof(uint32_t));
    WARN_ON(size > 0 && NULL == data);

    ret = mve_com_add_message(session, code, size, data);

    return ret;
}

/**
 * Called when the state of the HW has changed (MVE_BASE_HW_STATE_STOPPED <-> MVE_BASE_HW_STATE_RUNNING). Note that
 * this function assumes that the supplied session is valid.
 * @param session The session which has changed state.
 * @param state   The new state of the HW.
 */
static void change_session_hw_state(struct mve_session *session, enum mve_base_hw_state state)
{
    WARN_ON(MVE_BASE_HW_STATE_STOPPED != state && MVE_BASE_HW_STATE_RUNNING != state);

    session->state.hw_state = state;
    session->state.pending_hw_state = state;
}

static void send_event_to_userspace(struct mve_session *session, struct mve_base_event_header *event)
{
    mve_queue_add_event(session->mve_events, event);
#ifdef EMULATOR
    {
        int t = (int)(((uintptr_t)session->filep) & 0xFFFFFFFF);
        write(t, &t, sizeof(int));
    }
#endif
}

/**
 * Return a pending buffer to userspace.
 * @param session           Pointer to session.
 * @param list              Pointer to pending buffer request.
 * @param emtpy_this_buffer True if the buffer was enqueued on the input port, false otherwise.
 */
static void return_pending_buffer_to_userspace(struct mve_session *session, struct session_enqueue_request *request, bool empty_this_buffer)
{
    struct mve_base_event_header *event;
    struct mve_base_buffer_userspace *buffer;
    enum mve_base_event_code event_code;

    event_code = (false != empty_this_buffer) ? MVE_BASE_EVENT_INPUT : MVE_BASE_EVENT_OUTPUT;
    event = mve_queue_create_event(event_code, sizeof(struct mve_base_buffer_userspace));
    if (NULL == event)
    {
        return;
    }

    buffer = (struct mve_base_buffer_userspace *)event->data;
    memset(buffer, 0, sizeof(*buffer));
    buffer->buffer_id = request->param.buffer_id;
    buffer->handle = request->param.handle;
    buffer->flags = request->param.flags;
    buffer->mve_flags = request->param.mve_flags;

    send_event_to_userspace(session, event);
    MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
}

/**
 * Return all pending buffers to userspace.
 * @param session           Pointer to session.
 * @param list              Pointer to list with pending buffers.
 * @param emtpy_this_buffer True if the buffers were enqueued on the input port, false otherwise.
 */
static void return_all_pending_buffers_to_userspace(struct mve_session *session, struct list_head *list, bool empty_this_buffer)
{
    struct list_head *pos, *next;

    list_for_each_safe(pos, next, list)
    {
        struct session_enqueue_request *request = container_of(pos, struct session_enqueue_request, list);

        return_pending_buffer_to_userspace(session, request, empty_this_buffer);
        list_del(&request->list);
        MVE_RSRC_MEM_FREE(request);
    }
}

/**
 * Enqueue a job message to firmware message queue.
 * @param session    The sessions to add the message to
 * @return Error code describing the error. MVE_BASE_ERROR_NONE on success.
 */
static mve_base_error enqueue_job(struct mve_session *session)
{
    struct mve_request_job cmd;
    mve_base_error err;

    cmd.cores = mver_scheduler_get_ncores();
    cmd.frames = (num_sessions > 1) ? MULTI_SESSION_FRAME_COUNT : 0;
    cmd.flags = 0;

    err = enqueue_message(session, MVE_REQUEST_CODE_JOB, sizeof(cmd), &cmd);

    /* Remember the job size since this will be used to determine whether the
     * session must be rescheduled or not in mve_session_has_work_callback */
    session->state.job_frame_size = cmd.frames;

    return err;
}

/**
 * Enqueue pending buffers to firmware queue.
 * @param session           Pointer to session.
 * @param list              Pointer to list with pending buffers.
 * @param emtpy_this_buffer Boolean if buffer is of type 'empty' or 'fill'.
 */
static void enqueue_pending_buffers(struct mve_session *session, struct list_head *list, bool empty_this_buffer)
{
    struct list_head *pos, *next;

    list_for_each_safe(pos, next, list)
    {
        mve_base_error ret;
        struct session_enqueue_request *request = container_of(pos, struct session_enqueue_request, list);

        ret = mve_session_buffer_enqueue_internal(session, &request->param, empty_this_buffer);

        /* If request timed out, then firmware queue is full and we need to retry again later. */
        if (MVE_BASE_ERROR_TIMEOUT == ret)
        {
            break;
        }
        else if (MVE_BASE_ERROR_NONE != ret)
        {
            /* On failure return the buffer to userspace. */
            return_pending_buffer_to_userspace(session, request, empty_this_buffer);
        }

        list_del(&request->list);
        MVE_RSRC_MEM_FREE(request);
    }
}

/**
 * Notify the scheduler that this session wants to be scheduled for execution.
 * Note the the session mutex must not be taken when calling this function.
 * @param session   The session to schedule.
 * @return True if the session was scheduled, false if the session was put in
 *         the list of sessions to schedule in the future or if it cannot be
 *         scheduled at all (e.g. no firmware loaded).
 */
static bool schedule_session(struct mve_session *session)
{
    bool ret = true;

    if (false == session->fw_loaded)
    {
        /* Don't schedule a session that hasn't loaded a firmware */
        return false;
    }

    if (STATE_SWITCHED_OUT == session->state.state)
    {
        ret = mver_scheduler_execute((mver_session_id)session);
    }

    if (STATE_SWITCHING_OUT == session->state.state ||
        STATE_SWITCHED_OUT == session->state.state)
    {
        /* Failed to schedule session */
        ret = false;
    }

    return ret;
}

/**
 * This function is used to query whether a switched in session has any remaining
 * work to do.
 * @param session The session to check
 * @return true if the session has work to do, false otherwise
 */
static bool fw_has_work(struct mve_session *session)
{
    /* The session has work to do if it's waiting for response from the FW or the FW
     * has not signalled idleness */
    return (session->pending_response_count != 0) || (IDLE_STATE_IDLE != session->state.idle_state);
}

/**
 * Helper function to request a session to switch out. A switch out message
 * will be sent to the FW asking to to switch out as soon as possible.
 */
static void switchout_session(struct mve_session *session)
{
    if (STATE_SWITCHING_OUT != session->state.state &&
        STATE_SWITCHED_OUT != session->state.state &&
        STATE_WAITING_FOR_RESPONSE != session->state.state &&
        STATE_SWITCHING_IN != session->state.state)
    {
        /* Use polling mode here in case FW needs time
         * to handle the previous msg and into the
         * switchingable state */
        int i;
        bool ret;

        for (i = 0; i < 20; i++)
        {
            ret = change_session_state(session, STATE_SWITCHING_OUT);
            if (false != ret)
            {
                break;
            }

            /* Failed to request state transition, so sleep and retry. */
            unlock_session(session);
            msleep(10);

            /* Lock session session again after sleep. */
            lock_session(session);
        }

        if (false != ret)
        {
            uint32_t param = 0;

            enqueue_message(session, MVE_REQUEST_CODE_SWITCH, sizeof(param), &param);
            mver_irq_signal_mve((mver_session_id)session);
        }
    }
    else if (STATE_SWITCHING_IN == session->state.state ||
             STATE_WAITING_FOR_RESPONSE == session->state.state)
    {
        /* This session is in a state that doesn't allow direct switch out.
         * Request that the session is switched out as soon as possible */
        session->state.request_switchout = true;
    }
}

static bool supports_idle_ack(struct mve_session *session)
{
    struct mve_base_fw_version *ver = mve_fw_get_version(session->fw_inst);
    bool ret = false;

    if (NULL != ver &&
        ((MVE_BASE_FW_MAJOR_PROT_V2 == ver->major && MVE_BASE_FW_MINOR_V4 <= ver->minor) ||
         (MVE_BASE_FW_MAJOR_PROT_V3 == ver->major && MVE_BASE_FW_MINOR_V1 <= ver->minor) ||
         (MVE_BASE_FW_MAJOR_PROT_V3 < ver->major)))
    {
        ret = true;
    }

    return ret;
}

/**
 * This function is called by the IRQ worker thread for all messages that
 * are not responses to get/set parameter/config. The purpose of this function
 * is to inspect each event and change the driver state if needed. Before this
 * function returns, the event is added to the mve_events queue and made visible
 * to user-space.
 * @param session The session.
 * @param event   The event received.
 */
static void process_generic_event(struct mve_session *session, struct mve_base_event_header *event)
{
    bool delete_event = false;

    switch (event->code)
    {
        case MVE_BASE_EVENT_OUTPUT: /* Intentional fall-through! */
        case MVE_BASE_EVENT_INPUT:
        {
            mve_com_buffer buffer;
            struct mve_buffer_client *buffer_client;
            enum mve_port_index port_index;

            port_index = (event->code == MVE_BASE_EVENT_INPUT) ? MVE_PORT_INDEX_INPUT : MVE_PORT_INDEX_OUTPUT;

            /* Mark buffer as invalid */
            buffer.frame.nHandle = 0;

            if (STATE_SWITCHED_OUT == session->state.state ||
                STATE_SWITCHING_IN == session->state.state)
            {
                response_received(session);
            }

            if (MVE_BASE_EVENT_OUTPUT == event->code)
            {
                mve_com_get_output_buffer(session, &buffer);
            }
            else
            {
                mve_com_get_input_buffer(session, &buffer);
            }

            /* nHandle is the first member of each buffer type so it is safe to
             * access this member from any of the buffer types. */
            buffer_client = get_external_descriptor_by_mve_handle(session,
                                                                  buffer.frame.nHandle, port_index);
            if (NULL != buffer_client)
            {
                if ((session->state.flush_target & MVE_BASE_FLUSH_QUICK) &&
                    ((session->state.flush_target & MVE_BASE_FLUSH_OUTPUT_PORT &&
                      buffer_client->port_index == MVE_PORT_INDEX_OUTPUT) ||
                     (session->state.flush_target & MVE_BASE_FLUSH_INPUT_PORT &&
                      buffer_client->port_index == MVE_PORT_INDEX_INPUT)))
                {
                    /* In quick flush state, save the returned buffers in the
                     * quick_flush_buffers linked-list. When all buffers have been
                     * flushed, enqueue them again. */
                    list_add_tail(&buffer_client->quick_flush_list, &session->quick_flush_buffers);
                    buffer_client->mve_handle = MVE_HANDLE_INVALID;
                    /* Do not pass MVE_BASE_EVENT_OUTPUT/INPUT to userspace when
                     * performing a quick flush. */
                    return;
                }
                else
                {
                    struct mve_base_event_header *usr_event;
                    struct mve_base_buffer_userspace *buf;
                    mve_base_error res;

                    res = mve_session_buffer_dequeue_internal(session, buffer_client->buffer->info.buffer_id, &buffer);
                    if (MVE_BASE_ERROR_NONE != res)
                    {
                        return;
                    }

                    if (MVE_BASE_EVENT_OUTPUT == event->code &&
                        MVE_BASE_BUFFERFLAG_EOS == (buffer_client->flags & MVE_BASE_BUFFERFLAG_EOS))
                    {
                        session->state.eos_queued = false;
                        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "Resetting eos_queued.");
                    }

                    usr_event = mve_queue_create_event(event->code, sizeof(struct mve_base_buffer_userspace));
                    if (NULL == usr_event)
                    {
                        return;
                    }

                    buf = (struct mve_base_buffer_userspace *)usr_event->data;
                    mve_session_buffer_convert_to_userspace(buf, buffer_client);

                    event = usr_event;
                    delete_event = true;
                }
            }
            else
            {
                WARN_ON(true);
                /* This buffer has already been sent to userspace. Don't send it again */
                return;
            }

            /* Try to enqueue any pending buffer requests. */
            if (MVE_BASE_EVENT_INPUT == event->code)
            {
                enqueue_pending_buffers(session, &session->queued_input_buffers, true);
            }
            else
            {
                enqueue_pending_buffers(session, &session->queued_output_buffers, false);
            }

            break;
        }
        case MVE_BASE_EVENT_INPUT_FLUSHED:
        case MVE_BASE_EVENT_OUTPUT_FLUSHED:
        {
            if (0 != (session->state.flush_target & MVE_BASE_FLUSH_QUICK))
            {
                if (MVE_BASE_EVENT_INPUT_FLUSHED == event->code)
                {
                    session->state.flush_state |= MVE_BASE_FLUSH_INPUT_PORT;
                }
                else if (MVE_BASE_EVENT_OUTPUT_FLUSHED == event->code)
                {
                    session->state.flush_state |= MVE_BASE_FLUSH_OUTPUT_PORT;
                }

                if ((session->state.flush_target & MVE_BASE_FLUSH_ALL_PORTS) ==
                    session->state.flush_state)
                {
                    /* All buffers have been flushed, re-enqueue them */
                    struct list_head *pos, *next;

                    list_for_each_safe(pos, next, &session->quick_flush_buffers)
                    {
                        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, quick_flush_list);
                        struct mve_base_buffer_details param;
                        mve_base_error ret;
                        uint32_t mve_flags = ptr->mve_flags;
                        uint32_t crc_offset = 0;

                        if (0 != (session->state.flush_target & MVE_BASE_FLUSH_QUICK_SET_INTERLACE))
                        {
                            mve_flags |= MVE_BUFFER_FRAME_FLAG_INTERLACE;
                        }
                        else
                        {
                            mve_flags &= ~MVE_BUFFER_FRAME_FLAG_INTERLACE;
                        }

                        if (NULL != ptr->crc)
                        {
                            crc_offset = ptr->crc->info.offset;
                        }

                        ptr->in_use -= 1;
                        WARN_ON(0 > ptr->in_use);

                        memset(&param, 0, sizeof(param));
                        param.handle = ptr->buffer->info.handle;
                        param.buffer_id = ptr->buffer->info.buffer_id;
                        param.filled_len = ptr->filled_len;
                        param.flags = ptr->flags;
                        param.mve_flags = mve_flags;
                        param.crc_offset = crc_offset;
                        param.timestamp = 0;
                        ret = mve_session_buffer_enqueue_internal(session,
                                                                  &param,
                                                                  ptr->port_index == 0 ? true : false);
                        WARN_ON(MVE_BASE_ERROR_NONE != ret);
                        if (MVE_BASE_ERROR_NONE == ret)
                        {
                            /* Reset the process to detect idleness and request that this session is
                             * switched in again if a switch out has already started.
                             * The reason this condition is inside of the loop is that quick_flush_buffers
                             * list could be empty and no buffers will be enqueued. */
                            session->state.idle_state = IDLE_STATE_ACTIVE;
                            session->state.request_switchin = true;
                        }

                        list_del(&ptr->quick_flush_list);
                    }

                    session->state.flush_state = 0;
                    session->state.flush_target = 0;

                    /* The session can be switching out at this point as the result
                     * of idleness. Don't attempt to change state if this is true. */
                    if (STATE_SWITCHING_OUT != session->state.state)
                    {
                        response_received(session);
                    }
                }
                /* Do not pass this event on to userspace */
                return;
            }
            else
            {
                /* Mark all corresponding buffers as no longer in use by the MVE */
                struct list_head *pos;
                int port_index = 0;

                if (MVE_BASE_EVENT_INPUT_FLUSHED == event->code)
                {
                    port_index = 0;
                }
                else
                {
                    port_index = 1;
                }

                list_for_each(pos, &session->external_buffers)
                {
                    struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

                    if (port_index == ptr->port_index)
                    {
                        if (0 < ptr->in_use)
                        {
                            /* VIDDK-150: The FW has not returned all buffers. Enqueue another flush request */
                            uint32_t code = (1 == port_index) ? MVE_REQUEST_CODE_OUTPUT_FLUSH :
                                            MVE_REQUEST_CODE_INPUT_FLUSH;
                            enqueue_message(session, code, 0, NULL);
                            mver_irq_signal_mve((mver_session_id)session);
                            return;
                        }
                    }
                }

                if (MVE_BASE_EVENT_INPUT_FLUSHED == event->code)
                {
                    session->state.flush_target &= ~MVE_BASE_FLUSH_INPUT_PORT;
                }
                else
                {
                    session->state.flush_target &= ~MVE_BASE_FLUSH_OUTPUT_PORT;
                }
            }
            break;
        }
        case MVE_BASE_EVENT_STATE_CHANGED:
        {
            change_session_hw_state(session, *((enum mve_base_hw_state *)event->data));
            if (STATE_SWITCHING_OUT != session->state.state)
            {
                response_received(session);
            }
            break;
        }
        case MVE_BASE_EVENT_SWITCHED_IN:
        {
            /* Don't change state from STATE_WAITING_FOR_RESPONSE or STATE_SWITCHING_OUT! */
            if (STATE_WAITING_FOR_RESPONSE != session->state.state &&
                STATE_SWITCHING_OUT != session->state.state)
            {
                response_received(session);
            }

            /* In case FW in sleep while we are waiting for response */
            if (session->pending_response_count != 0)
            {
                mver_irq_signal_mve((mver_session_id)session);
            }
            break;
        }
        case MVE_BASE_EVENT_SWITCHED_OUT:
        {
            enum MVE_SESSION_STATE state = session->state.state;

            (void)change_session_state(session, STATE_SWITCHED_OUT);
            session->state.request_switchout = false;

            if (STATE_WAITING_FOR_RESPONSE == state)
            {
                session->state.request_switchin = true;
            }
            break;
        }
        case MVE_BASE_EVENT_PONG:
        {
            if (STATE_SWITCHED_OUT == session->state.state ||
                STATE_SWITCHING_IN == session->state.state)
            {
                response_received(session);
            }
            break;
        }
        case MVE_BASE_EVENT_JOB_DEQUEUED:
            /* Do not pass this event to userspace */
            if (STATE_SWITCHED_OUT == session->state.state ||
                STATE_SWITCHING_IN == session->state.state)
            {
                response_received(session);
            }
            return;
        case MVE_BASE_EVENT_STREAM_ERROR:

            break;
        case MVE_BASE_EVENT_PROCESSED:
            if (STATE_SWITCHED_OUT == session->state.state ||
                STATE_SWITCHING_IN == session->state.state)
            {
                response_received(session);
            }
            break;
        case MVE_BASE_EVENT_IDLE:
        {
            if (false != supports_idle_ack(session))
            {
                /* IDLE-ACK supported */
                switch (session->state.idle_state)
                {
                    case IDLE_STATE_ACTIVE:
                        session->state.idle_state = IDLE_STATE_PENDING;
                        break;

                    case IDLE_STATE_PENDING:
                        session->state.idle_state = IDLE_STATE_IDLE;
                        session->state.request_switchin = false;
#if (defined UNIT) && !(defined DISABLE_WATCHDOG)
                        if (true == watchdog_ignore_pong)
                        {
                            return;
                        }
#endif

#if SCHEDULER_MODE_IDLE_SWITCHOUT == 1
                        switchout_session(session);
#endif
                        enqueue_message(session, MVE_REQUEST_CODE_IDLE_ACK, 0, NULL);
#if SCHEDULER_MODE_IDLE_SWITCHOUT == 1
                        mver_irq_signal_mve((mver_session_id)session);
#endif
                        return;

                    default:
                        break;
                }

                enqueue_message(session, MVE_REQUEST_CODE_IDLE_ACK, 0, NULL);
                mver_irq_signal_mve((mver_session_id)session);
            }
            else
            {
                /* IDLE-ACK not supported */
                switch (session->state.idle_state)
                {
                    case IDLE_STATE_ACTIVE:
                        /* The first idle message doesn't mean the FW is idle. */
                        session->state.idle_state = IDLE_STATE_PENDING;
                        mver_irq_signal_mve((mver_session_id)session);
                        break;

                    case IDLE_STATE_PENDING:
                        /* May have to switch in again to be certain that the FW is really idle */
                        session->state.idle_state = IDLE_STATE_IDLE;

                        if (0 != session->output_buffer_count &&
                            0 != session->input_buffer_count &&
                            false != session->state.idle_dirty)
                        {
                            /* The idle message cannot fully be truested. The only way to really
                             * know if the FW is idle or not is to switch in the session again and check
                             * for idle messages. */
                            session->state.request_switchin = true;
                        }
                        else
                        {
                            session->state.request_switchin = false;
                        }
                        break;

                    default:
                        break;
                }
            }

            return;
        }
        /* By default, just pass the message on to user space */
        case MVE_BASE_EVENT_FW_ERROR:
        {
            struct mve_com_error *e = (struct mve_com_error *)(event->data);
            if (NULL != e && MVE_ERROR_UNSUPPORTED == e->error_code)
            {
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_ERROR, session, "Unsupported firmware.");
                /* If waiting for firmware response but instead get an error event of wrong firmware
                 * the wait on response_events queue should be interupted and error returned instead of
                 * waiting for watchdog to trigger
                 */
                if (STATE_WAITING_FOR_RESPONSE == session->state.state)
                {
                    mve_queue_interrupt(session->response_events);
                }
            }
            else if (NULL != e && MVE_ERROR_WATCHDOG == e->error_code && !strcmp(e->message, "Watchdog"))
            {
                struct mve_comm_area_host *host_area;
                struct mve_comm_area_mve *mve_area;

                host_area = mve_rsrc_dma_mem_map(session->msg_in_queue);
                mve_area = mve_rsrc_dma_mem_map(session->msg_out_queue);

                if (host_area != NULL && mve_area != NULL)
                {
                    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_ERROR, session, "host_area wpos: %d mve_area rpos: %d.", host_area->in_wpos, mve_area->in_rpos);
                }
                mve_rsrc_dma_mem_unmap(session->msg_in_queue);
                mve_rsrc_dma_mem_unmap(session->msg_out_queue);
            }
            break;
        }
        case MVE_BASE_EVENT_ALLOC_PARAMS:
        case MVE_BASE_EVENT_SEQUENCE_PARAMS: /* Intentional fallthrough */
        {
            /* Just forward these messages to userspace */
            break;
        }
        case MVE_BASE_EVENT_REF_FRAME_RELEASED:
        {
            struct mve_buffer_client *buffer_client;
            struct mve_response_ref_frame_unused *ref_frame;

            ref_frame = (struct mve_response_ref_frame_unused *)event->data;
            buffer_client = get_external_descriptor_by_mve_address(session, ref_frame->unused_buffer_address);
            if (NULL != buffer_client)
            {
                /* Create a new event and send that to userspace. A new event must be created
                 * because the original event contains the 32-bit virtual address of the buffer
                 * in MVE address space. Userspace wants the buffer handle which is a 64-bit value. */
                struct mve_base_event_header *usr_event;
                usr_event = mve_queue_create_event(event->code, sizeof(mve_base_buffer_handle_t));
                if (NULL == usr_event)
                {
                    return;
                }

                *(mve_base_buffer_handle_t *)usr_event->data = buffer_client->buffer->info.buffer_id;
                /* Swap the old event for the new one. Make sure the new event is deleted before
                 * returning. The old event will automatically be deleted by the caller */
                event = usr_event;
                delete_event = true;
            }

            break;
        }
        case MVE_BASE_EVENT_BUFFER_PARAM:
        {
            struct buffer_param
            {
                uint32_t type;
                uint32_t arg;
            };
            struct buffer_param *p = (struct buffer_param *)event->data;
            if (NULL != p && MVE_BASE_BUFFER_PARAM_TYPE_DPB_HELD_FRAMES == p->type)
            {
                session->pending_buffer_on_hold_count = p->arg;
                /* Don't send these events to userspace */
                return;
            }
            break;
        }
        case MVE_BASE_EVENT_FW_TRACE_BUFFERS:
        {
            /* Store information about the FW trace buffers */
            session->fw_trace = *(struct mve_com_trace_buffers *)event->data;
            /* Don't send this message to userspace */
            return;
        }
        default:
        {
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_ERROR, session, "Illegal event code. code=%u.", event->code);
            return;
        }
    }

    /* Add the event to the event queue. Userspace clients may now retrieve the
     * event. */
    send_event_to_userspace(session, event);
    if (false != delete_event)
    {
        MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
    }
}

/**
 * Called by the interrupt handler when an interrupt is received to handle
 * messages from the MVE. The messages are extracted and placed in different
 * queues for later processing.
 * @param session The session.
 */
static void handle_mve_message(struct mve_session *session)
{
    struct mve_msg_header header;
    uint32_t *data;

    data = mve_com_get_message(session, &header);
    while (NULL != data)
    {
        enum mve_base_event_code event_code;
        struct mve_base_event_header *event;
        uint32_t size;

        event_code = response_code_to_mve_event(header.code);
        size = header.size;

        event = mve_queue_create_event(event_code, size);
        if (NULL != event)
        {
            memcpy(event->data, data, size);

            if (MVE_BASE_EVENT_FW_ERROR == event->code)
            {
                struct mve_error
                {
                    uint32_t reason;
                    char msg[1];
                };

                struct mve_error *err = (struct mve_error *)event->data;
                MVE_LOG_PRINT(&mve_rsrc_log_session, MVE_LOG_ERROR, "Firmware error. reason=%u, msg=%s.", err->reason, err->msg);
            }

#ifndef DISABLE_WATCHDOG
#ifdef UNIT
            if (true == watchdog_ignore_pong)
            {}
            else
#endif
            {
                session->state.ping_count = 0;
                session->state.watchdog_state = WATCHDOG_PONGED;
            }
#endif
            if (MVE_BASE_EVENT_IDLE != event->code &&
                MVE_BASE_EVENT_SWITCHED_OUT != event->code)
            {
                /* Reset idleness */
                session->state.idle_state = IDLE_STATE_ACTIVE;
            }

            switch (event_code)
            {
                case MVE_BASE_EVENT_SET_PARAMCONFIG:
                    WARN_ON(STATE_SWITCHED_OUT == session->state.state);
                    response_received(session);
                    mve_queue_add_event(session->response_events, event);
#ifdef EMULATOR
                    {
                        int t = (int)(((uintptr_t)session->filep) & 0xFFFFFFFF);
                        write(t, &t, sizeof(int));
                    }
#endif
                    break;
                default:
                    process_generic_event(session, event);
                    break;
            }

            MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
        }
        MVE_RSRC_MEM_CACHE_FREE(data, header.size);
        data = mve_com_get_message(session, &header);
    }

    mve_rsrc_dma_mem_unmap(session->msg_in_queue);
    mve_rsrc_dma_mem_unmap(session->msg_out_queue);
}

#ifndef EMULATOR

mve_base_error mve_session_handle_rpc_mem_alloc(struct mve_session *session, int fd)
{
    struct mve_com_rpc rpc;
    mve_base_error err;

    bool ret = acquire_session(session);
    if (false == ret)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    lock_session(session);

    err = mve_com_get_rpc_message(session, &rpc);
    if (MVE_BASE_ERROR_NONE == err)
    {
        uint32_t size, max_size;
        uint32_t num_pages, max_pages;
        uint32_t alignment;
        phys_addr_t *pages = NULL;
        uint32_t mve_addr = 0;
        struct mve_buffer_info info;
        struct mve_buffer_external *buffer = NULL;

        if (fd < 0)
        {
            err = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            goto error;
        }

        size = rpc.params.mem_alloc.size;
        max_size = rpc.params.mem_alloc.max_size;
        alignment = rpc.params.mem_alloc.log2_alignment;

        num_pages = (size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;
        max_pages = (max_size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;

        info.allocator = MVE_BASE_BUFFER_ALLOCATOR_DMABUF;
        info.handle = fd;
        info.size = num_pages * MVE_MMU_PAGE_SIZE;

        buffer = mve_buffer_create_buffer_external(&info, 0);
        if (NULL != buffer)
        {
            bool ret = mve_buffer_map_physical_pages(buffer);
            if (false != ret && num_pages <= buffer->mapping.num_pages)
            {
                pages = buffer->mapping.pages;
            }
        }

        if (NULL != pages)
        {
            /* Map the allocated pages and return the MVE address */
            struct mve_mem_virt_region region;
            bool ret;

            if (MVE_BASE_MEMORY_REGION_PROTECTED == rpc.params.mem_alloc.region)
            {
                mve_mem_virt_region_get(VIRT_MEM_REGION_PROTECTED, &region);
            }
            else
            {
                mve_mem_virt_region_get(VIRT_MEM_REGION_OUT_BUF, &region);
            }

            ret = mve_mmu_map_pages_and_reserve(session->mmu_ctx,
                                                pages,
                                                &region,
                                                num_pages,
                                                max_pages,
                                                1 << alignment,
                                                buffer->mapping.num_pages,
                                                ATTRIB_SHARED_RW,
                                                ACCESS_READ_WRITE,
                                                false);
            if (false == ret)
            {
                pages = NULL;
                err = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            }
            else
            {
                /* Success! */
                mve_addr = region.start;
            }

            mve_buffer_unmap_physical_pages(buffer);
            mve_buffer_destroy_buffer_external(buffer);
            buffer = NULL;
        }

        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session,
                              "RPC alloc requested. size=%d(%u), num_pages=%d, addr=0x%X - 0x%X, alignment=%d.",
                              size, max_size, num_pages, mve_addr, mve_addr + max_size, alignment);

error:
        rpc.size = sizeof(uint32_t);
        rpc.params.data[0] = mve_addr;
    }

    rpc.state = MVE_RPC_STATE_RETURN;

    mve_com_put_rpc_message(session, &rpc);

    session->rpc_in_progress = false;
    mver_irq_signal_mve((mver_session_id)session);

    unlock_session(session);
    release_session(session);

    return err;
}

mve_base_error mve_session_handle_rpc_mem_resize(struct mve_session *session, int fd)
{
    struct mve_com_rpc rpc;
    mve_base_error err;

    bool ret = acquire_session(session);
    if (false == ret)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    lock_session(session);

    err = mve_com_get_rpc_message(session, &rpc);
    if (MVE_BASE_ERROR_NONE == err)
    {
        uint32_t mve_addr = rpc.params.mem_resize.ve_pointer;
        uint32_t new_size = rpc.params.mem_resize.new_size;
        uint32_t num_pages = (new_size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;
        uint32_t ret_addr = 0;
        bool ret;
        uint32_t mapped_pages, max_pages, num_new_pages;
        struct mve_buffer_info info;
        struct mve_buffer_external *buffer;

        if (fd < 0)
        {
            err = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            goto error;
        }

        ret = mve_mmu_map_info(session->mmu_ctx,
                               mve_addr,
                               &mapped_pages,
                               &max_pages);
        if (false == ret || num_pages > max_pages)
        {
            err = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            goto error;
        }

        num_new_pages = num_pages - mapped_pages;

        info.allocator = MVE_BASE_BUFFER_ALLOCATOR_DMABUF;
        info.handle = fd;
        info.size = num_new_pages * MVE_MMU_PAGE_SIZE;

        buffer = mve_buffer_create_buffer_external(&info, 0);
        if (NULL != buffer)
        {
            ret = mve_buffer_map_physical_pages(buffer);
            if (false != ret && num_new_pages <= buffer->mapping.num_pages)
            {
                ret = mve_mmu_map_resize(session->mmu_ctx,
                                         buffer->mapping.pages,
                                         mve_addr,
                                         num_pages,
                                         ATTRIB_SHARED_RW,
                                         ACCESS_READ_WRITE,
                                         true);

                if (false != ret)
                {
                    ret_addr = mve_addr;
                }
                mve_buffer_unmap_physical_pages(buffer);
            }
            mve_buffer_destroy_buffer_external(buffer);
            buffer = NULL;
        }
error:
        mver_scheduler_flush_tlb((mver_session_id)session);

        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "RPC resize requested. new_size=%d, fd: %d, num_pages=%d, addr=0x%X ret_addr= %#x.", new_size, fd, num_pages, mve_addr, ret_addr);

        rpc.size = sizeof(uint32_t);
        rpc.params.data[0] = ret_addr;
    }
    rpc.state = MVE_RPC_STATE_RETURN;
    mve_com_put_rpc_message(session, &rpc);

    session->rpc_in_progress = false;
    mver_irq_signal_mve((mver_session_id)session);

    unlock_session(session);
    release_session(session);

    return err;
}
#endif /* EMULATOR */

/**
 * This function is invoked when an interrupt has been received. It takes care
 * of all RPC requests the MVE can make.
 * @param session The session that has received a RPC request.
 */
static void handle_rpc_request(struct mve_session *session)
{
    struct mve_com_rpc rpc;
    mve_base_error err;

    if (false != session->rpc_in_progress)
    {
        return;
    }

    err = mve_com_get_rpc_message(session, &rpc);

    if (MVE_BASE_ERROR_NONE == err)
    {
        session->rpc_in_progress = true;
        if (MVE_COM_RPC_FUNCTION_DEBUG_PRINTF != rpc.call_id)
        {
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session, "<- RPC %s", print_rpc_code(rpc.call_id));
        }

        switch (rpc.call_id)
        {
            case MVE_COM_RPC_FUNCTION_DEBUG_PRINTF:
            {
                struct mve_base_event_header *event;
                uint16_t size;

                /* Make sure the string is NULL terminated */
                rpc.params.debug_print.string[NELEMS(rpc.params.debug_print.string) - 1] = '\0';
                size = strlen(rpc.params.debug_print.string);

                /* Strip trailing new line. */
                if ((size > 0) && rpc.params.debug_print.string[size - 1] == '\n')
                {
                    rpc.params.debug_print.string[--size] = '\0';
                }

                /* Include end of string. */
                size++;

                event = mve_queue_create_event(MVE_BASE_EVENT_RPC_PRINT, size);
                if (NULL == event)
                {
                    return;
                }

                memcpy(event->data, rpc.params.debug_print.string, size);

                mve_queue_add_event(session->mve_events, event);
#ifdef EMULATOR
                {
                    int t = (int)(((uintptr_t)session->filep) & 0xFFFFFFFF);
                    write(t, &t, sizeof(int));
                }
#endif
                MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
                rpc.size = 0;
                break;
            }
            case MVE_COM_RPC_FUNCTION_MEM_ALLOC:
            {
                uint32_t size, max_size;
                uint32_t num_pages, max_pages;
                uint32_t num_sys_pages;
                uint32_t alignment;
                phys_addr_t *pages;
                uint32_t mve_addr = 0;
                bool secure = mve_fw_secure(session->fw_inst);

                size = rpc.params.mem_alloc.size;
                max_size = rpc.params.mem_alloc.max_size;
                alignment = rpc.params.mem_alloc.log2_alignment;

                num_pages = (size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;
                max_pages = (max_size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;

//                num_sys_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
                num_sys_pages = num_pages;

#ifdef EMULATOR
                /* Always allocate max pages as the workaround for emulator resize issue EGIL-1421*/
                num_pages = (max_pages > num_pages) ? max_pages : num_pages;
                secure = false;
#endif
                if (false != secure)
                {
                    struct mve_base_event_header *event;
                    struct mve_base_rpc_memory *rpc_memory;

                    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "%s send event to userspace size: %d", __FUNCTION__, num_pages * MVE_MMU_PAGE_SIZE);

                    event = mve_queue_create_event(MVE_BASE_EVENT_RPC_MEM_ALLOC, sizeof(struct mve_base_rpc_memory));
                    if (NULL != event)
                    {
                        rpc_memory = (struct mve_base_rpc_memory *)event->data;
                        rpc_memory->region = rpc.params.mem_alloc.region;
                        rpc_memory->size = num_pages * MVE_MMU_PAGE_SIZE;
                    }
                    mve_queue_add_event(session->mve_events, event);
                    MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
                    return;
                }

                pages = MVE_RSRC_MEM_ALLOC_PAGES(num_sys_pages);
                if (NULL != pages || 0 == num_sys_pages)
                {
                    /* Map the allocated pages and return the MVE address */
                    struct mve_mem_virt_region region;
                    bool ret;

                    if (MVE_BASE_MEMORY_REGION_PROTECTED == rpc.params.mem_alloc.region)
                    {
                        mve_mem_virt_region_get(VIRT_MEM_REGION_PROTECTED, &region);
                    }
                    else
                    {
                        mve_mem_virt_region_get(VIRT_MEM_REGION_OUT_BUF, &region);
                    }

                    ret = mve_mmu_map_pages_and_reserve(session->mmu_ctx,
                                                        pages,
                                                        &region,
                                                        num_pages,
                                                        max_pages,
                                                        1 << alignment,
                                                        num_sys_pages,
                                                        ATTRIB_SHARED_RW,
                                                        ACCESS_READ_WRITE,
                                                        true);
                    if (false == ret)
                    {
                        MVE_RSRC_MEM_FREE_PAGES(pages, num_sys_pages);
                        pages = NULL;
                    }
                    else
                    {
                        /* Success! */
                        mve_addr = region.start;
                    }
                }

                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session,
                                      "RPC alloc requested. size=%d, num_pages=%d, addr=0x%X - 0x%X, alignment=%d.",
                                      size, num_pages, mve_addr, mve_addr + max_size, alignment);

                rpc.size = sizeof(uint32_t);
                rpc.params.data[0] = mve_addr;
                break;
            }
            case MVE_COM_RPC_FUNCTION_MEM_RESIZE:
            {
                uint32_t mve_addr = rpc.params.mem_resize.ve_pointer;
                uint32_t new_size = rpc.params.mem_resize.new_size;
                uint32_t num_pages = (new_size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;
                uint32_t ret_addr = 0;
                bool secure = mve_fw_secure(session->fw_inst);

                if (false != secure)
                {
                    struct mve_base_event_header *event;
                    uint32_t mapped_pages, max_pages;
                    bool ret = mve_mmu_map_info(session->mmu_ctx,
                                                mve_addr,
                                                &mapped_pages,
                                                &max_pages);
                    if (false == ret || num_pages > max_pages)
                    {
                        err = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                        goto resize_error;
                    }

                    /* Donot shrink the area for secure memory */
                    if (num_pages > mapped_pages)
                    {
                        struct mve_base_rpc_memory *rpc_memory;
                        uint32_t num_new_pages = num_pages - mapped_pages;

                        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "%s send event to userspace old: %d extra: %d",
                                              __FUNCTION__, mapped_pages * MVE_MMU_PAGE_SIZE, num_new_pages * MVE_MMU_PAGE_SIZE);

                        event = mve_queue_create_event(MVE_BASE_EVENT_RPC_MEM_RESIZE, sizeof(struct mve_base_rpc_memory));
                        if (NULL != event)
                        {
                            rpc_memory = (struct mve_base_rpc_memory *)event->data;
                            rpc_memory->region = VIRT_MEM_REGION_PROTECTED == mve_mem_virt_region_type_get(mve_addr) ?
                                                 MVE_BASE_MEMORY_REGION_PROTECTED : MVE_BASE_MEMORY_REGION_OUTBUF;
                            rpc_memory->size = num_new_pages * MVE_MMU_PAGE_SIZE;
                        }
                        mve_queue_add_event(session->mve_events, event);
                        MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
                        return;
                    }
                    ret_addr = mve_addr;
                }
                else
                {
                    bool ret = mve_mmu_map_resize(session->mmu_ctx,
                                                  NULL,
                                                  mve_addr,
                                                  num_pages,
                                                  ATTRIB_SHARED_RW,
                                                  ACCESS_READ_WRITE,
                                                  false);
                    if (true == ret)
                    {
                        ret_addr = mve_addr;
                    }
                }

resize_error:
                mver_scheduler_flush_tlb((mver_session_id)session);

                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session, "RPC resize requested. new_size=%d, num_pages=%d, mve_addr=0x%X, ret_addr: 0x%X.", new_size, num_pages, mve_addr, ret_addr);

                rpc.size = sizeof(uint32_t);
                rpc.params.data[0] = ret_addr;
                break;
            }
            case MVE_RPC_FUNCTION_MEM_FREE:
            {
                if (0 != rpc.params.mem_free.ve_pointer)
                {
                    mve_mmu_unmap_pages(session->mmu_ctx, rpc.params.mem_free.ve_pointer);
                }

                mver_scheduler_flush_tlb((mver_session_id)session);

                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session, "RPC free requested. addr=0x%X.", rpc.params.mem_free.ve_pointer);

                rpc.size = 0;
                break;
            }
            default:
            {
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_ERROR, session, "Unsupported RPC request. id=%u.", rpc.call_id);
                rpc.size = 0;
            }
        }

        switch (rpc.call_id)
        {
            case MVE_RPC_FUNCTION_DEBUG_PRINTF:
            case MVE_RPC_FUNCTION_MEM_ALLOC:
            case MVE_RPC_FUNCTION_MEM_RESIZE:
            case MVE_RPC_FUNCTION_MEM_FREE:
            {
                rpc.state = MVE_RPC_STATE_RETURN;
                break;
            }
            default:
            {
                rpc.state = MVE_RPC_STATE_FREE;
                break;
            }
        }

        mve_com_put_rpc_message(session, &rpc);
        session->rpc_in_progress = false;

        if (MVE_RPC_FUNCTION_DEBUG_PRINTF != rpc.call_id)
        {
            mver_irq_signal_mve((mver_session_id)session);
        }
    }
}

/**
 * Callback function that is invoked when the CPU receives an interrupt
 * from the MVE.
 * @param session_id Session identifier of the session mapped to the LSID
 *                   that generated the interrupt.
 */
void mve_session_irq_callback(mver_session_id session_id)
{
    struct mve_session *session;
    bool ret;

    session = (struct mve_session *)session_id;

    ret = acquire_session(session);
    if (false == ret)
    {
        return;
    }

    lock_session(session);

    session->state.irqs++;

    handle_rpc_request(session);
    handle_mve_message(session);

    /* Update the timestamp marking the most recent interaction with the HW */
//    do_gettimeofday(&session->state.timestamp);
    ktime_get_ts64(&session->state.timestamp);

    unlock_session(session);
    release_session(session);
}

#ifndef DISABLE_WATCHDOG
/**
 * This function is called when MVE failed to respond to the ping requests.
 * It informs userspace that the session has hung. The session
 * is also removed from the scheduler. Please note that the seession
 * must be validated by the caller.
 *
 * @param session    Pointer to session that has hung.
 */
static void mve_session_hung(struct mve_session *session)
{
    struct mve_base_event_header *event;
    struct list_head *pos;

    session->state.watchdog_state = WATCHDOG_TIMEOUT;

    /* Pass the event with the frame counter info to userspace */
    event = mve_queue_create_event(MVE_BASE_EVENT_SESSION_HUNG, 0);
    if (NULL == event)
    {
        return;
    }

    mve_queue_add_event(session->mve_events, event);
#ifdef EMULATOR
    {
        int t = (int)(((uintptr_t)session->filep) & 0xFFFFFFFF);
        write(t, &t, sizeof(int));
    }
#endif
    MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));

    /* Mark all input/output buffers as no longer in use by the MVE */
    list_for_each(pos, &session->external_buffers)
    {
        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

        ptr->in_use = 0;
    }

    /* Stop and remove the session from the scheduler */
    mver_scheduler_stop((mver_session_id)session);
    mver_scheduler_unregister_session((mver_session_id)session);
}

/**
 * Check if the session is active for ping command, which means that
 * the corresponding firmware is loaded in MVE and the session is
 * in either in STATE_ACTIVE_JOB or STATE_WAITING_FOR_RESPONSE
 * state.
 *
 * @param session    Pointer to session to be checked.
 * @return True if the session is active, else return false.
 */
static bool is_session_active(struct mve_session *session)
{
    bool ret = false;

    if (STATE_ACTIVE_JOB  == session->state.state ||
        STATE_WAITING_FOR_RESPONSE == session->state.state)
    {
        ret = true;
    }

    return ret;
}

/**
 * Ping the FW. The watchdog state gets set after the ping command has been
 * enqueued. The session must be validated by the caller.
 *
 * @param session    Pointer to session to be pinged.
 */
static void mve_session_ping(struct mve_session *session)
{
    enqueue_message(session, MVE_REQUEST_CODE_PING, 0, NULL);
    mver_irq_signal_mve((mver_session_id)session);

    session->state.watchdog_state = WATCHDOG_PINGED;
    session->state.ping_count++;
}

/**
 * Session watchdog task that checks the response from
 * previous ping and proceeds accordingly
 *
 * @param session    Pointer to watchdog session.
 * @param tv         Current time.
 */
static void session_watchdog(struct mve_session *session, struct timespec64 *tv)
{
    long dt;
    bool is_valid, res;

    is_valid = is_session_valid(session);
    if (true == is_valid)
    {
        kref_get(&session->refcount);
    }
    else
    {
        return;
    }

    lock_session(session);

    res = is_session_active(session);
    if (false != res)
    {
        /* Calculate the time delta between the current time and the last
         * HW interaction. */
        dt = tv->tv_sec * 1000 + tv->tv_nsec / 1000000 -
             (session->state.timestamp.tv_sec * 1000 + session->state.timestamp.tv_nsec / 1000000);
        if (dt > WATCHDOG_PING_INTERVAL)
        {
            if (MAX_NUM_PINGS > session->state.ping_count)
            {
                mve_session_ping(session);
            }
            else
            {
                mve_session_hung(session);
            }
        }
    }

    unlock_session(session);
    release_session(session);
}

/**
 * Watchdog task that invokes session watchdog.
 */
static void watchdog_task(void)
{
    struct list_head *pos, *next;
    struct timespec64 tv;

    //do_gettimeofday(&tv);
    ktime_get_ts64(&tv);

    list_for_each_safe(pos, next, &sessions)
    {
        struct mve_session *session = container_of(pos, struct mve_session, list);
        bool acquired;

        /* If acquire session failed, then the session has been removed. This also means
         * that the pointer to the next session in the session list can't be trusted, so
         * the loop is aborted and retried the next poll.
         */
        acquired = acquire_session(session);
        if (acquired == false)
        {
            break;
        }

        session_watchdog(session, &tv);

        release_session(session);
    }
}

/**
 * Watchdog thread that execute the watchdog task periodically.
 * @param data Not used but required by the kthread API.
 * @return Returns 0 when the thread exits.
 */
static int watchdog_thread_func(void *data)
{
    int ret;
    (void)data;

    do
    {
#if (1 == MVE_MEM_DBG_TRACKMEM)
#ifdef EMULATOR
        (void)down_interruptible(&watchdog_sem);
#endif
#endif
        watchdog_task();
#if (1 == MVE_MEM_DBG_TRACKMEM)
#ifdef EMULATOR
        up(&watchdog_sem);
#endif
#endif
        ret = down_timeout(&watchdog.killswitch, msecs_to_jiffies(WATCHDOG_PING_INTERVAL));
    }
    while (ret != 0 && !kthread_should_stop());

    return 0;
}

/**
 * Initialize the watchdog system.
 */
static void watchdog_init(void)
{
    atomic_set(&watchdog.refcount, 0);
}

/**
 * Create the watchdog task.
 */
static void watchdog_create_session(void)
{
    int cnt;

    cnt = atomic_add_return(1, &watchdog.refcount);
    if (1 == cnt)
    {
        /* Create the watchdog thread */
        char name[] = "mv500-watchdog";

        sema_init(&watchdog.killswitch, 0);
        watchdog.watchdog_thread = kthread_run(watchdog_thread_func, NULL, name);
    }
}

/**
 * Terminate the watchdog task.
 */
static void watchdog_destroy_session(void)
{
    int cnt;

    cnt = atomic_sub_return(1, &watchdog.refcount);
    if (0 == cnt)
    {
        up(&watchdog.killswitch);
        kthread_stop(watchdog.watchdog_thread);
    }
}
#endif /* #ifndef DISABLE_WATCHDOG */

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
#include <linux/device.h>

static ssize_t sysfs_print_sessions(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
    ssize_t num = 0;
    struct mve_session *session = NULL;
    struct list_head *pos;
    int i = 0;

    num += snprintf(buf, PAGE_SIZE, "Sessions:\n");
    list_for_each(pos, &sessions)
    {
        uint32_t num_data;

        session = container_of(pos, struct mve_session, list);

        num += snprintf(buf + num, PAGE_SIZE - num, "%d: %p\n", i, session);
        num += snprintf(buf + num, PAGE_SIZE - num, "    role: %s\n", session->role);

        num_data = session->mve_events->wpos - session->mve_events->rpos;
        num += snprintf(buf + num, PAGE_SIZE - num, "    regular events: %d\n",
                        num_data);

        /* Print event codes */
        if (0 < num_data)
        {
            uint32_t p;

            num += snprintf(buf + num, PAGE_SIZE - num, "        ");
            p = session->mve_events->rpos;
            while (0 < session->mve_events->wpos - p)
            {
                uint32_t message_size = session->mve_events->buffer[p % session->mve_events->buffer_size] >> 16;
                num += snprintf(buf + num, PAGE_SIZE - num, "(%d, %d) ",
                                session->mve_events->buffer[p % session->mve_events->buffer_size] & 0xFFFF,
                                message_size);
                p += 1 + (message_size + 3) / 4;
            }
            num += snprintf(buf + num, PAGE_SIZE - num, "\n");
        }

        num += snprintf(buf + num, PAGE_SIZE - num, "    query events: %d\n",
                        session->response_events->wpos - session->response_events->rpos);

        num += snprintf(buf + num, PAGE_SIZE - num, "    state: %d\n", session->state.state);
        num += snprintf(buf + num, PAGE_SIZE - num, "    HW state: %d\n", session->state.hw_state);

        /* Print statistics on registered buffers */
        num += snprintf(buf + num, PAGE_SIZE - num, "    Buffers\n");
        {
            struct list_head *b;

            list_for_each(b, &session->external_buffers)
            {
                struct mve_buffer_client *ptr = container_of(b, struct mve_buffer_client, register_list);

                num += snprintf(buf + num, PAGE_SIZE - num, "\t\t%s: %llu va:0x%X - 0x%X owner: %s\n", ptr->port_index == 0 ? "IN" : "OUT",
                                ptr->buffer->info.handle, ptr->buffer->mapping.region.start, ptr->buffer->mapping.region.end,
                                ptr->in_use > 0 ? "MVE" : "HOST");
            }
        }
        i++;
    }

    return num;
}

static ssize_t sysfs_dump_fw_status(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
    ssize_t num = 0;
    struct mve_session *session = NULL;
    struct list_head *pos;

    num += snprintf(buf, PAGE_SIZE, "Requesting FW status dump\n");
    list_for_each(pos, &sessions)
    {
        session = container_of(pos, struct mve_session, list);

        /* Dump FW status for the currently running session */
        if (STATE_WAITING_FOR_RESPONSE == session->state.state ||
            STATE_ACTIVE_JOB == session->state.state)
        {
            enqueue_message(session, MVE_REQUEST_CODE_DUMP, 0, NULL);
            mver_irq_signal_mve((mver_session_id)session);
        }
    }

    return num;
}

#if (1 == DVFS_DEBUG_MODE)
static ssize_t sysfs_print_dvfs_b_enable(struct device *dev,
                                         struct device_attribute *attr,
                                         char *buf)
{
    ssize_t num = 0;
    num += snprintf(buf, PAGE_SIZE, "%u", atomic_read(&dvfs_b_enable) ? 1 : 0);
    return num;
}

static ssize_t sysfs_set_dvfs_b_enable(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf,
                                       size_t count)
{
    int failed;
    int enable;
    failed = kstrtouint(buf, 10, &enable);
    if (!failed)
    {
        atomic_set(&dvfs_b_enable, enable);
    }
    return (failed) ? failed : count;
}
#endif /* DVFS_DEBUG_MODE */

static struct device_attribute sysfs_files[] =
{
    __ATTR(sessions,  S_IRUGO, sysfs_print_sessions, NULL),
    __ATTR(dump_fw_status, S_IRUGO, sysfs_dump_fw_status,      NULL),
#if (1 == DVFS_DEBUG_MODE)
    __ATTR(dvfs_b_adjust, (S_IRUGO | S_IWUSR), sysfs_print_dvfs_b_enable, sysfs_set_dvfs_b_enable),
#endif
};

#endif /* CONFIG_SYSFS && _DEBUG */

static void session_destroy_instance(struct mve_session *session)
{
    struct list_head *pos, *next;
    int sem_taken;

    mver_scheduler_stop((mver_session_id)session);
    mver_scheduler_unregister_session((mver_session_id)session);

    list_for_each_safe(pos, next, &session->queued_input_buffers)
    {
        struct session_enqueue_request *request = container_of(pos, struct session_enqueue_request, list);
        list_del(&request->list);
        MVE_RSRC_MEM_FREE(request);
    }

    list_for_each_safe(pos, next, &session->queued_output_buffers)
    {
        struct session_enqueue_request *request = container_of(pos, struct session_enqueue_request, list);
        list_del(&request->list);
        MVE_RSRC_MEM_FREE(request);
    }

    mve_com_delete(session);

    mver_pm_request_suspend();

#ifndef DISABLE_WATCHDOG
    watchdog_destroy_session();
#endif

    num_sessions--;

    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session, "Session destroyed.");

    mver_scheduler_lock();
    sem_taken = down_interruptible(&sessions_sem);

    kref_put(&session->refcount, free_session);

    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }
    mver_scheduler_unlock();
}

/**
 * Callback function executed by the scheduler when it wants to know whether
 * the session has any work to perform or not.
 *
 * @param session_id ID of the session to query the scheduling state.
 * @return The session's scheduling state.
 */
static enum SCHEDULE_STATE mve_session_has_work_callback(mver_session_id session_id)
{
    struct mve_session *session;
    enum SCHEDULE_STATE state = SCHEDULE_STATE_BUSY;
    bool ret;

    session = (struct mve_session *)session_id;

    ret = acquire_session(session);
    if (false == ret)
    {
        return SCHEDULE_STATE_SLEEP;
    }

    if (STATE_SWITCHED_OUT == session->state.state)
    {
        /* Session just got switched out. Find out if it needs to be rescheduled */
        if (session->pending_response_count != 0 ||
            false != session->state.request_switchin ||
            session->state.hw_state != session->state.pending_hw_state ||
            (IDLE_STATE_IDLE != session->state.idle_state && 0 != session->state.job_frame_size))
        {
            state = SCHEDULE_STATE_RESCHEDULE;
        }
        /* Session has received two idle messages and no buffers or messages have been added. Do not
         * reschedule! */
        else if (IDLE_STATE_IDLE == session->state.idle_state)
        {
            state = SCHEDULE_STATE_SLEEP;
        }
    }
    else
    {
        bool has_work = fw_has_work(session);
        if (false != session->state.request_switchout)
        {
            state = SCHEDULE_STATE_REQUEST_SWITCHOUT;
        }
        else if (false != has_work)
        {
            state = SCHEDULE_STATE_BUSY;
        }
        else
        {
            state = SCHEDULE_STATE_IDLE;
        }
    }

    release_session(session);

    return state;
}

/**
 * Callback function executed by the scheduler when the session should be
 * switched out.
 *
 * @param session_id       ID of the session to switch out.
 * @param require_idleness True if switchout should occur only if the session is idle.
 */
static void mve_session_switchout_callback(mver_session_id session_id, bool require_idleness)
{
    struct mve_session *session;
    bool ret;

    session = (struct mve_session *)session_id;

    ret = acquire_session(session);
    if (false == ret)
    {
        return;
    }

    lock_session(session);
    /* The idle status can change between the scheduler calling
     * mve_session_has_work_callback and this function because the session lock
     * is not kept during that time. The if statement below is used to protect
     * the session from being switched out while it actually has work to do. */
    if (false == require_idleness ||
        (false != require_idleness &&
         IDLE_STATE_IDLE == session->state.idle_state))
    {
        switchout_session(session);
    }
    unlock_session(session);
    release_session(session);
}

/**
 * Callback function invoked by the scheduler when the session is being
 * switched in.
 *
 * @param session_id ID of the session to switch in.
 */
static void mve_session_switchin_callback(mver_session_id session_id)
{
    struct mve_session *session = (struct mve_session *)session_id;
    bool ret;
    mve_base_error err;

    ret = acquire_session(session);
    if (false == ret)
    {
        return;
    }

    lock_session(session);

    /* Reset idleness */
    session->state.idle_state = IDLE_STATE_ACTIVE;
    session->state.request_switchin = false;
    session->state.idle_dirty = false;

    err = enqueue_job(session);
    if (MVE_BASE_ERROR_NONE != err)
    {
        goto out;
    }

    /* If session has a pending state change, then add stop/go message. It is important
     * that this message is appended after the job message. */
    if (session->state.pending_hw_state != MVE_BASE_HW_STATE_PENDING &&
        session->state.hw_state != session->state.pending_hw_state)
    {
        uint32_t code = (session->state.pending_hw_state == MVE_BASE_HW_STATE_STOPPED) ? MVE_REQUEST_CODE_STOP : MVE_REQUEST_CODE_GO;

        err = enqueue_message(session, code, 0, NULL);
        if (MVE_BASE_ERROR_NONE != err)
        {
            goto out;
        }
        session->state.pending_hw_state = MVE_BASE_HW_STATE_PENDING;
    }

    if (STATE_SWITCHED_OUT == session->state.state)
    {
        /* Switching in session */
        ret = change_session_state(session, STATE_SWITCHING_IN);
        if (false != ret)
        {
            session->state.irqs = 0;
        }
    }
    else
    {
        /* Switching in a session that is in the wrong state */
        /* VIDDK-752: Remove this print */
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                              MVE_LOG_ERROR,
                              session,
                              "Attempting to switch in a session that is not switched out. Current state %d",
                              session->state.state);
        WARN_ON(true);
    }

    mver_irq_signal_mve((mver_session_id)session_id);

out:
    unlock_session(session);
    release_session(session);
}

/**
 * Callback function executed by the scheduler when the session has been switched
 * out completely. This session will not execute until mve_session_switchin_callback
 * has been called.
 * @param session_id ID of the session that has been switched out.
 */
static void mve_session_switchout_completed_callback(mver_session_id session_id)
{
    struct mve_session *session = (struct mve_session *)session_id;

    bool ret;

    /* Take care of pending IRQs not handled by the IRQ handler automatically */
    mve_session_irq_callback(session_id);

    ret = acquire_session(session);
    if (false == ret)
    {
        return;
    }

    lock_session(session);
    ret = change_session_state(session, STATE_SWITCHED_OUT);
    unlock_session(session);
    release_session(session);
}

/**
 * Callback function executed by the scheduler to retrieve the number of restricting
 * buffers enqueued to the FW. Restricting buffers are output buffers for decoders
 * and input buffers for encoders.
 *
 * Restricted buffers count is adjusted by amount of buffers which were processed by
 * the FW but not yet released. Because these two values updated at different moment
 * of time, potentially it could happen that return value would be negative. The
 * caller should ignore this value then and treat the call as failed one.
 *
 * @param session_id ID of the session
 * @return positive or zero amount of restricting buffers in case of success,
 *         negative value in case of failure
 */
static int mve_session_get_restricting_buffer_count_callback(mver_session_id session_id)
{
    struct mve_session *session = (struct mve_session *)session_id;
    int buffers_cnt;
    bool acquired = acquire_session(session);
    if (false == acquired)
    {
        return -1;
    }

    /* Don't have to lock the session since we just want to get the number of buffers */
    if (MVE_SESSION_TYPE_DECODER == session->session_type)
    {
        buffers_cnt = session->output_buffer_count;
#if (1 == DVFS_DEBUG_MODE)
        if (1 == atomic_read(&dvfs_b_enable))
#endif
        {
            buffers_cnt -= session->buffer_on_hold_count;
        }
    }
    else
    {
        buffers_cnt = session->input_buffer_count;
        if (false != session->state.eos_queued && buffers_cnt < 2)
        {
            buffers_cnt = 2;
        }
    }

    if (false != session->keep_freq_high)
    {
        buffers_cnt += 2;
    }

    release_session(session);

    return buffers_cnt;
}

/*****************************************************************************
 * External interface
 *****************************************************************************/
#if defined(_DEBUG) && !defined(EMULATOR)
static int fw_trace_open(struct inode *inode, struct file *file)
{
#define TRACE_SIZE 128 * 1024
    char *str;
    struct list_head *pos, *next;
    int num = 0;

    str = MVE_RSRC_MEM_VALLOC(TRACE_SIZE);
    if (NULL == str)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_session, MVE_LOG_ERROR, "Unable to allocate memory for FW trace buffer.");
        return -EBUSY;
    }
    memset(str, 0, TRACE_SIZE);

    list_for_each_safe(pos, next, &sessions)
    {
        struct mve_session *session = container_of(pos, struct mve_session, list);
        int buffer_index = 0;
        int core;

        struct mve_com_trace_buffers *fw_trace = &session->fw_trace;

        num += snprintf(str + num, TRACE_SIZE - num, "Session %p (%s):\n", session, session->role);

        for (core = 0; core < fw_trace->num_cores; core++)
        {
            int rasc;

            for (rasc = 0; rasc < 8 * sizeof(fw_trace->rasc_mask); rasc++)
            {
                if ((1 << rasc) & fw_trace->rasc_mask)
                {
                    uint32_t rasc_addr = fw_trace->buffers[buffer_index].rasc_addr;

                    if (0 != rasc_addr)
                    {
                        uint32_t size = fw_trace->buffers[buffer_index].size;
                        uint32_t *buffer = (uint32_t *)MVE_RSRC_MEM_VALLOC(size);

                        if (NULL != buffer)
                        {
                            uint32_t i;

                            memset(buffer, 0, size);
                            mve_mmu_read_buffer(session->mmu_ctx, rasc_addr, (uint8_t *)buffer, size);

                            /* convert size to be in words from now on */
                            size >>= 2;
                            for (i = 0; i < size; )
                            {
                                uint32_t event = buffer[(i + 0) & (size - 1)] & 0xffff;
                                uint32_t param0 = (buffer[(i + 0) & (size - 1)] >> 16) & 0xff;
                                uint32_t nparams = buffer[(i + 0) & (size - 1)] >> 28;
                                uint32_t timestamp = buffer[(i + 1) & (size - 1)];

                                if (0 != event)
                                {
                                    int j;

                                    num += snprintf(str + num, TRACE_SIZE - num, "%d %d %08x %04x %02x",
                                                    core, rasc, timestamp, event, param0);

                                    for (j = 0; j < ((int)nparams) - 1; j++)
                                    {
                                        uint32_t param = buffer[(i + 2 + j) & (size - 1)];
                                        num += snprintf(str + num, TRACE_SIZE - num, " %08x", param);
                                    }
                                    num += snprintf(str + num, TRACE_SIZE - num, "\n");
                                }
                                i += (2 + nparams - 1);
                            }
                            MVE_RSRC_MEM_VFREE(buffer);
                        }
                    }

                    buffer_index++;
                }
            }
        }
        num += snprintf(str + num, TRACE_SIZE - num, "\n\n");
    }

    file->private_data = (void *)str;

    return 0;
#undef TRACE_SIZE
}

static int fw_trace_close(struct inode *inode, struct file *file)
{
    MVE_RSRC_MEM_VFREE(file->private_data);
    return 0;
}

static ssize_t fw_trace_read(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    char *str = (char *)file->private_data;

    return simple_read_from_buffer(user_buffer, count, position, str, strlen(str));
}
#endif

void mve_session_init(struct device *dev)
{
    sema_init(&sessions_sem, 1);
    num_sessions = 0;

#ifndef DISABLE_WATCHDOG
    /* Initialize watchdog thread */
    watchdog_init();
#endif

#if defined(_DEBUG) && !defined(EMULATOR)
    {
        static const struct file_operations trace_fops =
        {
            .open = fw_trace_open,
            .read = fw_trace_read,
            .release = fw_trace_close
        };
        struct dentry *dentry, *parent_dentry;

        parent_dentry = mve_rsrc_log_get_parent_dir();
        dentry = debugfs_create_file("fw_trace", 0400, parent_dentry, NULL, &trace_fops);
        if (IS_ERR_OR_NULL(dentry))
        {
            MVE_LOG_PRINT(&mve_rsrc_log_session, MVE_LOG_ERROR, "Unable to create debugfs file 'fw_trace'.");
        }
    }

#if defined(CONFIG_SYSFS)
    {
        int i;

        for (i = 0; i < NELEMS(sysfs_files); ++i)
        {
            int err = device_create_file(dev, &sysfs_files[i]);
            if (err < 0)
            {
                MVE_LOG_PRINT(&mve_rsrc_log_session, MVE_LOG_ERROR, "Unable to create sysfs file.");
            }
        }
    }
#endif /* CONFIG_SYSFS */
#endif /* _DEBUG && !EMULATOR*/
}

void mve_session_deinit(struct device *dev)
{
#ifndef EMULATOR
#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    int i;
    for (i = 0; i < NELEMS(sysfs_files); ++i)
    {
        device_remove_file(dev, &sysfs_files[i]);
    }
#endif
#endif

    mve_session_destroy_all_sessions();
}

void mve_session_destroy_all_sessions(void)
{
    struct list_head *pos, *next;

    list_for_each_safe(pos, next, &sessions)
    {
        struct mve_session *ptr = container_of(pos, struct mve_session, list);

        list_del(&ptr->list);
        session_destroy_instance(ptr);
    }
}

struct mve_session *mve_session_create(struct file *filep)
{
    struct mve_session *session;
    struct mve_mem_virt_region region;
    bool ret;
    int sem_taken;
    phys_addr_t *pages;

    session = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_session), GFP_KERNEL);
    if (NULL == session)
    {
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                              MVE_LOG_ERROR,
                              session,
                              "Failed to allocate memory to create a new session.");
        return NULL;
    }

    session->mmu_ctx = mve_mmu_create_ctx();
    if (NULL == session->mmu_ctx)
    {
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                              MVE_LOG_ERROR,
                              session,
                              "Failed to create a MMU context struct.");
        MVE_RSRC_MEM_FREE(session);
        return NULL;
    }

    session->filep = filep;
    session->fw_loaded = false;
    session->keep_freq_high = true;

    session->state.state              = STATE_SWITCHED_OUT;
    session->state.hw_state           = MVE_BASE_HW_STATE_STOPPED;
    session->state.pending_hw_state   = session->state.hw_state;
    session->state.watchdog_state     = WATCHDOG_IDLE;
    session->state.ping_count         = 0;
    session->state.flush_state        = 0;
    session->state.flush_target       = 0;
    session->state.idle_state         = IDLE_STATE_ACTIVE;
    session->state.request_switchin   = false;
    session->state.request_switchout  = false;
    session->state.job_frame_size     = 0;

    //do_gettimeofday(&session->state.timestamp);
    ktime_get_ts64(&session->state.timestamp);

    session->role = MVE_RSRC_MEM_ZALLOC(sizeof(char) * MAX_STRINGNAME_SIZE, GFP_KERNEL);
    if (NULL == session->role)
    {
        goto error;
    }

    session->prot_ver = MVE_BASE_FW_MAJOR_PROT_UNKNOWN;
    session->rpc_in_progress = false;

    /* Alloc and map pages for the message and buffer queues */
    session->msg_in_queue   = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->msg_out_queue  = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->buf_input_in   = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->buf_input_out  = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->buf_output_in  = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->buf_output_out = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    session->rpc_area       = mve_rsrc_dma_mem_alloc(MVE_MMU_PAGE_SIZE, DMA_MEM_TYPE_UNCACHED);
    if (NULL == session->msg_in_queue ||
        NULL == session->msg_out_queue ||
        NULL == session->buf_input_in ||
        NULL == session->buf_input_out ||
        NULL == session->buf_output_in ||
        NULL == session->buf_output_out ||
        NULL == session->rpc_area)
    {
        goto error;
    }

    /* Map message in queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_IN_QUEUE, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->msg_in_queue);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map message out queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_OUT_QUEUE, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->msg_out_queue);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map input buffer in queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_IN, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->buf_input_in);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map input buffer out queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_OUT, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->buf_input_out);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map output buffer in queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_IN, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->buf_output_in);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map output buffer out queue */
    mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_OUT, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->buf_output_out);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }
    /* Map RPC area */
    mve_mem_virt_region_get(VIRT_MEM_REGION_RPC_QUEUE, &region);
    pages = mve_rsrc_dma_mem_get_pages(session->rpc_area);
    ret = mve_mmu_map_pages(session->mmu_ctx, pages,
                            region.start, 1, 1, ATTRIB_SHARED_RW, ACCESS_READ_WRITE, false);
    if (false == ret)
    {
        goto error;
    }

    session->mve_events = mve_queue_create(16384);
    session->response_events = mve_queue_create(8192);
    if (NULL == session->mve_events || NULL == session->response_events)
    {
        goto error;
    }

    session->output_buffer_count          = 0;
    session->input_buffer_count           = 0;
    session->pending_response_count       = 0;
    session->buffer_on_hold_count         = 0;
    session->pending_buffer_on_hold_count = 0;
    session->state.eos_queued             = false;

    INIT_LIST_HEAD(&session->queued_input_buffers);
    INIT_LIST_HEAD(&session->queued_output_buffers);

    INIT_LIST_HEAD(&session->external_buffers);
    INIT_LIST_HEAD(&session->quick_flush_buffers);
    session->next_mve_handle = 1;

    INIT_LIST_HEAD(&session->list);
    sema_init(&session->semaphore, 1);

    kref_init(&session->refcount);

    /* Add session to the list of sessions */
    sem_taken = down_interruptible(&sessions_sem);
    list_add(&session->list, &sessions);

    num_sessions++;

    if (2 == num_sessions)
    {
        /* Creating this session changed from single to multi session. If
         * the other session is running an infinite job, switch it out
         * and reschedule using finite jobs */
        struct list_head *pos;

        list_for_each(pos, &sessions)
        {
            struct mve_session *ptr = container_of(pos, struct mve_session, list);

            if (session != ptr)
            {
                /* We have found the session to switch out. Add a reference
                 * to the session and release the sessions_sem */
                kref_get(&ptr->refcount);
                if (0 == sem_taken)
                {
                    up(&sessions_sem);
                    sem_taken = -1;
                }

                lock_session(ptr);
                /* Infinite job. Set the switchin flag and switch it out! */
                ptr->state.request_switchin = true;
                switchout_session(ptr);
                unlock_session(ptr);
                release_session(ptr);
                break;
            }
        }
    }

    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }

    mver_pm_request_resume();

#ifndef DISABLE_WATCHDOG
    watchdog_create_session();
#endif

    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_INFO, session, "Session created.");

    return session;

error:
    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                          MVE_LOG_ERROR,
                          session,
                          "An error occurred during session creation.");

    mve_queue_destroy(session->response_events);
    mve_queue_destroy(session->mve_events);

    if (NULL != session->msg_out_queue)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_OUT_QUEUE, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->msg_out_queue);
    }
    if (NULL != session->msg_in_queue)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_MSG_IN_QUEUE, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->msg_in_queue);
    }
    if (NULL != session->buf_input_in)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_IN, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->buf_input_in);
    }
    if (NULL != session->buf_input_out)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_INPUT_BUFFER_OUT, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->buf_input_out);
    }
    if (NULL != session->buf_output_in)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_IN, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->buf_output_in);
    }
    if (NULL != session->buf_output_out)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_OUTPUT_BUFFER_OUT, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->buf_output_out);
    }
    if (NULL != session->rpc_area)
    {
        mve_mem_virt_region_get(VIRT_MEM_REGION_RPC_QUEUE, &region);
        mve_mmu_unmap_pages(session->mmu_ctx, region.start);
        mve_rsrc_dma_mem_free(session->rpc_area);
    }

    if (NULL != session->role)
    {
        MVE_RSRC_MEM_FREE(session->role);
    }
    if (NULL != session->mmu_ctx)
    {
        mve_mmu_destroy_ctx(session->mmu_ctx);
    }

    MVE_RSRC_MEM_FREE(session);

    return NULL;
}

void mve_session_destroy(struct mve_session *session)
{
    bool ret;
    int sem_taken;

    sem_taken = down_interruptible(&sessions_sem);
    ret = is_session_valid(session);
    if (false == ret)
    {
        if (0 == sem_taken)
        {
            up(&sessions_sem);
        }
        return;
    }

    /* Remove the session from the sessions list. This marks the session as
     * invalid. */
    list_del(&session->list);
    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }

    session_destroy_instance(session);
}

struct mve_session *mve_session_get_by_file(struct file *filep)
{
    struct list_head *pos, *next;
    struct mve_session *session = NULL;
    int sem_taken;

    sem_taken = down_interruptible(&sessions_sem);

    list_for_each_safe(pos, next, &sessions)
    {
        struct mve_session *ptr = container_of(pos, struct mve_session, list);

        if (filep == ptr->filep)
        {
            session = ptr;
            break;
        }
    }

    if (0 == sem_taken)
    {
        up(&sessions_sem);
    }

    return session;
}

void mve_session_cleanup_client(struct file *filep)
{
    struct mve_session *session;

    session = mve_session_get_by_file(filep);
    if (NULL != session)
    {
        int sem_taken = down_interruptible(&sessions_sem);
        list_del(&session->list);
        if (0 == sem_taken)
        {
            up(&sessions_sem);
        }

        session_destroy_instance(session);
    }
}

void mve_session_destroy_all(void)
{
    struct list_head *pos, *next;

    list_for_each_safe(pos, next, &sessions)
    {
        struct mve_session *ptr = container_of(pos, struct mve_session, list);

        int sem_taken = down_interruptible(&sessions_sem);
        list_del(&ptr->list);
        if (0 == sem_taken)
        {
            up(&sessions_sem);
        }

        session_destroy_instance(ptr);
    }
}

void mve_session_set_role(struct mve_session *session, char *role)
{
    bool ret;

    if (NULL == role)
    {
        return;
    }

    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                          MVE_LOG_INFO,
                          session,
                          "Setting session role to %s.",
                          role);

    ret = acquire_session(session);
    if (false == ret)
    {
        return;
    }

    if (true == session->fw_loaded)
    {
        WARN_ON(true == session->fw_loaded);
    }
    else
    {
        void *ptr;

        strncpy(session->role, role, MAX_STRINGNAME_SIZE);
        /* Make sure the string is NULL terminated */
        session->role[MAX_STRINGNAME_SIZE - 1] = '\0';

        ptr = strstr(session->role, "decoder");
        if (NULL != ptr)
        {
            session->session_type = MVE_SESSION_TYPE_DECODER;
        }
        else
        {
            session->session_type = MVE_SESSION_TYPE_ENCODER;
        }
    }

    release_session(session);
}

bool mve_session_activate(struct mve_session *session, uint32_t *version,
                          struct mve_base_fw_secure_descriptor *fw_secure_desc)
{
    phys_addr_t l1_page;
    bool ret;
    int ncores;

    ret = acquire_session(session);
    if (false == ret)
    {
        return false;
    }

    ncores = mver_scheduler_get_ncores();

    /* generate secure firmware instance with pre-created l2pages; or
     * just load regular firmware if secure l2pages are not available */
    session->fw_inst = mve_fw_load(session->mmu_ctx, fw_secure_desc, session->role, ncores);
    if (NULL != session->fw_inst)
    {
        mve_base_error error;
        struct mve_base_fw_version *ver;

        session->fw_loaded = true;
        ver = mve_fw_get_version(session->fw_inst);
        if (NULL != ver)
        {
            session->prot_ver = ver->major;
            if (NULL != version)
            {
                *version = ver->major << 8 | ver->minor;
            }
        }
        else
        {
            session->prot_ver = MVE_BASE_FW_MAJOR_PROT_UNKNOWN;
        }
        error = mve_com_set_interface_version(session, session->prot_ver);
        mve_fw_log_fw_binary(session->fw_inst, session);

        if (error == MVE_BASE_ERROR_NONE)
        {
            l1_page = mve_mmu_get_id(session->mmu_ctx);
            mver_scheduler_register_session((mver_session_id)session,
                                            mve_mmu_make_l0_entry(ATTRIB_PRIVATE, l1_page),
                                            ncores,
                                            mve_fw_secure(session->fw_inst),
                                            mve_session_irq_callback,
                                            mve_session_has_work_callback,
                                            mve_session_switchout_callback,
                                            mve_session_switchin_callback,
                                            mve_session_switchout_completed_callback,
                                            mve_session_get_restricting_buffer_count_callback);

            ret = true;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        ret = false;
    }

    release_session(session);
    return ret;
}

mve_base_error mve_session_enqueue_flush_buffers(struct mve_session *session, uint32_t flush)
{
    bool res;
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    mver_scheduler_lock();
    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    lock_session(session);

    if (WATCHDOG_TIMEOUT == session->state.watchdog_state)
    {
        ret = MVE_BASE_ERROR_HARDWARE;
        goto out;
    }

    if (0 == (flush & MVE_BASE_FLUSH_ALL_PORTS))
    {
        ret = MVE_BASE_ERROR_BAD_PARAMETER;
        goto out;
    }

    if (0 != (flush & MVE_BASE_FLUSH_INPUT_PORT))
    {
        ret = enqueue_message(session, MVE_REQUEST_CODE_INPUT_FLUSH, 0, NULL);
        mver_irq_signal_mve((mver_session_id)session);
        return_all_pending_buffers_to_userspace(session, &session->queued_input_buffers, true);
    }

    if (0 != (flush & MVE_BASE_FLUSH_OUTPUT_PORT))
    {
        ret |= enqueue_message(session, MVE_REQUEST_CODE_OUTPUT_FLUSH, 0, NULL);
        mver_irq_signal_mve((mver_session_id)session);
        return_all_pending_buffers_to_userspace(session, &session->queued_output_buffers, false);
    }

    if (MVE_BASE_ERROR_NONE == ret)
    {
        session->state.flush_state = 0;
        session->state.flush_target = flush;
    }

    if (0 != (flush & MVE_BASE_FLUSH_QUICK))
    {
        session->state.idle_dirty = true;
    }

out:
    unlock_session(session);
    mver_scheduler_unlock();
    if (MVE_BASE_ERROR_NONE == ret)
    {
        schedule_session(session);
    }
    release_session(session);

    return ret;
}

mve_base_error mve_session_enqueue_state_change(struct mve_session *session, enum mve_base_hw_state state)
{
    bool res;
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    if (MVE_BASE_HW_STATE_STOPPED != state &&
        MVE_BASE_HW_STATE_RUNNING != state)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    lock_session(session);

    if (WATCHDOG_TIMEOUT == session->state.watchdog_state)
    {
        ret = MVE_BASE_ERROR_HARDWARE;
        goto out;
    }

    if (session->state.pending_hw_state == MVE_BASE_HW_STATE_PENDING)
    {
        ret = MVE_BASE_ERROR_HARDWARE;
        goto out;
    }

    /* Remember requested state. The go/stop command can only be safely added to the message
     * queue just before the session is switched in, in order to guarantee that the go/stop
     * command is preceded by a job command. */
    session->state.pending_hw_state = state;

#if SCHEDULER_MODE_IDLE_SWITCHOUT != 1
    /* The state change implementation always defers state changes to immediately
     * after a switch in. This is normally not a problem since the FW will report
     * idleness which causes the session to switch out and because of the pending
     * state change, the session will be switched in again. The problem is when
     * idle switchout has been disabled. In this case, the driver will not switch
     * out idle sessions and the pending state change will never be handled. If
     * idle switchout is disabled, the driver must force switchout of the session
     * here. */
    switchout_session(session);
#endif

    /* Unlock the session before any calls to the scheduler, to avoid deadlocks if the scheduler
     * calls one of the session callbacks. */
    unlock_session(session);
    schedule_session(session);

    release_session(session);

    return ret;

out:
    unlock_session(session);
    release_session(session);

    return ret;
}

struct mve_base_event_header *mve_session_get_event(struct mve_session *session, uint32_t timeout)
{
    struct mve_base_event_header *event = NULL;
    bool ret;

    ret = acquire_session(session);
    if (false == ret)
    {
        return NULL;
    }

    if (true == session->fw_loaded)
    {
        event = mve_queue_wait_for_event(session->mve_events, timeout);
    }

    release_session(session);

    return event;
}

mve_base_error mve_session_set_paramconfig(struct mve_session *session,
                                           uint32_t size,
                                           void *data,
                                           uint32_t *firmware_error,
                                           enum mve_base_command_type entity)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;
    bool res;
    int msg;
    int response_count;

    if (NULL == data)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    if (WATCHDOG_TIMEOUT == session->state.watchdog_state)
    {
        ret = MVE_BASE_ERROR_HARDWARE;
        release_session(session);
        return ret;
    }

    res = schedule_session(session);
    if (false == res)
    {
        ret = MVE_BASE_ERROR_NOT_READY;
        release_session(session);
        return ret;
    }

    lock_session(session);

    if (STATE_ACTIVE_JOB != session->state.state &&
        STATE_WAITING_FOR_RESPONSE != session->state.state &&
        STATE_SWITCHING_IN != session->state.state)
    {
        ret = MVE_BASE_ERROR_NOT_READY;
        goto out;
    }

    if (MVE_BASE_SET_OPTION == entity)
    {
        msg = MVE_REQUEST_CODE_SET_OPTION;
    }
    else
    {
        msg = MVE_BUFFER_CODE_PARAM;
    }

    change_session_state(session, STATE_WAITING_FOR_RESPONSE);

    /* Remember the current number of messages we are waiting for. */
    response_count = session->pending_response_count;

    /* Enqueue message to firmware and send interrupt. */
    ret = enqueue_message(session, msg, size, data);
    mver_irq_signal_mve((mver_session_id)session);

    /* Calculate the number of confirms that should be received. */
    response_count = session->pending_response_count - response_count;
    if (MVE_BASE_ERROR_NONE == ret)
    {
        uint32_t set_error;

        struct mve_base_event_header *event = NULL;

        /* Wait for the response message. First unlock the session semaphore or
         * the interrupt handler will not be able to process the incoming message.
         * The timeout is set to 10 seconds because once a message is placed
         * in the message queue, it's not possible to remove it. If we don't
         * wait until the message has been received, the next get/set parameter/config
         * call will receive it and regard that message as the response to the one
         * it just put into the message queue. */
        while (response_count-- > 0)
        {
            unlock_session(session);
            event = mve_queue_wait_for_event(session->response_events, 10000);
            lock_session(session);

            if (NULL == event)
            {
                if (false != mve_queue_interrupted(session->response_events))
                {
                    ret = MVE_BASE_ERROR_HARDWARE;
                }
                else
                {
                    ret = MVE_BASE_ERROR_NOT_READY;
                }
                change_session_state(session, STATE_ACTIVE_JOB);
                goto out;
            }

            if (MVE_BASE_EVENT_SET_PARAMCONFIG != event->code ||
                sizeof(uint32_t) != event->size)
            {
                ret = MVE_BASE_ERROR_HARDWARE;
                MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
                goto out;
            }

            /* Extract error code */
            set_error = *((uint32_t *)event->data);
            if (set_error != 0)
            {
                *firmware_error = set_error;
                ret = MVE_BASE_ERROR_FIRMWARE;
            }

            MVE_RSRC_MEM_CACHE_FREE(event, EVENT_SIZE(event->size));
        }

        /* Flag response as received. Some set parameters in the v2 host interface
         * don't result in any response. Revert back to the old state in that case. */
        if (STATE_WAITING_FOR_RESPONSE == session->state.state)
        {
            response_received(session);
        }
    }
    else
    {
        if (response_count == 0)
        {
            *firmware_error = ret;
            ret = MVE_BASE_ERROR_FIRMWARE;
        }
        change_session_state(session, STATE_ACTIVE_JOB);
    }

out:
    unlock_session(session);
    release_session(session);
    return ret;
}

mve_base_error mve_session_buffer_register(struct mve_session *session,
                                           uint32_t port_index,
                                           struct mve_base_buffer_userspace *descriptor)
{
    mve_base_error ret;
    bool res;

    if (NULL == descriptor)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    if (MVE_BASE_BUFFER_FORMAT_YUV420_AFBC == descriptor->format ||
        MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B == descriptor->format)
    {
        uint32_t fuse = mver_reg_get_fuse();
        if (fuse & (1 << CORESCHED_FUSE_DISABLE_AFBC))
        {
            return MVE_BASE_ERROR_NOT_IMPLEMENTED;
        }
    }

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    mver_scheduler_lock();
    lock_session(session);
    ret = mve_session_buffer_register_internal(session, port_index, descriptor);
    unlock_session(session);
    mver_scheduler_unlock();
    release_session(session);

    return ret;
}

mve_base_error mve_session_buffer_unregister(struct mve_session *session,
                                             mve_base_buffer_handle_t buffer_id)
{
    mve_base_error ret;
    bool res;

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    mver_scheduler_lock();
    lock_session(session);
    ret = mve_session_buffer_unregister_internal(session, buffer_id);
    unlock_session(session);
    mver_scheduler_unlock();
    release_session(session);

    return ret;
}

mve_base_error mve_session_buffer_enqueue(struct mve_session *session,
                                          struct mve_base_buffer_details *param,
                                          bool empty_this_buffer)
{
    mve_base_error ret;
    bool res;

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    lock_session(session);

    if (false != empty_this_buffer)
    {
        session->state.eos_queued = ((param->flags & MVE_BASE_BUFFERFLAG_EOS) == MVE_BASE_BUFFERFLAG_EOS);
        if (false != session->state.eos_queued)
        {
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session, MVE_LOG_DEBUG, session, "Setting eos_queued.");
        }
    }
    else
    {
        session->keep_freq_high = false;
    }

    if ((false != empty_this_buffer && (0 != (session->state.flush_target & MVE_BASE_FLUSH_INPUT_PORT))) ||
        (false == empty_this_buffer && (0 != (session->state.flush_target & MVE_BASE_FLUSH_OUTPUT_PORT))))
    {
        if (session->state.flush_target & MVE_BASE_FLUSH_QUICK)
        {
            struct mve_buffer_client *buffer_client = mve_session_buffer_get_external_descriptor(session, param->buffer_id);

            if (NULL != buffer_client)
            {
                list_add_tail(&buffer_client->quick_flush_list, &session->quick_flush_buffers);
                buffer_client->mve_handle = MVE_HANDLE_INVALID;
                buffer_client->in_use += 1;

                /* Store the buffer details in the descriptor */
                buffer_client->mve_flags  = param->mve_flags;
                buffer_client->flags      = param->flags;
                buffer_client->timestamp  = param->timestamp;
                buffer_client->filled_len = param->filled_len;
            }
        }
        else
        {
            struct session_enqueue_request request;

            memcpy(&request.param, param, sizeof(struct mve_base_buffer_details));
            return_pending_buffer_to_userspace(session, &request, empty_this_buffer);
        }

        unlock_session(session);
        release_session(session);
        return MVE_BASE_ERROR_NONE;
    }

    ret = mve_session_buffer_enqueue_internal(session, param, empty_this_buffer);

    /* Queue the request and return success. */
    if (MVE_BASE_ERROR_TIMEOUT == ret)
    {
        struct session_enqueue_request *request;

        /* Allocate struct and store arguments. */
        request = MVE_RSRC_MEM_ZALLOC(sizeof(*request), GFP_KERNEL);
        if (NULL == request)
        {
            ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        request->param = *param;

        /* Add request last in queue. */
        if (true == empty_this_buffer)
        {
            list_add_tail(&request->list, &session->queued_input_buffers);
        }
        else
        {
            list_add_tail(&request->list, &session->queued_output_buffers);
        }

        ret = MVE_BASE_ERROR_NONE;
    }

    if (MVE_BASE_ERROR_NONE == ret)
    {
        /* Reset the process to detect idleness and request that this session is
         * switched in again if a switch out has already started. */
        session->state.idle_state = IDLE_STATE_ACTIVE;
        session->state.request_switchin = true;

        session->state.idle_dirty = true;
    }

exit:
    unlock_session(session);
    (void)schedule_session(session);
    mver_irq_signal_mve((mver_session_id)session);
    release_session(session);

    return ret;
}

mve_base_error mve_session_buffer_notify_ref_frame_release(struct mve_session *session,
                                                           mve_base_buffer_handle_t buffer_id)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;
    bool res;

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    lock_session(session);
    mve_session_buffer_notify_ref_frame_release_internal(session, buffer_id);
    unlock_session(session);
    res = schedule_session(session);
    if (false != res)
    {
        mver_irq_signal_mve((mver_session_id)session);
    }
    release_session(session);

    return ret;
}

mve_base_error mve_session_enqueue_message(struct mve_session *session,
                                           uint32_t code,
                                           uint32_t size,
                                           void *data)
{
    mve_base_error ret;
    bool res;

    res = acquire_session(session);
    if (false == res)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    lock_session(session);
    ret = enqueue_message(session, code, size, data);
    unlock_session(session);
    schedule_session(session);
    release_session(session);

    return ret;
}

#ifndef EMULATOR
uint32_t mve_session_poll(struct file *filp, struct poll_table_struct *poll_table)
{
    struct mve_session *session;
    uint32_t mask = 0;

    session = mve_session_get_by_file(filp);
    if (NULL != session)
    {
        lock_session(session);

        poll_wait(filp, &session->mve_events->wait_queue, poll_table);

        if (false != mve_queue_event_ready(session->mve_events))
        {
            mask |= POLLIN | POLLRDNORM;
        }

        unlock_session(session);
    }

    return mask;
}
#endif

#ifdef UNIT
#ifndef DISABLE_WATCHDOG
void mve_session_firmware_hung_simulation(struct mve_session *session, uint32_t on)
{
    if (1 == on)
    {
        watchdog_ignore_pong = true;
    }
    else
    {
        watchdog_ignore_pong = false;
    }
}
#endif
#endif
