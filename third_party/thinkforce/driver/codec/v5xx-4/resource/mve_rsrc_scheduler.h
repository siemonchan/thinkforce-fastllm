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

#ifndef MVE_RSRC_SCHEDULER_H
#define MVE_RSRC_SCHEDULER_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#include <linux/device.h>
#endif

/* Defines the type used by the scheduler to identify sessions */
typedef void *mver_session_id;

/* Logical Session ID enumeration */
enum LSID {NO_LSID = -1, LSID_0, LSID_1, LSID_2, LSID_3};

/**
 * When a session has received and processed an IRQ, the scheduler queries the
 * session whether is has any remaining work to do. The session can return any of
 * the following constants to describe its current work state.
 */
enum SCHEDULE_STATE
{
    SCHEDULE_STATE_BUSY,               /**< The session is currently executing and has work
                                        *   to do. E.g. processing frames or the session is
                                        *   waiting for the response of a get/set message. */
    SCHEDULE_STATE_IDLE,               /**< Session is switched in but does not have anything to do */
    SCHEDULE_STATE_RESCHEDULE,         /**< Session is switched out but needs to be rescheduled */
    SCHEDULE_STATE_SLEEP,              /**< Session is switched out and does not have anything to do */
    SCHEDULE_STATE_REQUEST_SWITCHOUT,  /**< Session requests to be switched out. This state should
                                        *   be used with care. Instead rely on the scheduler to decide
                                        *   when a session should be switched out. */
};

/* Session callback functions */
/* Called when an IRQ is received */
typedef void (*irq_callback_fptr)(mver_session_id session_id);
/* Called when the scheduler wants to query whether the session can be switched out */
typedef enum SCHEDULE_STATE (*session_has_work_fptr)(mver_session_id session_id);
/* Notify the session to switch out */
typedef void (*session_switchout_fptr)(mver_session_id session_id, bool require_idleness);
/* Notify the session that it's about to be switched in */
typedef void (*session_switchin_fptr)(mver_session_id session_id);
/* Notify the session that it has been switched out */
typedef void (*session_switchout_completed_fptr)(mver_session_id session_id);
/* Retrieve the number of restricting buffers for the session */
typedef int (*session_get_restricting_buffer_count_fptr)(mver_session_id session_id);

/**
 * Initialize the scheduler module. Must be invoked before any of the
 * other functions in this module may be called.
 * @param dev Device parameter received from the kernel.
 */
void mver_scheduler_init4(struct device *dev);

/**
 * Deinitialize the scheduler module. Must be invoked before the driver is
 * uninstalled.
 * @param dev Device parameter received from the kernel.
 */
void mver_scheduler_deinit4(struct device *dev);

/**
 * Register a session with the scheduler. Call function mver_scheduler_enqueue_session
 * to notify the scheduler that the session has work to perform.
 * @param session_id        Session identifier.
 * @param mmu_ctrl          L0 page entry.
 * @param ncores            Number of cores to schedule.
 * @param secure            Session is secure.
 * @param irq_callback      Callback function that is invoked when an interrupt
 *                          has been received for this session.
 * @param has_work_callback Callback function that is invoked when the scheduler
 *                          wants to know the sessions execution state.
 * @param switchout_callback Callback function that is invoked when the scheduler
 *                          wants the session to prepare for switch out.
 * @param switchin_callback Callback function that is invoked when the scheduler
 *                          wants to notify the session that it has been scheduled.
 * @param switchout_complete_callback Callback function that is invoked when the scheduler
 *                          wants to notify the session that the switchout is complete.
 * @param get_restricting_buffer_count_callback Callback function that is invoked when the scheduler
 *                          wants to retrieve amount of restricting buffers enqueued to the FW.
 * @return True if the session was successfully registered with the scheduler,
 *         false otherwise.
 */
bool mver_scheduler_register_session4(mver_session_id session_id,
                                     uint32_t mmu_ctrl,
                                     int ncores,
                                     bool secure,
                                     irq_callback_fptr irq_callback,
                                     session_has_work_fptr has_work_callback,
                                     session_switchout_fptr switchout_callback,
                                     session_switchin_fptr switchin_callback,
                                     session_switchout_completed_fptr switchout_complete_callback,
                                     session_get_restricting_buffer_count_fptr get_output_buffer_count_callback);

/**
 * Unregister a session with the scheduler.
 * @param session_id Session identifier of the session to unregister.
 */
void mver_scheduler_unregister_session4(mver_session_id session_id);

/**
 * Notify the scheduler that the supplied session has work to perform
 * and wants to be scheduled for execution on the MVE.
 * @param session_id Session identifier.
 * @return True if the session was scheduled for immediate execution. False
 *         if the session is waiting to be scheduled.
 */
bool mver_scheduler_execute4(mver_session_id session_id);

/**
 * Stop session for scheduling.
 * @param session_id Session identifier.
 */
void mver_scheduler_stop4(mver_session_id session_id);

/**
 * Called when an interrupt has been received for a LSID.
 * @param lsid LSID of the session that generated the interrupt.
 */
void mver_scheduler_handle_irq4(enum LSID lsid);

/**
 * Call this function to invalidate all cached page mappings in the TLB. This
 * function must be called when pages have been removed from the page table.
 * If the supplied session is not scheduled, nothing is done.
 * @param session_id LSID of the session that shall have its TLB flushed.
 */
void mver_scheduler_flush_tlb4(mver_session_id session_id);

/**
 * Returns the LSID of the supplied session.
 * @param session_id Session identifier.
 * @return The LSID of the session if the session is mapped, NO_LSID if the
 *         session is unmapped.
 */
enum LSID mver_scheduler_get_session_lsid4(mver_session_id session_id);

/**
 * Suspend all running sessions and make sure that no new sessions are scheduled.
 * Call mver_scheduler_resume to resume scheduling.
 */
void mver_scheduler_suspend4(void);

/**
 * Resume session scheduling.
 */
void mver_scheduler_resume4(void);

/**
 * scheduler lock/unlock.
 */
void mver_scheduler_lock4(void);

void mver_scheduler_unlock4(void);

/**
 * Return number of cores supported
 */
int mver_scheduler_get_ncores4(void);

#endif /* MVE_RSRC_SCHEDULER_H */
