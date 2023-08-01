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

#include "mve_buffer.h"

#include "mve_buffer_valloc.h"
#include "mve_buffer_attachment.h"
#include "mve_buffer_dmabuf.h"
#include "mve_buffer_ashmem.h"

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <asm/bug.h>
#endif

/** @brief Allocator interface
 *
 * Collection of function-pointers that together create an interface for an
 * allocator.
 */
struct allocator_interface
{
    /** Create the buffer client */
    struct mve_buffer_external *(*create_buffer_external)(struct mve_buffer_info *info);
    /** Destroy the buffer client */
    void (*destroy_buffer_external)(struct mve_buffer_external *buffer);
    /** Map the memory */
    bool (*map_physical_pages)(struct mve_buffer_external *buffer);
    /** Unmap the memory */
    bool (*unmap_physical_pages)(struct mve_buffer_external *buffer);
    /** Set the owner of the buffer (CPU or device) */
    void (*set_owner)(struct mve_buffer_external *buffer,
                      enum mve_buffer_owner owner,
                      enum mve_port_index port);
};

static struct allocator_interface allocators[] =
{
    {   /* Valloc */
        .create_buffer_external  = mve_buffer_valloc_create_buffer_external,
        .destroy_buffer_external = mve_buffer_valloc_destroy_buffer_external,
        .map_physical_pages      = mve_buffer_valloc_map_physical_pages,
        .unmap_physical_pages    = mve_buffer_valloc_unmap_physical_pages,
        .set_owner               = mve_buffer_valloc_set_owner
    },
    {   /* Attachment */
        .create_buffer_external  = mve_buffer_attachment_create_buffer_external,
        .destroy_buffer_external = mve_buffer_attachment_destroy_buffer_external,
        .map_physical_pages      = mve_buffer_attachment_map_physical_pages,
        .unmap_physical_pages    = mve_buffer_attachment_unmap_physical_pages,
        .set_owner               = mve_buffer_attachment_set_owner
    },
#ifndef EMULATOR
    {   /* dma buf */
        .create_buffer_external  = mve_buffer_dmabuf_create_buffer_external,
        .destroy_buffer_external = mve_buffer_dmabuf_destroy_buffer_external,
        .map_physical_pages      = mve_buffer_dmabuf_map_physical_pages,
        .unmap_physical_pages    = mve_buffer_dmabuf_unmap_physical_pages,
        .set_owner               = mve_buffer_dmabuf_set_owner
    },
    {   /* ashmem buf */
        .create_buffer_external  = mve_buffer_ashmem_create_buffer_external,
        .destroy_buffer_external = mve_buffer_ashmem_destroy_buffer_external,
        .map_physical_pages      = mve_buffer_ashmem_map_physical_pages,
        .unmap_physical_pages    = mve_buffer_ashmem_unmap_physical_pages,
        .set_owner               = mve_buffer_ashmem_set_owner
    }
#endif
};

