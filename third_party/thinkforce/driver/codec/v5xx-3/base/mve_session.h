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

#ifndef MVE_SESSION_H
#define MVE_SESSION_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#endif

#include "mve_base.h"
#include "mve_mmu.h"
#include "mve_fw.h"
#include "mve_queue.h"
#include "mve_buffer.h"
#include "mve_command.h"
#include "mve_com.h"

#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_mem_dma.h"

/**
 * Describing the session state.
 */
enum MVE_SESSION_STATE
{
    STATE_SWITCHED_OUT,         /**< Session is switched out and doesn't have an allocated LSID */
    STATE_SWITCHING_IN,         /**< Session is in the process of being switched in */
    STATE_SWITCHING_OUT,        /**< Session is in the process of being switched out */
    STATE_WAITING_FOR_RESPONSE, /**< Session is waiting for response from e.g. a get/set call */
    STATE_ACTIVE_JOB,           /**< Session is active, decoding/encoding a stream */
};

/**
 * Describing the session watchdog state.
 */
enum MVE_WATCHDOG_STATE
{
    WATCHDOG_IDLE,        /**< Watchdog idle */
    WATCHDOG_PINGED,      /**< Watchdog pinged the session and wait for response */
    WATCHDOG_PONGED,      /**< MVE responsed ping with pong */
    WATCHDOG_TIMEOUT,     /**< Watchdog timeout for ping response */
};

enum MVE_IDLE_STATE
{
    IDLE_STATE_ACTIVE,       /**< Session has not received any idle messages */
    IDLE_STATE_PENDING,      /**< Session has received one idle message but the FW can still have work to do */
    IDLE_STATE_IDLE,         /**< Session has received three idle messages in a row without the client sending
                              *   any other messages in between. This session can now be switched out. */
};

/**
 * This structure describes the state of the session
 */
struct mve_state
{
    enum MVE_SESSION_STATE state;               /**< The current session state */
    uint32_t irqs;                              /**< Interrupt counter */

    enum mve_base_hw_state hw_state;            /**< Track the hw state */
    enum mve_base_hw_state pending_hw_state;    /**< Pending hardware state that will be requested
                                                 *   next time the session switches in. */

    enum MVE_WATCHDOG_STATE watchdog_state;     /**< The current watchdog state */
    uint32_t ping_count;                        /**< Watchdog ping counter, the hardware
                                                 *   is considered to be dead when it reached
                                                 *   max counter without ponged*/
    struct timespec64 timestamp;                /**< Timestamp marking the most recent
                                                 *   respons from the FW. This is used by the
                                                 *   watchdog to decide when the FW
                                                 *   should be pinged. */
    enum MVE_IDLE_STATE idle_state;             /**< Describing the FW idle state */
    bool request_switchin;                      /**< This member marks that the session should be
                                                 *   switched in upon switchout */
    bool request_switchout;                     /**< This member marks that the sessions should be
                                                 *   switched out as soon as possible */
    int job_frame_size;                         /**< Size of the last queued job */

    enum mve_base_flush flush_state;            /**< The current flush state */
    enum mve_base_flush flush_target;           /**< When the current flush state
                                                 *   has reached this state then we're
                                                 *   done */
    bool eos_queued;                            /**< Indicates last incoming buffer has EOS flag */

    bool idle_dirty;                            /**< Only used for FW HI [2, 2.4] or [3, 3.1]: Set to true
                                                 *   when idleness can now always be decided by looking only
                                                 *   at the most recent idle messages */
};

enum mve_session_type
{
    MVE_SESSION_TYPE_DECODER = 0,
    MVE_SESSION_TYPE_ENCODER
};

/**
 * Structure representing a video session.
 */
struct mve_session
{
    struct file *filep;                         /**< struct file * of the file descriptor used
                                                 *   to create this session. When a file descriptor
                                                 *   is closed, this member is used to identify all
                                                 *   sessions that must be destroyed to avoid memory
                                                 *   leaks. */

    struct mve_mmu_ctx *mmu_ctx;                /**< MMU context */

    struct mve_state state;                     /**< Session state */
    bool fw_loaded;                             /**< Has a firmware been loaded for this session? */

    char *role;                                 /**< The role string */
    struct mve_fw_instance *fw_inst;            /**< The loaded firmware instance */

    enum mve_session_type session_type;         /**< Decoder or encoder */

    uint32_t output_buffer_count;               /**< Number of output buffers currently owned by the firmware. */
    uint32_t input_buffer_count;                /**< Number of input buffers currently owned by the firmware. */
    uint32_t pending_response_count;            /**< Number of pending responses from the firmware. */
    uint32_t buffer_on_hold_count;              /**< Number of buffers that the firmware has completed process of,
                                                 *   but is holding on to for frame reordering. */
    uint32_t pending_buffer_on_hold_count;      /**< Some FW versions signal the DPB in the form of a message
                                                 *   that will be valid once the next frame has been returned
                                                 *   by the FW. In these cases, the new DPB is stored in this
                                                 *   variable and copied to buffer_on_hold_count once the next
                                                 *   buffer arrives. */
    struct list_head queued_input_buffers;      /**< Input buffers that have not yet been added to firmware queue. */
    struct list_head queued_output_buffers;     /**< Output buffers that have not yet been added to firmware queue. */

