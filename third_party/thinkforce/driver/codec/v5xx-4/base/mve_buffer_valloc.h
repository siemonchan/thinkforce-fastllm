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

#ifndef MVE_BUFFER_VALLOC_H
#define MVE_BUFFER_VALLOC_H

#include "mve_buffer.h"

/**
 * Allocate memory for private data and a mve_buffer_external instance. This is
 * also the place to e.g. import the external buffer etc.
 * @param info Buffer information.
 * @return Pointer to the mve_buffer_external part of the allocated structure. NULL
 *         if no such structure could be allocated.
 */
struct mve_buffer_external *mve_buffer_valloc_create_buffer_external4(struct mve_buffer_info *info);

/**
 * Free the memory allocated in mve_buffer_create_buffer_external.
 * @param buffer Pointer to the mve_buffer_external instance to free.
 */
void mve_buffer_valloc_destroy_buffer_external4(struct mve_buffer_external *buffer);

/**
 * Constructs an array of physical pages backing the user allocated buffer and
 * stores the array in the supplied mve_buffer_external. Note that the pages
 * must be mapped in the MVE MMU table before MVE can access the buffer. This
 * is the responsibility of the client.
 * @param buffer The buffer to map.
 * @return True on success, false on failure.
 */
bool mve_buffer_valloc_map_physical_pages4(struct mve_buffer_external *buffer);

/**
 * Hand back the pages to the user allocated buffer allocator. Note that the
 * pages must also be unmapped from the MVE MMU table. This is the responsibility
 * of the client.
 * @param buffer The buffer to unmap.
 * @return True on success, false on failure. This function fails if the
 *         supplied buffer is currently not registered.
 */
bool mve_buffer_valloc_unmap_physical_pages4(struct mve_buffer_external *buffer);

/**
 * Set the owner of the buffer. This function takes care of cache flushing.
 * @param buffer The buffer to change ownership of.
 * @param owner  The new owner of the buffer.
 * @param port   The port this buffer is connected to (input/output).
 */
void mve_buffer_valloc_set_owner4(struct mve_buffer_external *buffer,
                                 enum mve_buffer_owner owner,
                                 enum mve_port_index port);

#endif /* MVE_BUFFER_VALLOC_H */
