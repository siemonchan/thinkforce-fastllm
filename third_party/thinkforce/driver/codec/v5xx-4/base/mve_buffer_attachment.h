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

#ifndef MVE_BUFFER_ATTACHMENT_H
#define MVE_BUFFER_ATTACHMENT_H

#include "mve_buffer.h"

/**
 * Allocate memory for private data and a mve_buffer_external instance.
 * @param info Buffer information.
 * @return Pointer to the mve_buffer_external part of the allocated structure. NULL
 *         if no such structure could be allocated.
 */
struct mve_buffer_external *mve_buffer_attachment_create_buffer_external4(struct mve_buffer_info *info);

/**
 * Free the memory allocated in mve_buffer_create_buffer_external.
 * @param buffer Pointer to the mve_buffer_external instance to free.
 */
void mve_buffer_attachment_destroy_buffer_external4(struct mve_buffer_external *buffer);

/**
 * This function doesn't do anything for attachment buffers.
 * @param buffer The buffer.
 * @return Always returns true
 */
bool mve_buffer_attachment_map_physical_pages4(struct mve_buffer_external *buffer);

/**
 * This function doesn't do anything for attachment buffers.
 * @param buffer The buffer.
 * @return Always returns true
 */
bool mve_buffer_attachment_unmap_physical_pages4(struct mve_buffer_external *buffer);

/**
 * This function doesn't do anything for attachment buffers.
 */
void mve_buffer_attachment_set_owner4(struct mve_buffer_external *buffer,
                                     enum mve_buffer_owner owner,
                                     enum mve_port_index port);

#endif /* MVE_BUFFER_ATTACHMENT_H */