    struct list_head external_buffers;          /**< External buffer mappings */
    struct list_head quick_flush_buffers;       /**< List of quick flush buffers */

    uint16_t next_mve_handle;                   /**< Each external buffer is assigned a session
                                                 *   unique 16-bit identifier. This ID is used to
                                                 *   match all buffers returned from the MVE with
                                                 *   the corresponding mve_buffer_client instance. The
                                                 *   next_buffer_handle is incremented for each
                                                 *   registered buffer and the value is assigned
                                                 *   to the corresponding mve_buffer_client instance. */

    struct mve_com *com;                        /**< Firmware communication. */
    enum mve_base_fw_major_prot_ver prot_ver;   /**< FW host interface protocol version */
    bool rpc_in_progress;                       /**< RPC irq being processed, useful for async rpc memory operations */

    struct mve_rsrc_dma_mem_t *msg_in_queue;    /**< CPU -> MVE messages */
    struct mve_rsrc_dma_mem_t *msg_out_queue;   /**< CPU <- MVE messages */
    struct mve_rsrc_dma_mem_t *buf_input_in;    /**< Input buffers to MVE */
    struct mve_rsrc_dma_mem_t *buf_input_out;   /**< Input buffers returned by MVE */
    struct mve_rsrc_dma_mem_t *buf_output_in;   /**< Output buffers to MVE */
    struct mve_rsrc_dma_mem_t *buf_output_out;  /**< Output buffers returned by MVE */
    struct mve_rsrc_dma_mem_t *rpc_area;        /**< Physical address of the RPC data */

    struct mve_queue *mve_events;               /**< Queue for regular MVE events */
    struct mve_queue *response_events;          /**< Queue for query responses */

    struct semaphore semaphore;                 /**< Semaphore offering mutual exclusion */
    struct kref refcount;                       /**< Session reference counter */
    struct list_head list;                      /**< Sessions linked list */
    bool keep_freq_high;                        /**< Control DVFS by avoiding low frequency at start of usecase */

    struct mve_com_trace_buffers fw_trace;      /**< Book keeping data for FW trace buffers */
};

/**
 * Performs all initialization required before any sessions can be created. This
 * includes creating a watchdog thread, master session semaphore and debug
 * functionality initialization.
 * @param dev Pointer to the device structure received from the kernel.
 */
void mve_session_init3(struct device *dev);

/**
 * Frees resources allocated during mve_session_init. This function must
 * be called when the driver is uninstalled.
 * @param dev Pointer to the device structure received from the kernel.
 */
void mve_session_deinit3(struct device *dev);

/**
 * Frees all created sessions. This function is called internally by
 * mve_session_deinit.
 */
void mve_session_destroy_all_sessions3(void);

/**
 * Create a new session.
 * @param filep ID used to identify which client process this session belongs to.
 *              This will be the ID of the created session.
 * @return mve_session struct used to represent this session.
 */
struct mve_session *mve_session_create3(struct file *filep);

/**
 * Destroy session. All resources referenced by this session are freed.
 * @param session Session to destroy.
 */
void mve_session_destroy3(struct mve_session *session);

/**
 * This function is called to make sure that all sessions created by a process
 * are destroyed when the process quits. The supplied parameter is used to
 * identify the process that has just quit.
 * @param filep struct file * of the file descriptor used to create sessions.
 */
void mve_session_cleanup_client3(struct file *filep);

/**
 * Destroy all sessions currently allocated.
 */
void mve_session_destroy_all3(void);

/**
 * Returns the session associated with the supplied struct file *.
 * @param filep The struct file * associated with the file descriptor used to
 *              create the session that will be returned.
 * @return A session or NULL if no such has been created using the supplied struct file *.
 */
struct mve_session *mve_session_get_by_file3(struct file *filep);

/**
 * Set the role of the session. The role is used to decide which firmware
 * to load. The role must be set before the session can be activated. Note
 * that the role is not validated until the session is activated.
 * @param session The MVE session.
 * @param role The role string.
 */
void mve_session_set_role3(struct mve_session *session, char *role);

/**
 * Activate session. This function loads the firmware and sets up the
 * MMU tables. Note that the role of the session must be set before
 * the session can be activated.
 * @param session Session to activate.
 * @param version Firmware protocol version returned.
 * @param fw_secure_desc Contains fw version and l2 page tables address for secure sessions.
 * @return True on success, false on failure.
 */
bool mve_session_activate3(struct mve_session *session, uint32_t *version,
                          struct mve_base_fw_secure_descriptor *fw_secure_desc);

