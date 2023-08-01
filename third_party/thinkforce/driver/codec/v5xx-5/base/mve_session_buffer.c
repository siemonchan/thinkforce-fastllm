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
#include <linux/list.h>
#include <linux/slab.h>
#endif

#include "mve_session.h"
#include "mve_buffer.h"
#include "mve_mmu.h"
#include "mve_com.h"

#include "mve_rsrc_irq.h"

#include <host_interface_v3/mve_protocol_def.h>

#define AFBC_MB_WIDTH 16
/* Map buffers on 16MB boundaries */
#define EXTERNAL_OUTPUT_BUFFER_ALIGNMENT 16 * 1024 * 1024

/**
 * Maps a buffer into MVE address space.
 * @param session   The owner of the buffer.
 * @param buffer    The buffer to map.
 * @param alignment VE alignment of the buffer mapping.
 * @return True on success, false on failure.
 */
static bool map_external_buffer_mve(struct mve_session *session,
                                    struct mve_buffer_external *buffer,
                                    uint32_t alignment)
{
    return mve_mmu_map_pages_and_reserve5(session->mmu_ctx,
                                         buffer->mapping.pages,
                                         &buffer->mapping.region,
                                         buffer->mapping.num_mve_pages,
                                         buffer->mapping.num_mve_pages,
                                         alignment,
                                         buffer->mapping.num_pages,
                                         ATTRIB_SHARED_RW,
                                         ACCESS_READ_WRITE,
                                         false);
}

/**
 * Remove the buffer pages from the MVE MMU table.
 * @param session The owner of the buffer.
 * @param buffer  User allocated buffer.
 */
static void unmap_external_buffer_mve(struct mve_session *session, struct mve_buffer_external *buffer)
{
    mve_mmu_unmap_pages5(session->mmu_ctx, buffer->mapping.region.start);
    mver_scheduler_flush_tlb5((mver_session_id)session);
}

struct mve_buffer_client *mve_session_buffer_get_external_descriptor5(struct mve_session *session,
                                                                     mve_base_buffer_handle_t buffer_id)
{
    struct list_head *pos;

    list_for_each(pos, &session->external_buffers)
    {
        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

        if (buffer_id == ptr->buffer->info.buffer_id)
        {
            return ptr;
        }
    }

    return NULL;
}

/**
 * Destroy a struct mve_buffer_external. This function makes sure that the buffer
 * is unmapped and that all memory is freed.
 * @param session The owner of the buffer.
 * @param buffer  The buffer to destroy.
 */
static void destroy_buffer_external(struct mve_session *session,
                                    struct mve_buffer_external *buffer)
{
    if (NULL != buffer)
    {
        if (MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT != buffer->info.allocator)
        {
            unmap_external_buffer_mve(session, buffer);
        }

        mve_buffer_unmap_physical_pages5(buffer);
        mve_buffer_destroy_buffer_external5(buffer);
    }
}

void mve_session_buffer_unmap_all5(struct mve_session *session)
{
    struct list_head *pos, *next;

    list_for_each_safe(pos, next, &session->external_buffers)
    {
        struct mve_buffer_client *ptr = container_of(pos, struct mve_buffer_client, register_list);

        destroy_buffer_external(session, ptr->buffer);
        destroy_buffer_external(session, ptr->crc);

        /* Remove mapping from the bookkeeping structure */
        list_del(&ptr->register_list);

        MVE_RSRC_MEM_CACHE_FREE(ptr, sizeof(struct mve_buffer_client));
    }
}

/**
 * Creates a struct mve_buffer_external instance and setups its members.
 * @param session     The owner of the buffer.
 * @param port_index  The port this buffer will be mapped to.
 * @param region_type Specifies where in the MVE address space this buffer will be mapped.
 * @param info        Buffer information.
 * @return Returns a struct mve_buffer_external on success, NULL  on failure.
 */
static struct mve_buffer_external *setup_buffer_external(struct mve_session *session,
                                                         uint32_t port_index,
                                                         enum mve_mem_virt_region_type region_type,
                                                         struct mve_buffer_info *info)
{
    struct mve_buffer_external *buffer;
    bool res;

    buffer = mve_buffer_create_buffer_external5(info, port_index);
    if (NULL == buffer)
    {
        return NULL;
    }

    mve_mem_virt_region_get5(region_type, &buffer->mapping.region);

    res = mve_buffer_map_physical_pages5(buffer);
    if (false == res)
    {
        mve_buffer_destroy_buffer_external5(buffer);
        return NULL;
    }
    else
    {
        if (false != info->do_cache_maintenance)
        {
            /* Set the CPU as the owner. When the buffer is enqueued,
             * owner will be changed to the hardware. */
            mve_buffer_set_owner5(buffer, MVE_BUFFER_OWNER_CPU, port_index);
        }

        if (MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT != info->allocator)
        {
            uint32_t alignment = 0;

            if (VIRT_MEM_REGION_OUT_BUF == region_type)
            {
                alignment = EXTERNAL_OUTPUT_BUFFER_ALIGNMENT;
            }

            res = map_external_buffer_mve(session, buffer, alignment);
            if (false == res)
            {
                mve_buffer_unmap_physical_pages5(buffer);
                mve_buffer_destroy_buffer_external5(buffer);
                return NULL;
            }
        }
    }