bool mve_buffer_is_valid(const struct mve_buffer_info *info)
{
    int bpp, size;

    if (0 == info->handle)
    {
        return false;
    }

    if (0 == info->size)
    {
        return false;
    }

    if (MVE_BASE_BUFFER_FORMAT_BITSTREAM != info->format &&
        MVE_BASE_BUFFER_FORMAT_CRC != info->format)
    {
        if (0 == info->width ||
            0 == info->height ||
            0 == info->stride)
        {
            return false;
        }
    }

    if (MVE_BASE_BUFFER_ALLOCATOR_VMALLOC != info->allocator &&
        MVE_BASE_BUFFER_ALLOCATOR_DMABUF != info->allocator &&
        MVE_BASE_BUFFER_ALLOCATOR_ASHMEM != info->allocator &&
        MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT != info->allocator)
    {
        return false;
    }

    if (MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT == info->allocator &&
        0 == info->offset)
    {
        return false;
    }

    switch (info->format)
    {
        case MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR:
        case MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR:
        case MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR:
        case MVE_BASE_BUFFER_FORMAT_YV12:
            bpp = 12;
            if (((info->width * info->height * bpp) / 8) > info->size)
            {
                return false;
            }
            if (info->stride * info->height > info->size)
            {
                return false;
            }
            if (info->stride < info->width)
            {
                return false;
            }

            if (MVE_BASE_BUFFER_ALLOCATOR_DMABUF == info->allocator ||
                MVE_BASE_BUFFER_ALLOCATOR_ASHMEM == info->allocator)
            {
                if (MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR == info->format ||
                    MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR == info->format)
                {
                    size = ROUND_UP(info->stride, info->stride_alignment) * info->height +
                           ROUND_UP(info->stride, info->stride_alignment) * info->height / 2;
                }
                else
                {
                    size = ROUND_UP(info->stride, info->stride_alignment) * info->height +
                           ROUND_UP((info->stride + 1) >> 1, info->stride_alignment) * info->height;
                }
                /* This will help detect stride alignment changes that causes
                 * the buffer to become smaller than expected */
                WARN_ON(size > info->size);
            }
            break;
        case MVE_BASE_BUFFER_FORMAT_YUYYVY_10B:
            bpp = 16;
            if (((info->width * info->height * bpp) / 8) > info->size)
            {
                return false;
            }
            if (info->stride < info->width)
            {
                return false;
            }
            if (info->stride * 4 * info->height / 2 > info->size)
            {
                return false;
            }
            if (MVE_BASE_BUFFER_ALLOCATOR_DMABUF == info->allocator ||
                MVE_BASE_BUFFER_ALLOCATOR_ASHMEM == info->allocator)
            {
                size = ROUND_UP(info->stride * 4, info->stride_alignment) * info->height / 2;
                /* This will help detect stride alignment changes that causes
                 * the buffer to become smaller than expected */
                WARN_ON(size > info->size);
            }
            break;
        case MVE_BASE_BUFFER_FORMAT_RGBA_8888:
        case MVE_BASE_BUFFER_FORMAT_BGRA_8888:
        case MVE_BASE_BUFFER_FORMAT_ARGB_8888:
        case MVE_BASE_BUFFER_FORMAT_ABGR_8888:
            bpp = 32;
            if (((info->width * info->height * bpp) / 8) > info->size)
            {
                return false;
            }
            if (info->stride < info->width * 4)
            {
                return false;
            }
            if (info->stride * info->height > info->size)
            {
                return false;
            }

            size = ROUND_UP(info->stride, info->stride_alignment) * info->height;
            /* This will help detect stride alignment changes that causes
             * the buffer to become smaller than expected */
            WARN_ON(size > info->size);
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC:
        {
            int w = (info->width + 15) / 16;
            int h = (info->height + 15) / 16;
            /* Magic formula for worstcase content */
            if (w * 16 * h * 16 / 16 + w * 16 * h * 16 * 3 / 2 > info->size)
            {
                return false;
            }
            break;
        }
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B:
        {
            int w = (info->width + 15) / 16;
            int h = (info->height + 15) / 16;
            /* Magic formula for worstcase content */
            if (w * h * 16 + w * 16 * h * 16 * 15 / 8 > info->size)
            {
                return false;
            }
            break;
        }
        case MVE_BASE_BUFFER_FORMAT_YUV420_I420_10:
        {
            if (info->stride < info->width * 2)
            {
                return false;
            }
            if (info->stride * info->height * 3 / 2 > info->size)
            {
                return false;
            }
            break;
        }
        case MVE_BASE_BUFFER_FORMAT_BITSTREAM:
        case MVE_BASE_BUFFER_FORMAT_YUV422_1P:
        case MVE_BASE_BUFFER_FORMAT_YVU422_1P:
        case MVE_BASE_BUFFER_FORMAT_CRC:
            break;
        default:
            /* Unsupported format */
            return false;
    }

    return true;
}

struct mve_buffer_external *mve_buffer_create_buffer_external(struct mve_buffer_info *info, uint32_t port)
{
    struct mve_buffer_external *buffer = allocators[info->allocator].create_buffer_external(info);
    if (NULL != buffer)
    {
        buffer->mapping.write = 1; /*MVE_PORT_INDEX_INPUT == port ? 0 : 1*/
    }
    return buffer;
}

void mve_buffer_destroy_buffer_external(struct mve_buffer_external *buffer)
{
    allocators[buffer->info.allocator].destroy_buffer_external(buffer);
}

bool mve_buffer_map_physical_pages(struct mve_buffer_external *buffer)
{
    return allocators[buffer->info.allocator].map_physical_pages(buffer);
}

bool mve_buffer_unmap_physical_pages(struct mve_buffer_external *buffer)
{
    return allocators[buffer->info.allocator].unmap_physical_pages(buffer);
}

void mve_buffer_set_owner(struct mve_buffer_external *buffer,
                          enum mve_buffer_owner owner,
                          enum mve_port_index port)
{
    allocators[buffer->info.allocator].set_owner(buffer, owner, port);
}
