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

#ifndef MVE_QUEUE_H
#define MVE_QUEUE_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#endif

#define EVENT_SIZE(_size)                  \
    sizeof(struct mve_base_event_header) + \
    ((_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)) * sizeof(uint32_t)

/**
 * This struct is used to store events received by the interrupt handler
 * from the MVE hardware.
 */
struct mve_queue
{
    uint32_t *buffer;               /**< Pointer to a ring buffer */
    uint32_t buffer_size;           /**< Size of the ring buffer in 32-bit words */
    uint32_t rpos;                  /**< Tail of the ring buffer */
    uint32_t wpos;                  /**< Head of the ring buffer */
    bool interrupted;               /**< Set if wait was interrupted */

    struct semaphore consumer_lock; /**< Used to prevent concurrent modifications to the queue */
    wait_queue_head_t wait_queue;   /**< Indicating whether there are any pending messages or not */
};

/**
 * Create a new queue.
 * @param size Size of the internal ring buffer in 32-bit words.
 * @returns A mve_queue.
 */
struct mve_queue *mve_queue_create6(uint32_t size);

/**
 * Destroy a queue created by mve_queue_create.
 * @param queue The queue to destroy.
 */
void mve_queue_destroy6(struct mve_queue *queue);

/**
 * Wait until there is an unprocessed event in the queue. This function will
 * return immediately if there is an unprocessed event in the queue. Otherwise
 * it will wait until an event is added to the queue or the timeout expires.
 * If the wait is interrupted this function will return false.
 * @param queue The queue.
 * @param timeout Timeout in milliseconds.
 * @return The event or NULL in case of an error.
 *         The client is responsible for freeing the pointer if it's not NULL.
 */
struct mve_base_event_header *mve_queue_wait_for_event6(struct mve_queue *queue, uint32_t timeout);

/**
 * Interrupt the wait queue and exit the wait by setting interrupted flag.
 * @param queue The queue.
 */
void mve_queue_interrupt6(struct mve_queue *queue);

/**
 * Helper function which tells if the queue was interrupted in previous wait.
 * @param queue The queue.
 * @return true in case wait was interrupted by calling mve_queue_interrupt.
 *         false otherwise.
 */
bool mve_queue_interrupted6(struct mve_queue *queue);

/**
 * Add an item to the queue. This function copies the content of the event
 * to the queue and it's the responsibility of the client to delete the
 * event object after the call has returned.
 * @param queue  The queue.
 * @param event  The event to enqueue.
 */
void mve_queue_add_event6(struct mve_queue *queue,
                         struct mve_base_event_header *event);

/**
 * Helper function that allocates a mve_queue_event and initializes the code
 * and size members.
 * @param code The value to assign to the code member.
 * @param size The value to assign to the size member.
 * @return A pointer to a mve_queue_event structure or NULL on failure.
 */
struct mve_base_event_header *mve_queue_create_event6(int code, int size);

/**
 * Returns true if the queue contains an unprocessed event.
 * @param queue The queue.
 * @return True if the queue contains an unpreccessed event. False otherwise.
 */
bool mve_queue_event_ready6(struct mve_queue *queue);

#endif /* MVE_QUEUE_H */
