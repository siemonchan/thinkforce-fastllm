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

#include "mve_mmu.h"
#include "mve_buffer_attachment.h"

/* This is an implementation of the mve_buffer_client interface where the client
 * buffers are attachment to other buffers.
 */

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x) ((void)(x))
#endif

struct mve_buffer_external *mve_buffer_attachment_create_buffer_external3(struct mve_buffer_info *info)
{
    /* The attachment implementation doesn't need to store any private data in
     * the mve_buffer_client structure. Just create an instance and return it. */
    struct mve_buffer_external *ret;

    ret = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_buffer_external), GFP_KERNEL);
    if (NULL != ret)
    {
        ret->info = *info;
    }

    return ret;
}

void mve_buffer_attachment_destroy_buffer_external3(struct mve_buffer_external *buffer)
{
    MVE_RSRC_MEM_CACHE_FREE(buffer, sizeof(struct mve_buffer_external));
}

bool mve_buffer_attachment_map_physical_pages3(struct mve_buffer_external *buffer)
{
    CSTD_UNUSED(buffer);

    return true;
}

bool mve_buffer_attachment_unmap_physical_pages3(struct mve_buffer_external *buffer)
{
    CSTD_UNUSED(buffer);

    return true;
}

void mve_buffer_attachment_set_owner3(struct mve_buffer_external *buffer,
                                     enum mve_buffer_owner owner,
                                     enum mve_port_index port)
{
    CSTD_UNUSED(buffer);
    CSTD_UNUSED(owner);
    CSTD_UNUSED(port);
}
