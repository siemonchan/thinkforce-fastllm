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
#endif

#include "mve_rsrc_circular_buffer.h"
#include "mve_rsrc_mem_frontend.h"

struct mver_circular_buffer *mver_circular_buffer_create5(int size)
{
    struct mver_circular_buffer *cb;

    cb = (struct mver_circular_buffer *)
         MVE_RSRC_MEM_ZALLOC(sizeof(struct mver_circular_buffer), GFP_KERNEL);
    if (NULL == cb)
    {
        return NULL;
    }

    cb->data = (void **)MVE_RSRC_MEM_VALLOC(sizeof(void *) * size);
    if (NULL == cb->data)
    {
        MVE_RSRC_MEM_FREE(cb);
        return NULL;
    }

    cb->size = size;
    cb->r_pos = 0;
    cb->w_pos = 0;

    return cb;
}

void mver_circular_buffer_destroy5(struct mver_circular_buffer *cb)
{
    if (NULL != cb)
    {
        MVE_RSRC_MEM_VFREE(cb->data);
        MVE_RSRC_MEM_FREE(cb);
    }
}

static bool has_free_element(struct mver_circular_buffer *cb)
{
    /* A circular buffer of size can only hold size - 1 elements, otherwise
     * r_pos and w_pos will point at the same element which is the same
     * state as for an empty list. */
    return mver_circular_buffer_get_num_entries5(cb) < cb->size - 1;
}

bool mver_circular_buffer_add5(struct mver_circular_buffer *cb, void *item)
{
    bool ret = false;

    if (has_free_element(cb))
    {
        cb->data[cb->w_pos] = item;
        cb->w_pos = (cb->w_pos + 1) % cb->size;
        ret = true;
    }

    if (false == ret)
    {
        WARN_ON(true);
    }

    return ret;
}

bool mver_circular_buffer_remove5(struct mver_circular_buffer *cb, void **data)
{
    bool ret = false;

    if (mver_circular_buffer_get_num_entries5(cb) > 0)
    {
        *data = cb->data[cb->r_pos];
        cb->r_pos = (cb->r_pos + 1) % cb->size;
        ret = true;
    }
    else
    {
        *data = NULL;
    }

    return ret;
}

int mver_circular_buffer_get_num_entries5(struct mver_circular_buffer *cb)
{
    int num = 0;

    if (cb->r_pos <= cb->w_pos)
    {
        num = cb->w_pos - cb->r_pos;
    }
    else
    {
        num = cb->size - cb->r_pos + cb->w_pos;
    }

    return num;
}

bool mver_circular_buffer_peek5(struct mver_circular_buffer *cb, void **data)
{
    bool ret = false;

    if (mver_circular_buffer_get_num_entries5(cb) > 0)
    {
        *data = cb->data[cb->r_pos];
        ret = true;
    }
    else
    {
        *data = NULL;
    }

    return ret;
}

void mver_circular_buffer_remove_all_occurences5(struct mver_circular_buffer *cb, void *data)
{
    int curr;

    curr = cb->r_pos;
    while (curr != cb->w_pos)
    {
        if (data == cb->data[curr])
        {
            int i = curr;
            /* Remove the current element by moving all the rest of the elements one step down */
            while (i != cb->w_pos)
            {
                cb->data[i] = cb->data[(i + 1) % cb->size];
                i = (i + 1) % cb->size;
            }

            cb->w_pos = (0 == cb->w_pos) ? cb->size - 1 : cb->w_pos - 1;
        }
        else
        {
            curr = (curr + 1) % cb->size;
        }
    }
}
