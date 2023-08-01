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

#ifndef MVE_IRQ_H
#define MVE_IRQ_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/workqueue.h>
#include <linux/irqreturn.h>
#endif

#include "mve_rsrc_scheduler.h"

/**
 * Bottom half interrupt handler. For each LSID, checks whether an interrupt
 * has occurred. If this is the case, it checks for incoming messages, buffers
 * and RCP calls.
 */
void mver_irq_handler_bottom(struct work_struct *work);

/**
 * Top half interrupt handler. Receives interrupts, clears them and starts
 * the bottom half handler.
 */
irqreturn_t mver_irq_handler_top(int irq, void *dev_id);

/**
 * Initialize the interrupt handler. This function creates a work queue that
 * takes the responsibility as the bottom half interrupt handler.
 * @param dev Device parameter received from the kernel.
 */
void mver_irq_handler_init(struct device *dev);

/**
 * Tear down the interrupt handler. This function destroys the work queue
 * that was created for the bottom half interrupt handler.
 * @param dev Device parameter received from the kernel.
 */
void mver_irq_handler_deinit(struct device *dev);

/**
 * Send an interrupt to the MVE.
 * @param session_id The logical session ID of the session to interrupt.
 */
void mver_irq_signal_mve(mver_session_id session_id);

/**
 * Enable IRQ reception from the MVE. Note that enable and disable are reference
 * counted.
 * @return 0 on success, standard Linux error code on failure (see errno.h).
 */
int mver_irq_enable(void);

/**
 * Disable IRQ reception from the MVE. Note that enable and disable are reference
 * counted.
 */
void mver_irq_disable(void);

#endif /* MVE_IRQ_H */
