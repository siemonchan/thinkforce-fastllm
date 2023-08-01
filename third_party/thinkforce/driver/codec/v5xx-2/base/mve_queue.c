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
#else
#include <linux/slab.h>
#include <linux/sched.h>
#endif

#include "mve_base.h"
#include "mve_queue.h"
#include "mve_rsrc_mem_frontend.h"

struct mve_queue *mve_queue_create2(uint32_t size)
{
    struct mve_queue *queue;

    queue = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_queue), GFP_KERNEL);
    if (NULL == queue)
    {
        goto error;
    }

    queue->buffer = MVE_RSRC_MEM_VALLOC(size * sizeof(uint32_t));
    if (NULL == queue->buffer)
    {
        goto error;
    }

    sema_init(&queue->consumer_lock, 1);
    init_waitqueue_head(&queue->wait_queue);

    queue->buffer_size = size;
    queue->rpos = 0;
    queue->wpos = 0;
    queue->interrupted = false;

    return queue;

error:
    if (NULL != queue)
    {
        if (NULL != queue->buffer)
        {
            MVE_RSRC_MEM_VFREE(queue->buffer);
        }

        MVE_RSRC_MEM_FREE(queue);
    }

    return NULL;
}

void mve_queue_destroy2(struct mve_queue *queue)
{
    if (NULL != queue)
    {
        MVE_RSRC_MEM_VFREE(queue->buffer);
        MVE_RSRC_MEM_FREE(queue);
    }
}

/**
 * Returns the event header for the current event.
 * @param queue The queue.
 * @return The event header of the current event.
 */
static struct mve_base_event_header get_event_header(struct mve_queue *queue)
{
    uint32_t buf_size = queue->buffer_size;

    return *((struct mve_base_event_header *)&queue->buffer[queue->rpos % buf_size]);
}

void mve_queue_interrupt2(struct mve_queue *queue)
{
    queue->interrupted = true;
    wake_up_interruptible(&queue->wait_queue);
}

bool mve_queue_interrupted2(struct mve_queue *queue)
{
    return queue->interrupted;
}

struct mve_base_event_header *mve_queue_wait_for_event2(struct mve_queue *queue, uint32_t timeout)
{
    struct mve_base_event_header *event = NULL;
    struct mve_base_event_header header;
    uint32_t buf_size;
    uint32_t words;
    uint32_t i;
    uint32_t *dst;
    int ret;
    int sem_taken;

    if (NULL == queue)
    {
        return NULL;
    }

    if (0 != timeout)
    {
        queue->interrupted = false;
        /* Wait for items to process */
        ret = wait_event_interruptible_timeout(queue->wait_queue,
                                               (0 < (queue->wpos - queue->rpos) || false != queue->interrupted),
                                               msecs_to_jiffies(timeout));
        if (1 > ret)
        {
            return NULL;
        }
    }

    sem_taken = down_interruptible(&queue->consumer_lock);

    if (0 == queue->wpos - queue->rpos)
    {
        goto exit;
    }

    header = get_event_header(queue);
    words = (header.size + sizeof(uint32_t) - 1) / sizeof(uint32_t);

    event = mve_queue_create_event2(header.code, header.size);
    if (NULL == event)
    {
        /* Failed to allocate memory. Skip this event */
        goto out;
    }

    /* Copy data to return buffer */
    buf_size = queue->buffer_size;
    dst = (uint32_t *)event->data;
    for (i = 0; i < words; ++i)
    {
        dst[i] = queue->buffer[(sizeof(struct mve_base_event_header) / sizeof(uint32_t) +
                                queue->rpos + i) % buf_size];
    }

out:
    queue->rpos += (sizeof(struct mve_base_event_header) + words * sizeof(uint32_t)) /
                   sizeof(uint32_t);
exit:
    if (0 == sem_taken)
    {
        up(&queue->consumer_lock);
    }

    return event;
}

void mve_queue_add_event2(struct mve_queue *queue,
                         struct mve_base_event_header *event)
{
    struct mve_base_event_header *header;
    uint32_t buf_size;
    uint32_t words, i;
    uint32_t *dst, *src;
    int sem_taken;

    if (NULL == queue || NULL == event)
    {
        return;
    }

    dst = (uint32_t *)queue->buffer;
    src = (uint32_t *)event->data;

    words = (event->size + sizeof(uint32_t) - 1) / sizeof(uint32_t);

    sem_taken = down_interruptible(&queue->consumer_lock);
    if (0 != sem_taken)
    {
        /* Drop this event! */
        return;
    }

    buf_size = queue->buffer_size;
    if (queue->wpos + sizeof(struct mve_base_event_header) / sizeof(uint32_t) + words -
        queue->rpos > buf_size)
    {
        /* Drop this event! */
        /* WARN_ON(true); */
        up(&queue->consumer_lock);
        return;
    }

    header = (struct mve_base_event_header *)(dst + (queue->wpos++ % buf_size));
    header->code = event->code;
    header->size = event->size;

    for (i = 0; i < words; ++i)
    {
        dst[queue->wpos++ % buf_size] = src[i];
    }

    up(&queue->consumer_lock);
    /* Notify listeners that an item has been added to the queue */
    wake_up_interruptible(&queue->wait_queue);
}

struct mve_base_event_header *mve_queue_create_event2(int code, int size)
{
    struct mve_base_event_header *event;

    event = MVE_RSRC_MEM_CACHE_ALLOC(EVENT_SIZE(size), GFP_KERNEL);
    if (NULL != event)
    {
        event->code = code;
        event->size = size;
    }

    return event;
}

bool mve_queue_event_ready2(struct mve_queue *queue)
{
    return 0 < (queue->wpos - queue->rpos);
}