    return buffer;
}

mve_base_error mve_session_buffer_register_internal5(struct mve_session *session,
                                                    uint32_t port_index,
                                                    struct mve_base_buffer_userspace *userspace_buffer)
{
    enum mve_mem_virt_region_type region_mapping[][2] =
    {
        /* Input port                Output port */
        { VIRT_MEM_REGION_PROTECTED, VIRT_MEM_REGION_OUT_BUF },  /* Decoder */
        { VIRT_MEM_REGION_OUT_BUF,   VIRT_MEM_REGION_PROTECTED } /* Encoder */
    };

    struct mve_buffer_client *buffer;
    enum mve_mem_virt_region_type region_type;
    bool res;
    struct mve_buffer_info buffer_info, crc_info;
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    buffer = mve_session_buffer_get_external_descriptor5(session, userspace_buffer->buffer_id);
    if (NULL != buffer)
    {
        /* Buffer has already been registered */
        return MVE_BASE_ERROR_NONE;
    }

    buffer_info.buffer_id            = userspace_buffer->buffer_id;
    buffer_info.handle               = userspace_buffer->handle;
    buffer_info.allocator            = userspace_buffer->allocator;
    buffer_info.size                 = userspace_buffer->size;
    buffer_info.width                = userspace_buffer->width;
    buffer_info.height               = userspace_buffer->height;
    if (0 != userspace_buffer->stride)
    {
        buffer_info.stride = userspace_buffer->stride;
    }
    else
    {
        buffer_info.stride = userspace_buffer->width;
    }
    if (0 != userspace_buffer->max_height)
    {
        buffer_info.max_height = userspace_buffer->max_height;
    }
    else
    {
        buffer_info.max_height = userspace_buffer->height;
    }
    buffer_info.stride_alignment     = userspace_buffer->stride_alignment;
    buffer_info.format               = userspace_buffer->format;
    buffer_info.offset               = 0;
    buffer_info.do_cache_maintenance = true;

    buffer_info.afbc_width_in_superblocks = userspace_buffer->afbc_width_in_superblocks;
    buffer_info.afbc_alloc_bytes = userspace_buffer->afbc_alloc_bytes;

    crc_info.handle    = userspace_buffer->crc_handle;
    crc_info.allocator = userspace_buffer->crc_allocator;
    crc_info.size      = userspace_buffer->crc_size;
    crc_info.format    = MVE_BASE_BUFFER_FORMAT_CRC;
    crc_info.offset    = userspace_buffer->crc_offset;
    crc_info.do_cache_maintenance = true;

    if ((userspace_buffer->mve_flags & MVE_BASE_FLAGS_DMABUF_DISABLE_CACHE_MAINTENANCE) != 0 &&
        MVE_BASE_BUFFER_ALLOCATOR_DMABUF == userspace_buffer->allocator)
    {
        buffer_info.do_cache_maintenance = false;
        crc_info.do_cache_maintenance = false;
    }

    /* Hardware ensure memory cohenrency. */
    buffer_info.do_cache_maintenance = false;
    crc_info.do_cache_maintenance = false;

    /* Sanity-check the buffer information */
    res = mve_buffer_is_valid5(&buffer_info) &&
          (0 != crc_info.handle ? mve_buffer_is_valid5(&crc_info) : true);
    if (false == res)
    {
#ifdef _DEBUG
        WARN_ON(true);
#endif
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    /* Check that the buffer attachment is actually within the containing buffer */
    if (0 != crc_info.handle && MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT == crc_info.allocator &&
        buffer_info.size < crc_info.offset + crc_info.size)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    buffer = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_buffer_client), GFP_KERNEL);
    if (NULL == buffer)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    /* Decide in which MVE virtual region to map the external buffer */
    region_type = region_mapping[session->session_type][port_index];

    buffer->in_use     = 0;
    buffer->port_index = port_index;

    buffer->buffer = setup_buffer_external(session, port_index, region_type, &buffer_info);
    if (NULL == buffer->buffer)
    {
        ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
        goto error;
    }

    if (0 != crc_info.handle)
    {
        buffer->crc = setup_buffer_external(session, port_index, region_type, &crc_info);
        if (NULL == buffer->crc)
        {
            ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            goto error;
        }
    }

    /* Save the buffer for later */
    INIT_LIST_HEAD(&buffer->register_list);
    INIT_LIST_HEAD(&buffer->quick_flush_list);
    list_add(&buffer->register_list, &session->external_buffers);

    buffer->mve_handle = MVE_HANDLE_INVALID;

    return MVE_BASE_ERROR_NONE;

error:
    if (NULL != buffer)
    {
        destroy_buffer_external(session, buffer->buffer);
        MVE_RSRC_MEM_CACHE_FREE(buffer, sizeof(struct mve_buffer_client));
    }

    return ret;
}

