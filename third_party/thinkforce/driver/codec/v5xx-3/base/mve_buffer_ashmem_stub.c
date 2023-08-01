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

#include "mve_buffer_ashmem.h"

struct mve_buffer_external *mve_buffer_ashmem_create_buffer_external3(struct mve_buffer_info *info)
{
    return NULL;
}

void mve_buffer_ashmem_destroy_buffer_external3(struct mve_buffer_external *buffer)
{}

bool mve_buffer_ashmem_map_physical_pages3(struct mve_buffer_external *buffer)
{
    return false;
}

bool mve_buffer_ashmem_unmap_physical_pages3(struct mve_buffer_external *buffer)
{
    return false;
}

void mve_buffer_ashmem_set_owner3(struct mve_buffer_external *buffer,
                                 enum mve_buffer_owner owner,
                                 enum mve_port_index port)
{}
