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

#include "mve_buffer_dmabuf.h"

struct mve_buffer_external *mve_buffer_dmabuf_create_buffer_external6(struct mve_buffer_info *info)
{
    return NULL;
}

void mve_buffer_dmabuf_destroy_buffer_external6(struct mve_buffer_external *buffer)
{}

bool mve_buffer_dmabuf_map_physical_pages6(struct mve_buffer_external *buffer)
{
    return false;
}

bool mve_buffer_dmabuf_unmap_physical_pages6(struct mve_buffer_external *buffer)
{
    return false;
}

void mve_buffer_dmabuf_set_owner6(struct mve_buffer_external *buffer,
                                 enum mve_buffer_owner owner,
                                 enum mve_port_index port)
{}
