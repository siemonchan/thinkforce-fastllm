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

#ifndef MVE_RSRC_CIRCULAR_BUFFER
#define MVE_RSRC_CIRCULAR_BUFFER

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

/**
 * Structure used to represent a circular buffer.
 */
struct mver_circular_buffer
{
    int size;    /**< Number of 32-bit entries in the circular buffer */
    int r_pos;   /**< Tail position (read position) */
    int w_pos;   /**< Head position (write position) */

    void **data; /**< Pointer to the actual circular buffer */
};

/**
 * Iterate over all elements in the circular buffer.
 */
#define mver_circular_buffer_for_each(index, ptr, type, cb) \
    for (index = 0; ptr = (type)cb->data[(cb->r_pos + index) % cb->size], index < mver_circular_buffer_get_num_entries(cb); ++index)

/**
 * Create a circular buffer of the given size.
 * @param size Number of 32-bit elements in the circular buffer.
 * @return Pointer to an initialized mver_circular_buffer, NULL on failure
 */
struct mver_circular_buffer *mver_circular_buffer_create6(int size);

/**
 * Destroy a circular buffer. Frees all resources that were allocated during
 * the creation of the circular buffer. Note that the elements themselves are
 * not freed.
 * @param cb The circular buffer to destroy
 */
void mver_circular_buffer_destroy6(struct mver_circular_buffer *cb);

/**
 * Add an element to the circular buffer.
 * @param cb   The circular buffer to add the element to
 * @param item The item to add to the circular buffer
 * @return True on success, false if the element couldn't be added
 */
bool mver_circular_buffer_add6(struct mver_circular_buffer *cb, void *item);

/**
 * Remove the tail element in the circular buffer and return it.
 * @param cb   The circular buffer
 * @param data This pointer will point to the removed element
 * @return True if an element was removed. False if the circular buffer was empty
 */
bool mver_circular_buffer_remove6(struct mver_circular_buffer *cb, void **data);

/**
 * Get the number of items in the circular buffer.
 * @param cb The circular buffer
 * @return The number of items in the circular buffer
 */
int mver_circular_buffer_get_num_entries6(struct mver_circular_buffer *cb);

/**
 * Get the tail element without removing it from the circular buffer.
 * @param cb   The circular buffer
 * @param data This pointer will point to the tail element
 * @return True if the circular buffer contains atleast one element, false otherwise
 */
bool mver_circular_buffer_peek6(struct mver_circular_buffer *cb, void **data);

/**
 * Remove all occurances of a certain item.
 * @param cb   The circular buffer
 * @param data The item to be removed
 */
void mver_circular_buffer_remove_all_occurences6(struct mver_circular_buffer *cb, void *data);

#endif /* MVE_RSRC_CIRCULAR_BUFFER */