mve_base_error mve_session_buffer_unregister_internal5(struct mve_session *session,
                                                      mve_base_buffer_handle_t buffer_id)
{
    struct mve_buffer_client *buffer;
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    buffer = mve_session_buffer_get_external_descriptor5(session, buffer_id);
    if (NULL == buffer)
    {
        ret = MVE_BASE_ERROR_BAD_PARAMETER;
    }
    else
    {
        if (0 != buffer->in_use)
        {
            /* The buffer is still in use! Do not unmap it */
            //WARN_ON(true);
            ret = MVE_BASE_ERROR_NOT_READY;
        }
        else
        {
            /* Remove mapping from the bookkeeping structure */
            list_del(&buffer->register_list);

            destroy_buffer_external(session, buffer->buffer);
            destroy_buffer_external(session, buffer->crc);

            MVE_RSRC_MEM_CACHE_FREE(buffer, sizeof(struct mve_buffer_client));
        }
    }

    return ret;
}

mve_base_error mve_session_buffer_enqueue_internal5(struct mve_session *session,
                                                   struct mve_base_buffer_details *param,
                                                   bool empty_this_buffer)
{
    struct mve_buffer_client *buffer_client;
    struct mve_buffer_info *buffer_info;
    struct mve_buffer_external *external_buffer;
    enum mve_com_buffer_type buffer_type = MVE_COM_BUFFER_TYPE_FRAME;

    mve_base_error ret = MVE_BASE_ERROR_NONE;
    mve_com_buffer buf;
    bool res;
    bool interlace = (param->mve_flags & MVE_BUFFER_FRAME_FLAG_INTERLACE) ? true : false;
    int stride;
    int stride_align;
    int stride_shift = 0;
    int max_height;
    uint32_t offset_in_page;

    res = (false != empty_this_buffer) ? list_empty(&session->queued_input_buffers) : list_empty(&session->queued_output_buffers);
    if (false == res)
    {
        /* There are already buffers queued waiting to be added to the firmware queue. */
        return MVE_BASE_ERROR_TIMEOUT;
    }

    buffer_client = mve_session_buffer_get_external_descriptor5(session, param->buffer_id);
    if (NULL == buffer_client)
    {
        /* The supplied user-space allocated buffer has not been registered
         * or is invalid. */
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    external_buffer = buffer_client->buffer;
    buffer_info     = &external_buffer->info;

    stride       = buffer_info->stride;
    stride_align = buffer_info->stride_alignment;
    max_height = buffer_info->max_height;

    if (interlace)
    {
        stride_shift = 1;
        max_height >>= 1;
    }

    offset_in_page = external_buffer->mapping.offset_in_page;

    /* Update the descriptor */
    buffer_client->filled_len = param->filled_len;
    buffer_client->flags = param->flags;
    buffer_client->mve_flags = param->mve_flags;
    buffer_client->timestamp = param->timestamp;

    if (NULL != buffer_client->crc)
    {
        buffer_client->crc->info.offset = param->crc_offset;
    }

    /* Assign the buffer a new MVE handle */
    if (MVE_HANDLE_INVALID == buffer_client->mve_handle)
    {
        buffer_client->mve_handle = session->next_mve_handle++;
        if (session->next_mve_handle == 0xFFFF)
        {
            session->next_mve_handle = 1;
        }
    }

    switch (buffer_info->format)
    {
        case MVE_BASE_BUFFER_FORMAT_BITSTREAM:
            buf.bitstream.nHandle     = buffer_client->mve_handle;
            buf.bitstream.nAllocLen   = buffer_info->size;
            buf.bitstream.pBufferData = external_buffer->mapping.region.start + offset_in_page;
            buf.bitstream.nFilledLen  = param->filled_len;
            buf.bitstream.nOffset     = 0;
            buf.bitstream.nFlags      = param->flags;
            buf.bitstream.timestamp   = param->timestamp;
            buffer_type = MVE_COM_BUFFER_TYPE_BITSTREAM;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR:
        case MVE_BASE_BUFFER_FORMAT_YV12:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride;

            /* The chroma and luma stride should be specified from gralloc eventually */
            buf.frame.data.planar.stride[0]    = ROUND_UP(stride, stride_align) << stride_shift;
            buf.frame.data.planar.stride[1]    = ROUND_UP((stride + 1) >> 1, stride_align) << stride_shift;
            buf.frame.data.planar.stride[2]    = ROUND_UP((stride + 1) >> 1, stride_align) << stride_shift;
            if (buf.frame.data.planar.stride[0] * buffer_info->height >=
                ROUND_UP(buffer_info->height, stride_align) * buffer_info->width &&
                buf.frame.data.planar.stride[1] * ((buffer_info->height + 1) >> 1) >=
                ROUND_UP((buffer_info->height + 1) >> 1, stride_align) * ((buffer_info->width + 1) >> 1))
            {
                buf.frame.data.planar.stride_90[0] = ROUND_UP(buffer_info->height, stride_align) << stride_shift;
                buf.frame.data.planar.stride_90[1] = ROUND_UP((buffer_info->height + 1) >> 1, stride_align) << stride_shift;
                buf.frame.data.planar.stride_90[2] = ROUND_UP((buffer_info->height + 1) >> 1, stride_align) << stride_shift;
            }
            else
            {
                buf.frame.data.planar.stride_90[0] = buffer_info->height << stride_shift;
                buf.frame.data.planar.stride_90[1] = ((buffer_info->height + 1) >> 1) << stride_shift;
                buf.frame.data.planar.stride_90[2] = ((buffer_info->height + 1) >> 1) << stride_shift;
            }
            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;

            if (MVE_BASE_BUFFER_FORMAT_YV12 == buffer_info->format)
            {
                buf.frame.data.planar.plane_top[2] = buf.frame.data.planar.plane_top[0] +
                                                     buf.frame.data.planar.stride[0] * max_height;
                buf.frame.data.planar.plane_top[1] = buf.frame.data.planar.plane_top[2] +
                                                     buf.frame.data.planar.stride[2] * (max_height >> 1);
            }
            else /* if (MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR == buffer_info->format) */
            {
                buf.frame.data.planar.plane_top[1] = buf.frame.data.planar.plane_top[0] +
                                                     buf.frame.data.planar.stride[0] * max_height;
                buf.frame.data.planar.plane_top[2] = buf.frame.data.planar.plane_top[1] +
                                                     buf.frame.data.planar.stride[1] * (max_height >> 1);
            }

            if (interlace)
            {
                buf.frame.data.planar.plane_bot[0] =
                    buf.frame.data.planar.plane_top[0] + (buf.frame.data.planar.stride[0] >> 1);
                buf.frame.data.planar.plane_bot[1] =
                    buf.frame.data.planar.plane_top[1] + (buf.frame.data.planar.stride[1] >> 1);
                buf.frame.data.planar.plane_bot[2] =
                    buf.frame.data.planar.plane_top[2] + (buf.frame.data.planar.stride[2] >> 1);
            }
            else
            {
                buf.frame.data.planar.plane_bot[0] = 0;
                buf.frame.data.planar.plane_bot[1] = 0;
                buf.frame.data.planar.plane_bot[2] = 0;
            }
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_I420_10:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride / 2;

            /* The chroma and luma stride should be specified from gralloc eventually */
            buf.frame.data.planar.stride[0]    = ROUND_UP(stride, stride_align) << stride_shift;
            buf.frame.data.planar.stride[1]    = ROUND_UP((stride + 1) >> 1, stride_align) << stride_shift;
            buf.frame.data.planar.stride[2]    = ROUND_UP((stride + 1) >> 1, stride_align) << stride_shift;
            if (buf.frame.data.planar.stride[0] * buffer_info->height >=
                ROUND_UP(buffer_info->height * 2, stride_align) * buffer_info->width &&
                buf.frame.data.planar.stride[1] * ((buffer_info->height + 1) >> 1) >=
                ROUND_UP(buffer_info->height, stride_align) * ((buffer_info->width + 1) >> 1))
            {
                buf.frame.data.planar.stride_90[0] = ROUND_UP(buffer_info->height * 2, stride_align) << stride_shift;
                buf.frame.data.planar.stride_90[1] = ROUND_UP(buffer_info->height, stride_align) << stride_shift;
                buf.frame.data.planar.stride_90[2] = ROUND_UP(buffer_info->height, stride_align) << stride_shift;
            }
            else
            {
                buf.frame.data.planar.stride_90[0] = (buffer_info->height * 2) << stride_shift;
                buf.frame.data.planar.stride_90[1] = buffer_info->height << stride_shift;
                buf.frame.data.planar.stride_90[2] = buffer_info->height << stride_shift;
            }
            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;
            buf.frame.data.planar.plane_top[1] = buf.frame.data.planar.plane_top[0] +
                                                 buf.frame.data.planar.stride[0] * max_height;
            buf.frame.data.planar.plane_top[2] = buf.frame.data.planar.plane_top[1] +
                                                 buf.frame.data.planar.stride[1] * (max_height >> 1);
            /* 10-bit interlaced streams do not exist */
            buf.frame.data.planar.plane_bot[0] = 0;
            buf.frame.data.planar.plane_bot[1] = 0;
            buf.frame.data.planar.plane_bot[2] = 0;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR:
        case MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride;

            buf.frame.data.planar.stride[0]    = ROUND_UP(stride, stride_align) << stride_shift;
            buf.frame.data.planar.stride[1]    = ROUND_UP(stride, stride_align) << stride_shift;
            buf.frame.data.planar.stride[2]    = 0;
            if (buf.frame.data.planar.stride[0] * buffer_info->height >=
                ROUND_UP(buffer_info->height, stride_align) * buffer_info->width)
            {
                buf.frame.data.planar.stride_90[0] = ROUND_UP(buffer_info->height, stride_align) << stride_shift;
                buf.frame.data.planar.stride_90[1] = ROUND_UP(buffer_info->height, stride_align) << stride_shift;
            }
            else
            {
                buf.frame.data.planar.stride_90[0] = buffer_info->height << stride_shift;
                buf.frame.data.planar.stride_90[1] = buffer_info->height << stride_shift;
            }
            buf.frame.data.planar.stride_90[2] = 0;
            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;
            buf.frame.data.planar.plane_top[1] = buf.frame.data.planar.plane_top[0] +
                                                 buf.frame.data.planar.stride[0] * max_height;
            buf.frame.data.planar.plane_top[2] = 0;
            if (interlace)
            {
                buf.frame.data.planar.plane_bot[0] =
                    buf.frame.data.planar.plane_top[0] + (buf.frame.data.planar.stride[0] >> 1);
                buf.frame.data.planar.plane_bot[1] =
                    buf.frame.data.planar.plane_top[1] + (buf.frame.data.planar.stride[1] >> 1);
                buf.frame.data.planar.plane_bot[2] = 0;
            }
            else
            {
                buf.frame.data.planar.plane_bot[0] = 0;
                buf.frame.data.planar.plane_bot[1] = 0;
                buf.frame.data.planar.plane_bot[2] = 0;
            }
            break;
        case MVE_BASE_BUFFER_FORMAT_YUYYVY_10B:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride;

            buf.frame.data.planar.stride[0]    = ROUND_UP(stride * 4, stride_align) << stride_shift;
            buf.frame.data.planar.stride[1]    = 0;
            buf.frame.data.planar.stride[2]    = 0;
            if (buf.frame.data.planar.stride[0] * buffer_info->height >=
                ROUND_UP(buffer_info->height * 4, stride_align) * buffer_info->width)
            {
                buf.frame.data.planar.stride_90[0] = ROUND_UP(buffer_info->height * 4, stride_align);
            }
            else
            {
                buf.frame.data.planar.stride_90[0] = buffer_info->height * 4;
            }
            buf.frame.data.planar.stride_90[1] = 0;
            buf.frame.data.planar.stride_90[2] = 0;
            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;
            buf.frame.data.planar.plane_top[1] = 0;
            buf.frame.data.planar.plane_top[2] = 0;

            /* 10-bit interlaced streams do not exist */
            buf.frame.data.planar.plane_bot[0] = 0;
            buf.frame.data.planar.plane_bot[1] = 0;
            buf.frame.data.planar.plane_bot[2] = 0;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC: /* Intentional fallthrough */
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride;
            buf.frame.data.afbc.plane_top      = external_buffer->mapping.region.start + offset_in_page;

            if (interlace)
            {
                buf.frame.data.afbc.plane_bot  = buf.frame.data.afbc.plane_top +
                                                 (((buffer_info->size >> 1) + 31) & 0xffffffe0);
                buf.frame.data.afbc.alloc_bytes_top = buf.frame.data.afbc.plane_bot - buf.frame.data.afbc.plane_top;
                buf.frame.data.afbc.alloc_bytes_bot = buffer_info->size - buf.frame.data.afbc.alloc_bytes_top;
            }
            else
            {
                buf.frame.data.afbc.plane_bot  = 0;
                buf.frame.data.afbc.alloc_bytes_top = buffer_info->size;
                buf.frame.data.afbc.alloc_bytes_bot = 0;
            }
            buf.frame.data.afbc.cropx          = 0;
            buf.frame.data.afbc.cropy          = 0;
            buf.frame.data.afbc.y_offset       = 0;
            buf.frame.data.afbc.afbc_alloc_bytes = buffer_info->afbc_alloc_bytes;
            buf.frame.data.afbc.afbc_width_in_superblocks[0] = buffer_info->afbc_width_in_superblocks;
            buf.frame.data.afbc.afbc_width_in_superblocks[1] = (false == interlace) ? 0 : buffer_info->afbc_width_in_superblocks;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV422_1P:
        case MVE_BASE_BUFFER_FORMAT_YVU422_1P:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = buffer_info->stride;

            buf.frame.data.planar.stride[0]    = (stride * 2) << stride_shift;
            buf.frame.data.planar.stride[1]    = 0;
            buf.frame.data.planar.stride[2]    = 0;
            buf.frame.data.planar.stride_90[0] = 0;
            buf.frame.data.planar.stride_90[1] = 0;
            buf.frame.data.planar.stride_90[2] = 0;

            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;
            buf.frame.data.planar.plane_top[1] = 0;
            buf.frame.data.planar.plane_top[2] = 0;
            if (interlace)
            {
                buf.frame.data.planar.plane_bot[0] =
                    buf.frame.data.planar.plane_top[0] + (buf.frame.data.planar.stride[0] >> 1);
                buf.frame.data.planar.plane_bot[1] =
                    buf.frame.data.planar.plane_top[1] + (buf.frame.data.planar.stride[1] >> 1);
                buf.frame.data.planar.plane_bot[2] =
                    buf.frame.data.planar.plane_top[2] + (buf.frame.data.planar.stride[2] >> 1);
            }
            else
            {
                buf.frame.data.planar.plane_bot[0] = 0;
                buf.frame.data.planar.plane_bot[1] = 0;
                buf.frame.data.planar.plane_bot[2] = 0;
            }
            break;
        case MVE_BASE_BUFFER_FORMAT_RGBA_8888:
        case MVE_BASE_BUFFER_FORMAT_BGRA_8888:
        case MVE_BASE_BUFFER_FORMAT_ARGB_8888:
        case MVE_BASE_BUFFER_FORMAT_ABGR_8888:
            buf.frame.nHandle                  = buffer_client->mve_handle;
            buf.frame.format                   = buffer_info->format;
            buf.frame.nFlags                   = param->flags;
            buf.frame.nMVEFlags                = param->mve_flags;
            buf.frame.timestamp                = param->timestamp;
            buf.frame.decoded_height           = buffer_info->height;
            buf.frame.decoded_width            = buffer_info->width;
            buf.frame.max_height               = buffer_info->max_height;
            buf.frame.max_width                = ROUND_UP(stride, stride_align) / 4;

            buf.frame.data.planar.stride[0]    = ROUND_UP(stride, stride_align) << stride_shift;
            buf.frame.data.planar.stride[1]    = 0;
            buf.frame.data.planar.stride[2]    = 0;
            if (buf.frame.data.planar.stride[0] * buffer_info->height >=
                ROUND_UP(buffer_info->height, stride_align) * buffer_info->width)
            {
                buf.frame.data.planar.stride_90[0] = ROUND_UP(buffer_info->height, stride_align);
            }
            else
            {
                buf.frame.data.planar.stride_90[0] = buffer_info->height;
            }
            buf.frame.data.planar.stride_90[1] = 0;
            buf.frame.data.planar.stride_90[2] = 0;
            buf.frame.data.planar.plane_top[0] = external_buffer->mapping.region.start + offset_in_page;
            buf.frame.data.planar.plane_top[1] = 0;
            buf.frame.data.planar.plane_top[2] = 0;
            buf.frame.data.planar.plane_bot[0] = 0;
            buf.frame.data.planar.plane_bot[1] = 0;
            buf.frame.data.planar.plane_bot[2] = 0;
            break;
        default:
            /* Unsupported format */
            WARN_ON(true);
            buffer_client->mve_handle = MVE_HANDLE_INVALID;
            return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    /* Setup ROI if this feature is enabled */
    if (0 != (param->mve_flags & MVE_BASE_BUFFER_FRAME_FLAG_ROI) &&
        0 != param->nRegions)
    {
        /* Send ROI rectangles to the FW */
        mve_com_buffer roi_buf;
        roi_buf.roi.nRegions = param->nRegions;
        memcpy(roi_buf.roi.regions, param->regions, param->nRegions * sizeof(param->regions[0]));

        ret = mve_com_add_input_buffer5(session, &roi_buf, MVE_COM_BUFFER_TYPE_ROI);
        WARN_ON(ret != MVE_BASE_ERROR_NONE);
    }

    /* Setup CRC buffer and correct MVE flags */
    switch (buffer_info->format)
    {
        case MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR: /* Intentional fallthrough */
        case MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR:
        case MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR:
        case MVE_BASE_BUFFER_FORMAT_YUV420_I420_10:
        case MVE_BASE_BUFFER_FORMAT_YUYYVY_10B:
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC:
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B:
        case MVE_BASE_BUFFER_FORMAT_YUV422_1P:
        case MVE_BASE_BUFFER_FORMAT_YVU422_1P:
        case MVE_BASE_BUFFER_FORMAT_YV12:
        case MVE_BASE_BUFFER_FORMAT_RGBA_8888:
        case MVE_BASE_BUFFER_FORMAT_BGRA_8888:
        case MVE_BASE_BUFFER_FORMAT_ARGB_8888:
        case MVE_BASE_BUFFER_FORMAT_ABGR_8888:
            /* Adjust MVE flags to indicate if the buffer is empty or not. */
            if (0 == param->filled_len)
            {
                buf.frame.nMVEFlags &= ~(MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT);
            }
            else
            {
                buf.frame.nMVEFlags |= MVE_BUFFER_FRAME_FLAG_TOP_PRESENT;
            }

            if (NULL != buffer_client->crc)
            {
                if (0 != (param->mve_flags & MVE_BASE_BUFFER_FRAME_FLAG_CRC))
                {
                    if (MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT == buffer_client->crc->info.allocator)
                    {
                        buf.frame.crc_top = external_buffer->mapping.region.start + offset_in_page + param->crc_offset;
                    }
                    else
                    {
                        buf.frame.crc_top = buffer_client->crc->mapping.region.start +
                                            buffer_client->crc->mapping.offset_in_page;
                    }
                    buf.frame.crc_bot = 0;
                }
            }
            break;
        default:
            break;
    }

    /* Transfer ownership of the buffer from the CPU to the hardware. This
     * call handles cache management */
    if (false != buffer_info->do_cache_maintenance)
    {
        mve_buffer_set_owner5(buffer_client->buffer, MVE_BUFFER_OWNER_DEVICE, buffer_client->port_index);
        if (NULL != buffer_client->crc)
        {
            mve_buffer_set_owner5(buffer_client->crc, MVE_BUFFER_OWNER_DEVICE, buffer_client->port_index);
        }
    }

    if (false == empty_this_buffer)
    {
        ret = mve_com_add_output_buffer5(session, &buf, buffer_type);
    }
    else
    {
        ret = mve_com_add_input_buffer5(session, &buf, buffer_type);
    }

    if (MVE_BASE_ERROR_NONE != ret)
    {
        /* Failed to enqueue the buffer. */
        buffer_client->mve_handle = MVE_HANDLE_INVALID;
        WARN_ON(0 < buffer_client->in_use);
    }
    else
    {
        /* Success! The buffer is now owned by the MVE */
        buffer_client->in_use += 1;
    }

    return ret;
}

static uint32_t mve_session_buffer_get_afbc_size(uint32_t mbw,
                                                 uint32_t mbh,
                                                 uint32_t mb_size,
                                                 uint32_t payload_alignment,
                                                 uint32_t interlaced)
{
    unsigned size;

    if (interlaced)
    {
        mbh = (mbh + 1) >> 1;
    }
    size = mbw * (mbh + 0) * (16 + mb_size);
    size = (size + payload_alignment - 1) & ~(payload_alignment - 1);

    if (interlaced)
    {
        size <<= 1;
    }
    return size;
}

mve_base_error mve_session_buffer_dequeue_internal5(struct mve_session *session,
                                                   mve_base_buffer_handle_t buffer_id,
                                                   mve_com_buffer *buffer)
{
    struct mve_buffer_client *buffer_client;
    struct mve_buffer_info *buffer_info;

    buffer_client = mve_session_buffer_get_external_descriptor5(session, buffer_id);
    if (NULL == buffer_client)
    {
        /* The supplied user-space allocated buffer has not been registered
         * or is invalid. */
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    buffer_info = &buffer_client->buffer->info;
    /* Mark the buffer as no longer in use by the MVE */
    buffer_client->in_use -= 1;
    WARN_ON(0 > buffer_client->in_use);

    if (MVE_SESSION_TYPE_DECODER == session->session_type &&
        MVE_PORT_INDEX_OUTPUT == buffer_client->port_index)
    {
        session->buffer_on_hold_count = session->pending_buffer_on_hold_count;
    }

    if (MVE_BASE_BUFFER_FORMAT_BITSTREAM == buffer_info->format)
    {
        buffer_client->filled_len = buffer->bitstream.nFilledLen;
        buffer_client->offset     = buffer->bitstream.nOffset;
        buffer_client->flags      = buffer->bitstream.nFlags;
        buffer_client->timestamp  = buffer->bitstream.timestamp;
    }
    else if (MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YV12 == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR == buffer_info->format)
    {
        if (buffer->frame.nMVEFlags & (MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT))
        {
            buffer_client->filled_len = buffer->frame.decoded_width * buffer->frame.decoded_height * 3 / 2;
        }
        buffer_client->flags     = buffer->frame.nFlags;
        buffer_client->mve_flags = buffer->frame.nMVEFlags;
        buffer_client->pic_index = buffer->frame.pic_index;
        buffer_client->timestamp = buffer->frame.timestamp;
        buffer_client->decoded_width = buffer->frame.decoded_width;
        buffer_client->decoded_height = buffer->frame.decoded_height;
    }
    else if (MVE_BASE_BUFFER_FORMAT_YUYYVY_10B == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YUV422_1P == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YVU422_1P == buffer_info->format)
    {
        if (buffer->frame.nMVEFlags & (MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT))
        {
            buffer_client->filled_len = buffer->frame.decoded_width * buffer->frame.decoded_height * 2;
        }
        buffer_client->flags     = buffer->frame.nFlags;
        buffer_client->mve_flags = buffer->frame.nMVEFlags;
        buffer_client->pic_index = buffer->frame.pic_index;
        buffer_client->timestamp = buffer->frame.timestamp;
        buffer_client->decoded_width = buffer->frame.decoded_width;
        buffer_client->decoded_height = buffer->frame.decoded_height;
    }
    else if (MVE_BASE_BUFFER_FORMAT_YUV420_I420_10 == buffer_info->format)
    {
        if (buffer->frame.nMVEFlags & (MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT))
        {
            buffer_client->filled_len = buffer->frame.decoded_width * buffer->frame.decoded_height * 3;
        }
        buffer_client->flags     = buffer->frame.nFlags;
        buffer_client->mve_flags = buffer->frame.nMVEFlags;
        buffer_client->pic_index = buffer->frame.pic_index;
        buffer_client->timestamp = buffer->frame.timestamp;
        buffer_client->decoded_width = buffer->frame.decoded_width;
        buffer_client->decoded_height = buffer->frame.decoded_height;
    }
    else if (MVE_BASE_BUFFER_FORMAT_YUV420_AFBC == buffer_info->format ||
             MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B == buffer_info->format)
    {
        if (buffer->frame.nMVEFlags & (MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT))
        {
            if (0 == buffer_info->afbc_alloc_bytes)
            {
                uint32_t w = (buffer->frame.decoded_width + AFBC_MB_WIDTH - 1) / AFBC_MB_WIDTH;
                uint32_t h = (buffer->frame.decoded_height + AFBC_MB_WIDTH - 1) / AFBC_MB_WIDTH;
                uint32_t mbs;

                /* Magic formula for worstcase content */
                if (MVE_BASE_BUFFER_FORMAT_YUV420_AFBC == buffer_info->format)
                {
                    mbs = AFBC_MB_WIDTH * AFBC_MB_WIDTH * 3 / 2;
                }
                else
                {
                    mbs = AFBC_MB_WIDTH * AFBC_MB_WIDTH * 15 / 8;
                }

                buffer_client->filled_len = mve_session_buffer_get_afbc_size(w, h, mbs, 128, 0);
            }
            else
            {
                buffer_client->filled_len = buffer_info->afbc_alloc_bytes;
            }
        }
        /* If the bottom field is present and valid then the filled_len needs
         * to be extended.
         */
        if (buffer->frame.nMVEFlags & MVE_BUFFER_FRAME_FLAG_BOT_PRESENT)
        {
            unsigned long b = (unsigned long)buffer->frame.data.afbc.plane_bot;
            unsigned long t = (unsigned long)buffer->frame.data.afbc.plane_top;
            buffer_client->filled_len += (b - t);
        }
        buffer_client->flags     = buffer->frame.nFlags;
        buffer_client->mve_flags = buffer->frame.nMVEFlags;
        buffer_client->cropx     = buffer->frame.data.afbc.cropx;
        buffer_client->cropy     = buffer->frame.data.afbc.cropy;
        buffer_client->y_offset  = buffer->frame.data.afbc.y_offset;
        buffer_client->pic_index = buffer->frame.pic_index;
        buffer_client->timestamp = buffer->frame.timestamp;
        buffer_client->decoded_width = buffer->frame.decoded_width;
        buffer_client->decoded_height = buffer->frame.decoded_height;
    }

    if (false != buffer_info->do_cache_maintenance)
    {
        /* Transfer ownership of the buffer from the hardware to the CPU. This
         * call handles cache management */
        mve_buffer_set_owner5(buffer_client->buffer, MVE_BUFFER_OWNER_CPU, buffer_client->port_index);
        if (NULL != buffer_client->crc)
        {
            mve_buffer_set_owner5(buffer_client->crc, MVE_BUFFER_OWNER_CPU, buffer_client->port_index);
        }
    }

    return MVE_BASE_ERROR_NONE;
}

mve_base_error mve_session_buffer_notify_ref_frame_release_internal5(struct mve_session *session,
                                                                    mve_base_buffer_handle_t buffer_id)
{
    struct mve_buffer_client *buffer_client;
    struct mve_request_release_ref_frame data;

    buffer_client = mve_session_buffer_get_external_descriptor5(session, buffer_id);
    if (NULL == buffer_client)
    {
        /* The supplied buffer handle has not been registered
         * or is invalid. */
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    data.buffer_address = buffer_client->buffer->mapping.region.start +
                          buffer_client->buffer->mapping.offset_in_page;

    return mve_com_add_message5(session, MVE_REQUEST_CODE_RELEASE_REF_FRAME, sizeof(data), (uint32_t *)&data);
}

void mve_session_buffer_convert_to_userspace5(struct mve_base_buffer_userspace *dst,
                                             struct mve_buffer_client *src)
{
    memset(dst, 0, sizeof(struct mve_base_buffer_userspace));

    dst->handle           = src->buffer->info.handle;
    dst->buffer_id        = src->buffer->info.buffer_id;
    dst->allocator        = src->buffer->info.allocator;
    dst->size             = src->buffer->info.size;
    dst->width            = src->buffer->info.width;
    dst->height           = src->buffer->info.height;
    dst->stride           = src->buffer->info.stride;
    dst->stride_alignment = src->buffer->info.stride_alignment;
    dst->format           = src->buffer->info.format;

    dst->filled_len     = src->filled_len;
    dst->offset         = src->offset;
    dst->flags          = src->flags;
    dst->mve_flags      = src->mve_flags;
    dst->cropx          = src->cropx;
    dst->cropy          = src->cropy;
    dst->y_offset       = src->y_offset;
    dst->pic_index      = src->pic_index;
    dst->timestamp      = src->timestamp;
    dst->decoded_width  = src->decoded_width;
    dst->decoded_height = src->decoded_height;

    dst->afbc_width_in_superblocks = src->buffer->info.afbc_width_in_superblocks;
    dst->afbc_alloc_bytes = src->buffer->info.afbc_alloc_bytes;

    if (NULL != src->crc)
    {
        dst->crc_handle    = src->crc->info.handle;
        dst->crc_allocator = src->crc->info.allocator;
        dst->crc_size      = src->crc->info.size;
        dst->crc_offset    = src->crc->info.offset;
    }
}