/**
 * Enqueue commands to stop a running session and flush all input and output
 * buffers.
 *
 * @param session Session to stop.
 * @param flush Specifies what kind of flush to do. See enum mve_flush
 * @return MVE error code.
 */
mve_base_error mve_session_enqueue_flush_buffers3(struct mve_session *session,
                                                 uint32_t flush);

/**
 * Enqueues a state change command to the session.
 * @param session The session to enqueue the state change command.
 * @param state   The state to change to.
 * @return MVE error code.
 */
mve_base_error mve_session_enqueue_state_change3(struct mve_session *session,
                                                enum mve_base_hw_state state);

/**
 * Get a queued MVE event. If no event is pending then the call is blocking
 * until either an event becomes available, or the timeout has expired.
 *
 * If the timeout expires without an event being available the method will
 * return NULL.
 *
 * If a non-NULL value is returned then it is the responsibility of the
 * caller to free the memory associated with the returned pointer.
 * @param session The session.
 * @param timeout Timeout, in milliseconds.
 * @return Pointer to a mve_event structure containing the event data, or NULL
 *         in the case of a timeout where no event is returned. */
struct mve_base_event_header *mve_session_get_event3(struct mve_session *session, uint32_t timeout);

/**
 * Set a firmware option or buffer param. The attached data must contain the
 * firmware option or buffer param structure structure.
 * @param session           The session.
 * @param size              Size of the attached data.
 * @param data              The option/param data to set.
 * @param firmware_error    Firmware error code.
 * @param entity            Specifies whether this is an option or buffer param operation.
 * @return driver error code.
 */
mve_base_error mve_session_set_paramconfig3(struct mve_session *session,
                                           uint32_t size,
                                           void *data,
                                           uint32_t *firmware_error,
                                           enum mve_base_command_type entity);

/**
 * Register a user allocated buffer with the session. This must be done before
 * the buffer can be used by the MVE.
 * @param session    The session.
 * @param port_index Firmware Buffer Queue Index (0 = input, 1 = output).
 * @param descriptor Buffer descriptor.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_register3(struct mve_session *session,
                                           uint32_t port_index,
                                           struct mve_base_buffer_userspace *descriptor);

/**
 * Unregister a user allocated buffer with the session. This must be done
 * to free system resources.
 * @param session   The session.
 * @param buffer_id ID of the buffer to unregister.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_unregister3(struct mve_session *session,
                                             mve_base_buffer_handle_t buffer_id);

/**
 * Enqueue an input or output buffer.
 * @param session           The session.
 * @param param             Description of the input/output buffer.
 * @param empty_this_buffer Signals whether this buffer shall be emptied or filled.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_enqueue3(struct mve_session *session,
                                          struct mve_base_buffer_details *param,
                                          bool empty_this_buffer);

/**
 * Send a request to the firmware that the userspace driver wants to be notified
 * when the supplied buffer is no longer used as a reference frame.
 * @param session   The session.
 * @param buffer_id ID of the buffer.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_notify_ref_frame_release3(struct mve_session *session,
                                                           mve_base_buffer_handle_t buffer_id);

/**
 * Enqueue a command to the command queue. This function must not be
 * exposed to userspace.
 * @param session The session.
 * @param code    Command code.
 * @param size    Size of the attached data.
 * @param data    Pointer to the first byte of the data attached to the command.
 * @return MVE error code.
 */
mve_base_error mve_session_enqueue_message3(struct mve_session *session,
                                           uint32_t code,
                                           uint32_t size,
                                           void *data);

/**
 * This function is used to support fd poll and select.
 * @param filp       struct file * structure representing the fd.
 * @param poll_table Linux Structure used by the Linux kernel to implement poll and select.
 * @return Mask describing the operations that can be performed immediately without blocking.
 */
uint32_t mve_session_poll3(struct file *filp, struct poll_table_struct *poll_table);

#ifndef EMULATOR
/**
 * Handler for RPC Memory allocation. It can be called from mve_command in case
 * of secure memory allocations.
 * @param session The session.
 * @param fd      DMA Buf fd of allocated memory incase it is secure memory, -1 otherwise.
 * @return MVE error code.
 */
mve_base_error mve_session_handle_rpc_mem_alloc3(struct mve_session *session, int fd);

/**
 * Handler for RPC Memory resize. It can be called from mve_command in case
 * of secure memory allocations.
 * @param session The session.
 * @param fd      DMA Buf fd for additional memory incase it is secure memory, -1 otherwise.
 * @return MVE error code.
 */
mve_base_error mve_session_handle_rpc_mem_resize3(struct mve_session *session, int fd);

#endif /* EMULATOR */

#ifdef UNIT
/**
 * Simulate firmware hung by ignoring the pong response for unit test
 * @param session The session.
 * @param on      1 - Simulate hung on; 0 - Simulate off
 */
void mve_session_firmware_hung_simulation3(struct mve_session *session, uint32_t on);

void *mve_session_get_static_fptr3(char *func_name);

#endif /* UNIT */

#endif /* MVE_SESSION_H */
