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

/****************************************************************************
 * Include v2 interface
 ****************************************************************************/

#include "host_interface_v2/mve_protocol_def.h"

/****************************************************************************
 * Includes
 ****************************************************************************/

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#include <linux/delay.h>
#endif

#include "mve_com_host_interface_v2.h"
#include "mve_com.h"
#include "mve_session.h"
#include "mve_rsrc_log.h"
#include "mve_rsrc_log_ram.h"
#include "mve_rsrc_mem_dma.h"
#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_register.h"

/****************************************************************************
 * Defines
 ****************************************************************************/
#define FW_VERSION_MINOR                 0
#define MVE_FORMAT_YUV420_I420_10        0x321

/****************************************************************************
 * Types
 ****************************************************************************/

struct mve_com_v2
{
    struct mve_com base;
    struct mve_response_frame_alloc_parameters alloc_params;
};

/* Virtual memory region map for V500, V550 and V61 hardware */
static const struct mve_mem_virt_region mem_regions[] =
{
    {   /* VIRT_MEM_REGION_FIRMWARE0 */
        MVE_MEM_REGION_FW_INSTANCE0_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE0_ADDR_END
    },
    #define MVE_COMM_QUEUE_SIZE 0x1000
    {   /* VIRT_MEM_REGION_MSG_IN_QUEUE */
        MVE_COMM_MSG_INQ_ADDR,
        MVE_COMM_MSG_INQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_MSG_OUT_QUEUE */
        MVE_COMM_MSG_OUTQ_ADDR,
        MVE_COMM_MSG_OUTQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_INPUT_BUFFER_IN */
        MVE_COMM_BUF_INQ_ADDR,
        MVE_COMM_BUF_INQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_INPUT_BUFFER_OUT */
        MVE_COMM_BUF_INRQ_ADDR,
        MVE_COMM_BUF_INRQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_OUTPUT_BUFFER_IN */
        MVE_COMM_BUF_OUTQ_ADDR,
        MVE_COMM_BUF_OUTQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_OUTPUT_BUFFER_OUT */
        MVE_COMM_BUF_OUTRQ_ADDR,
        MVE_COMM_BUF_OUTRQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_RPC_QUEUE */
        MVE_COMM_RPC_ADDR,
        MVE_COMM_RPC_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_PROTECTED */
        MVE_MEM_REGION_PROTECTED_ADDR_BEGIN,
        MVE_MEM_REGION_PROTECTED_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_OUT_BUF */
        MVE_MEM_REGION_FRAMEBUF_ADDR_BEGIN,
        MVE_MEM_REGION_FRAMEBUF_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE1 */
        MVE_MEM_REGION_FW_INSTANCE1_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE1_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE2 */
        MVE_MEM_REGION_FW_INSTANCE2_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE2_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE3 */
        MVE_MEM_REGION_FW_INSTANCE3_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE3_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE4 */
        MVE_MEM_REGION_FW_INSTANCE4_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE4_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE5 */
        MVE_MEM_REGION_FW_INSTANCE5_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE5_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE6 */
        MVE_MEM_REGION_FW_INSTANCE6_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE6_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE7 */
        MVE_MEM_REGION_FW_INSTANCE7_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE7_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_REGS */
        0xF0000000,
        0xFFFFFFFF,
    }
};
/****************************************************************************
 * Functions
 ****************************************************************************/

/**
 * Return true if port is of type frame, else return false (port is bitstream).
 */
static bool is_frame(struct mve_session *session,
                     bool direction_input)
{
    return (session->session_type == MVE_SESSION_TYPE_ENCODER) ? direction_input : !direction_input;
}

/**
 * Round up value. Rounding must be power of 2.
 */
#define ROUNDUP(v, r) (((v) + (r) - 1) & ~(r - 1))

/**
 * Log message.
 */
static void log_message(struct mve_session *session,
                        enum mve_log_fwif_channel channel,
                        enum mve_log_fwif_direction direction,
                        struct mve_msg_header *msg_header,
                        void *data)
{
    struct mve_log_header header;
    struct mve_log_fwif fwif;
    struct iovec vec[5];
    struct timespec64 timespec;
    int vecs = 4;
    int queued = -1;

    struct
    {
        struct mve_msg_header msg_header;
        struct mve_log_fwif_stat stat;
    }
    stat;

    //getnstimeofday(&timespec);
    ktime_get_ts64(&timespec);

    header.magic = MVE_LOG_MAGIC;
    header.length = sizeof(fwif) + sizeof(*msg_header) + ROUNDUP(msg_header->size, 4);
    header.type = MVE_LOG_TYPE_FWIF;
    header.severity = MVE_LOG_INFO;
    header.timestamp.sec = timespec.tv_sec;
    header.timestamp.nsec = timespec.tv_nsec;

    fwif.version_major = session->prot_ver;
    fwif.version_minor = FW_VERSION_MINOR;
    fwif.channel = channel;
    fwif.direction = direction;
    fwif.session = (uintptr_t)session;

    vec[0].iov_base = &header;
    vec[0].iov_len = sizeof(header);

    vec[1].iov_base = &fwif;
    vec[1].iov_len = sizeof(fwif);

    vec[2].iov_base = msg_header;
    vec[2].iov_len = sizeof(*msg_header);

    vec[3].iov_base = data;
    vec[3].iov_len = ROUNDUP(msg_header->size, 4);

    if (msg_header->code == MVE_BUFFER_CODE_FRAME || msg_header->code == MVE_BUFFER_CODE_BITSTREAM)
    {
        int change = direction == MVE_LOG_FWIF_DIRECTION_HOST_TO_FIRMWARE ? 1 : -1;
        switch (channel)
        {
            case MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER:
                queued = session->input_buffer_count + change;
                break;
            case MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER:
                queued = session->output_buffer_count + change;
                break;
            default:
                queued = -1;
        }
    }

    if (queued != -1)
    {
        stat.msg_header.code = MVE_LOG_FWIF_CODE_STAT;
        stat.msg_header.size = sizeof(stat.stat);
        stat.stat.handle = 0;
        stat.stat.queued = queued;

        vec[4].iov_base = &stat;
        vec[4].iov_len = ROUNDUP(sizeof(stat), 4);

        header.length += vec[4].iov_len;
        vecs = 5;
    }

    MVE_LOG_DATA(&mve_rsrc_log_fwif4, MVE_LOG_INFO, vec, vecs);
}

/**
 * Log RPC message.
 */
static void log_rpc(struct mve_session *session,
                    enum mve_log_fwif_direction direction,
                    struct mve_rpc_communication_area *rpc_data)
{
    struct mve_log_header header;
    struct mve_log_fwif fwif;
    struct iovec vec[3];
    struct timespec64 timespec;
    int vecs = 3;

    //getnstimeofday(&timespec);
    ktime_get_ts64(&timespec);

    header.magic = MVE_LOG_MAGIC;
    header.length = sizeof(fwif) + sizeof(*rpc_data);
    header.type = MVE_LOG_TYPE_FW_RPC;
    header.severity = MVE_LOG_INFO;
    header.timestamp.sec = timespec.tv_sec;
    header.timestamp.nsec = timespec.tv_nsec;

    fwif.version_major = session->prot_ver;
    fwif.version_minor = FW_VERSION_MINOR;
    fwif.channel = MVE_LOG_FWIF_CHANNEL_RPC;
    fwif.direction = direction;
    fwif.session = (uintptr_t)session;

    vec[0].iov_base = &header;
    vec[0].iov_len = sizeof(header);

    vec[1].iov_base = &fwif;
    vec[1].iov_len = sizeof(fwif);

    vec[2].iov_base = rpc_data;
    vec[2].iov_len = sizeof(*rpc_data);

    MVE_LOG_DATA(&mve_rsrc_log_fwif4, MVE_LOG_INFO, vec, vecs);
}

static uint32_t read_32(struct mve_comm_area_mve *mve_area,
                        uint32_t *rpos)
{
    uint32_t value;

    value = mve_area->out_data[*rpos];
    *rpos = (*rpos + 1) % MVE_COMM_QUEUE_SIZE_IN_WORDS;

    return value;
}

static mve_base_error read_from_queue_v2(struct mve_session *session,
                                         struct mve_rsrc_dma_mem_t *host,
                                         struct mve_rsrc_dma_mem_t *mve,
                                         enum mve_log_fwif_channel channel,
                                         struct mve_msg_header *msg_header,
                                         void *data,
                                         uint32_t size)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;
    struct mve_comm_area_host *host_area;
    struct mve_comm_area_mve *mve_area;
    uint32_t available;
    uint32_t rpos;
    uint32_t *d = data;

    host_area = mve_rsrc_dma_mem_map4(host);
    mve_area = mve_rsrc_dma_mem_map4(mve);

    if (host_area == NULL || mve_area == NULL)
    {
        ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
        goto out;
    }

    mve_rsrc_dma_mem_invalidate_cache4(mve);

    /* Check that there is enough data for the header. */
    if (host_area->out_rpos <= mve_area->out_wpos)
    {
        available = mve_area->out_wpos - host_area->out_rpos;
    }
    else
    {
        available = MVE_COMM_QUEUE_SIZE_IN_WORDS - (host_area->out_rpos - mve_area->out_wpos);
    }

    if (available == 0)
    {
        ret = MVE_BASE_ERROR_TIMEOUT;
        goto out;
    }

    rpos = host_area->out_rpos;

    /* Read header. */
    *(uint32_t *)msg_header = read_32(mve_area, &rpos);
    available--;

    /* Check that header size is not larger than available data. */
    if (((msg_header->size + 3) / 4) > available)
    {
        ret = MVE_BASE_ERROR_TIMEOUT;
        goto out;
    }

    /* Check that size is not larger that provided size. */
    if (msg_header->size > size)
    {
        ret = MVE_BASE_ERROR_TIMEOUT;
        goto out;
    }

    /* Read data. */
    size = (msg_header->size + 3) / 4;
    while (size-- > 0)
    {
        *d++ = read_32(mve_area, &rpos);
    }

    mve_rsrc_mem_flush_write_buffer4();
    /* Update read position. */
    host_area->out_rpos = rpos;
    mve_rsrc_mem_flush_write_buffer4();

    /* Update pending response counter. */
    switch (msg_header->code)
    {
        case MVE_RESPONSE_CODE_SET_OPTION_CONFIRM:
        case MVE_RESPONSE_CODE_SET_OPTION_FAIL:
        case MVE_RESPONSE_CODE_INPUT_FLUSHED:
        case MVE_RESPONSE_CODE_OUTPUT_FLUSHED:
        case MVE_RESPONSE_CODE_STATE_CHANGE:
            session->pending_response_count--;
            break;

        default:
            break;
    }

    /* Log firmware message. */
    MVE_LOG_EXECUTE(&mve_rsrc_log_fwif4, MVE_LOG_INFO,
                    log_message(session, channel, MVE_LOG_FWIF_DIRECTION_FIRMWARE_TO_HOST, msg_header, data));

out:
    mve_rsrc_dma_mem_clean_cache4(host);
    mve_rsrc_dma_mem_unmap4(host);
    mve_rsrc_dma_mem_unmap4(mve);

    return ret;
}

static void write_32(struct mve_comm_area_host *host_area,
                     uint32_t *wpos,
                     uint32_t value)
{
    host_area->in_data[*wpos] = value;
    *wpos = (*wpos + 1) % MVE_COMM_QUEUE_SIZE_IN_WORDS;
}

static mve_base_error wait_until_space_available(volatile uint16_t *rpos,
                                                 volatile uint16_t *wpos,
                                                 uint32_t words,
                                                 uint32_t queue_size)
{
#define ITERATION_TIMEOUT 100
    int i = 0;
    mve_base_error ret = MVE_BASE_ERROR_NONE;
    uint32_t free_words;

    if (words > queue_size)
    {
        return MVE_BASE_ERROR_TIMEOUT;
    }

    do
    {
        if (*rpos <= *wpos)
        {
            free_words = queue_size - *wpos + *rpos;
        }
        else
        {
            free_words = *rpos - *wpos;
        }
        i++;
        mb();
    }
    while (free_words < (words + 1) && i < ITERATION_TIMEOUT);

    /* Did we timeout? */
    if (i >= ITERATION_TIMEOUT)
    {
        ret = MVE_BASE_ERROR_TIMEOUT;
    }

    return ret;
#undef ITERATION_TIMEOUT
}

static mve_base_error write_to_queue_v2(struct mve_session *session,
                                        struct mve_rsrc_dma_mem_t *host,
                                        struct mve_rsrc_dma_mem_t *mve,
                                        enum mve_log_fwif_channel channel,
                                        uint32_t code,
                                        uint32_t size,
                                        void *data)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;
    struct mve_comm_area_host *host_area;
    struct mve_comm_area_mve *mve_area;
    struct mve_msg_header msg_header;
    uint32_t wpos;
    uint32_t *d = data;

    host_area = mve_rsrc_dma_mem_map4(host);
    mve_area = mve_rsrc_dma_mem_map4(mve);

    if ((host_area == NULL) || (mve_area == NULL))
    {
        ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
        goto out;
    }

    /* Set up header. */
    msg_header.code = code;
    msg_header.size = size;

    /* Add size of header and calculate number of 32-bit words. */
    size += sizeof(msg_header);
    size = (size + 3) / 4;

    /* Check that enough space is available in the buffer. */
    ret = wait_until_space_available(&mve_area->in_rpos,
                                     &host_area->in_wpos,
                                     size,
                                     MVE_COMM_QUEUE_SIZE_IN_WORDS);
    if (MVE_BASE_ERROR_NONE != ret)
    {
        goto out;
    }

    wpos = host_area->in_wpos;

    /* Write header. */
    write_32(host_area, &wpos, *((uint32_t *)&msg_header));
    size--;

    /* Write data. */
    while (size-- > 0)
    {
        write_32(host_area, &wpos, *d++);
    }

    mve_rsrc_mem_flush_write_buffer4();
    /* Set write position. */
    host_area->in_wpos = wpos;
    mve_rsrc_mem_flush_write_buffer4();

    /* Update pending response counter. */
    switch (code)
    {
        case MVE_REQUEST_CODE_GO:
        case MVE_REQUEST_CODE_STOP:
        case MVE_REQUEST_CODE_INPUT_FLUSH:
        case MVE_REQUEST_CODE_OUTPUT_FLUSH:
        case MVE_REQUEST_CODE_SET_OPTION:
            session->pending_response_count++;
            break;

        default:
            break;
    }

    /* Log firmware message. */
    MVE_LOG_EXECUTE(&mve_rsrc_log_fwif4, MVE_LOG_INFO,
                    log_message(session, channel, MVE_LOG_FWIF_DIRECTION_HOST_TO_FIRMWARE, &msg_header, data));

out:
    mve_rsrc_dma_mem_unmap4(host);
    mve_rsrc_dma_mem_unmap4(mve);

    return ret;
}

static uint32_t divide_by_power2_round_up(uint32_t n, uint32_t pow)
{
    return (n + (1 << pow) - 1) >> pow;
}

static mve_base_error write_buffer_frame(struct mve_session *session,
                                         struct mve_rsrc_dma_mem_t *host,
                                         struct mve_rsrc_dma_mem_t *mve,
                                         enum mve_log_fwif_channel channel,
                                         struct mve_com_buffer_frame *frame)
{
    mve_base_error ret;
    struct mve_buffer_frame mve_frame;
    uint32_t rotation;

    /* Convert com- to fw buffer. */
    mve_frame.host_handle = frame->nHandle;
    mve_frame.user_data_tag = frame->timestamp;

    mve_frame.frame_flags = frame->nMVEFlags & (MVE_BUFFER_FRAME_FLAG_INTERLACE | MVE_BUFFER_FRAME_FLAG_BOT_FIRST | MVE_BUFFER_FRAME_FLAG_TOP_PRESENT | MVE_BUFFER_FRAME_FLAG_BOT_PRESENT);
    mve_frame.frame_flags |= (frame->nFlags & MVE_BASE_BUFFERFLAG_EOS) ? MVE_BUFFER_FRAME_FLAG_EOS : 0;
    mve_frame.frame_flags |= (frame->nFlags & MVE_BASE_BUFFERFLAG_DECODEONLY) ? MVE_BUFFER_FRAME_FLAG_DECODE_ONLY : 0;
    mve_frame.frame_flags |= (frame->nFlags & MVE_BASE_BUFFERFLAG_DATACORRUPT) ? MVE_BUFFER_FRAME_FLAG_CORRUPT : 0;
    mve_frame.frame_flags |= (frame->nFlags & MVE_BASE_BUFFERFLAG_READONLY) ? MVE_BUFFER_FRAME_FLAG_REF_FRAME : 0;
    mve_frame.frame_flags |= (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_SCALING_2) ? MVE_BASE_BUFFER_FRAME_FLAG_SCALING_2 : 0;
    mve_frame.frame_flags |= (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_SCALING_4) ? MVE_BASE_BUFFER_FRAME_FLAG_SCALING_4 : 0;

    mve_frame.frame_flags |= (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_MASK);

    rotation = (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_MASK) >> 4;

    mve_frame.visible_frame_width = frame->decoded_width;
    mve_frame.visible_frame_height = frame->decoded_height;

    switch (frame->format)
    {
        case MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR:
            mve_frame.format = MVE_FORMAT_YUV420_I420;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR:
            mve_frame.format = MVE_FORMAT_YUV420_NV12;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUYYVY_10B:
            mve_frame.format = MVE_FORMAT_YUV420_Y0L2;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC:
            mve_frame.format = MVE_FORMAT_YUV420_AFBC_8;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B:
            mve_frame.format = MVE_FORMAT_YUV420_AFBC_10;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV422_1P:
            mve_frame.format = MVE_FORMAT_YUV422_YUY2;
            break;
        case MVE_BASE_BUFFER_FORMAT_YVU422_1P:
            mve_frame.format = MVE_FORMAT_YUV422_UYVY;
            break;
        case MVE_BASE_BUFFER_FORMAT_YV12:
            mve_frame.format = MVE_FORMAT_YUV420_I420;
            break;
        case MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR:
            mve_frame.format = MVE_FORMAT_YUV420_NV21;
            break;
        case MVE_BASE_BUFFER_FORMAT_YUV420_I420_10:
            mve_frame.format = MVE_FORMAT_YUV420_I420_10;
            break;
        case MVE_BASE_BUFFER_FORMAT_RGBA_8888:
            mve_frame.format = MVE_FORMAT_RGBA_8888;
            break;
        case MVE_BASE_BUFFER_FORMAT_BGRA_8888:
            mve_frame.format = MVE_FORMAT_BGRA_8888;
            break;
        case MVE_BASE_BUFFER_FORMAT_ARGB_8888:
            mve_frame.format = MVE_FORMAT_ARGB_8888;
            break;
        case MVE_BASE_BUFFER_FORMAT_ABGR_8888:
            mve_frame.format = MVE_FORMAT_ABGR_8888;
            break;
        default:
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_ERROR, session, "Illegal com buffer format. format=%u", frame->format);
            return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    switch (frame->format)
    {
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC:
        case MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B:
        {
            mve_frame.data.afbc.plane[0] = frame->data.afbc.plane_top;
            mve_frame.data.afbc.plane[1] = frame->data.afbc.plane_bot;
            mve_frame.data.afbc.alloc_bytes[0] = frame->data.afbc.alloc_bytes_top;
            mve_frame.data.afbc.alloc_bytes[1] = frame->data.afbc.alloc_bytes_bot;

            if (0 == frame->data.afbc.afbc_width_in_superblocks[0])
            {
                #define LOG2_32 (5)
                #define LOG2_16 (4)
                uint32_t pow2 = (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_AFBC_WIDEBLK) ? LOG2_32 : LOG2_16;
                #undef LOG2_32
                #undef LOG2_16
                bool interlace = (frame->nMVEFlags & MVE_BUFFER_FRAME_FLAG_INTERLACE) ? true : false;

                mve_frame.data.afbc.afbc_width_in_superblocks[0] = (false != interlace) ? 0 : divide_by_power2_round_up(mve_frame.visible_frame_width, pow2);
                mve_frame.data.afbc.afbc_width_in_superblocks[1] = 0;
            }
            else
            {
                mve_frame.data.afbc.afbc_width_in_superblocks[0] = frame->data.afbc.afbc_width_in_superblocks[0];
                mve_frame.data.afbc.afbc_width_in_superblocks[1] = frame->data.afbc.afbc_width_in_superblocks[1];
            }

            if (session->session_type == MVE_SESSION_TYPE_ENCODER)
            {
                mve_frame.data.afbc.cropx = 0;
                mve_frame.data.afbc.cropy = 0;
            }
            else
            {
                mve_frame.data.afbc.cropx = frame->data.afbc.cropx;
                mve_frame.data.afbc.cropy = frame->data.afbc.cropy + frame->data.afbc.y_offset;
            }
            mve_frame.data.afbc.afbc_params = 0;
            if (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_AFBC_TILED)
            {
                mve_frame.data.afbc.afbc_params |= MVE_BUFFER_FRAME_AFBC_TILED_BODY | MVE_BUFFER_FRAME_AFBC_TILED_HEADER;
            }
            if (frame->nMVEFlags & MVE_BASE_BUFFER_FRAME_FLAG_AFBC_WIDEBLK)
            {
                mve_frame.data.afbc.afbc_params |= MVE_BUFFER_FRAME_AFBC_32X8_SUPERBLOCK;
            }
            break;
        }

        default:
        {
            bool interlace = (frame->nMVEFlags & MVE_BUFFER_FRAME_FLAG_INTERLACE) ? true : false;

            mve_frame.data.planar.plane_top[0] = frame->data.planar.plane_top[0];
            mve_frame.data.planar.plane_top[1] = frame->data.planar.plane_top[1];
            mve_frame.data.planar.plane_top[2] = frame->data.planar.plane_top[2];

            mve_frame.data.planar.stride[0] = frame->data.planar.stride[0];
            mve_frame.data.planar.stride[1] = frame->data.planar.stride[1];
            mve_frame.data.planar.stride[2] = frame->data.planar.stride[2];

            mve_frame.data.planar.max_frame_width = frame->max_width;
            mve_frame.data.planar.max_frame_height = frame->max_height;

            if (MVE_SESSION_TYPE_DECODER ==  session->session_type)
            {
                /* stride alignment for rotation frame */
                if (1 == rotation || 3 == rotation)
                {
                    int max_height = frame->max_width;
                    if (true == interlace)
                    {
                        /*
                         * The implementation of interlaced stream rotation here depends on
                         * EGIL-2953 that FW is able to output yuv content according to
                         * the stride, plane_top and plane_bot settings below.
                         */
                        max_height >>= 1;
                    }

                    if (MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR == frame->format || MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR == frame->format)
                    {
                        mve_frame.data.planar.plane_top[1] = mve_frame.data.planar.plane_top[0] +
                                                             frame->data.planar.stride_90[0] * max_height;
                        mve_frame.data.planar.plane_top[2] = 0;
                    }
                    else if (MVE_BASE_BUFFER_FORMAT_YV12 == frame->format)
                    {
                        mve_frame.data.planar.plane_top[2] = mve_frame.data.planar.plane_top[0] +
                                                             frame->data.planar.stride_90[0] * max_height;
                        mve_frame.data.planar.plane_top[1] = mve_frame.data.planar.plane_top[2] +
                                                             frame->data.planar.stride_90[2] * (max_height >> 1);
                    }
                    else
                    {
                        mve_frame.data.planar.plane_top[1] = mve_frame.data.planar.plane_top[0] +
                                                             frame->data.planar.stride_90[0] * max_height;
                        mve_frame.data.planar.plane_top[2] = mve_frame.data.planar.plane_top[1] +
                                                             frame->data.planar.stride_90[1] * (max_height >> 1);
                    }

                    mve_frame.data.planar.stride[0] = frame->data.planar.stride_90[0];
                    mve_frame.data.planar.stride[1] = frame->data.planar.stride_90[1];
                    mve_frame.data.planar.stride[2] = frame->data.planar.stride_90[2];

                    mve_frame.visible_frame_width = frame->decoded_height;
                    mve_frame.visible_frame_height = frame->decoded_width;

                    mve_frame.data.planar.max_frame_width = frame->max_height;
                    mve_frame.data.planar.max_frame_height = frame->max_width;
                }
            }
            if (true == interlace)
            {
                if (1 == rotation || 3 == rotation)
                {
                    mve_frame.data.planar.plane_bot[0] = mve_frame.data.planar.plane_top[0] + (frame->data.planar.stride_90[0] >> 1);
                    mve_frame.data.planar.plane_bot[1] = mve_frame.data.planar.plane_top[1] + (frame->data.planar.stride_90[1] >> 1);
                    mve_frame.data.planar.plane_bot[2] = mve_frame.data.planar.plane_top[2] + (frame->data.planar.stride_90[2] >> 1);
                }
                else
                {
                    mve_frame.data.planar.plane_bot[0] = frame->data.planar.plane_bot[0];
                    mve_frame.data.planar.plane_bot[1] = frame->data.planar.plane_bot[1];
                    mve_frame.data.planar.plane_bot[2] = frame->data.planar.plane_bot[2];
                }
            }
            else
            {
                mve_frame.data.planar.plane_bot[0] = 0;
                mve_frame.data.planar.plane_bot[1] = 0;
                mve_frame.data.planar.plane_bot[2] = 0;
            }
            break;
        }
    }

    ret = write_to_queue_v2(session, host, mve, channel, MVE_BUFFER_CODE_FRAME, sizeof(mve_frame), &mve_frame);

    return ret;
}

static mve_base_error write_buffer_bitstream(struct mve_session *session,
                                             struct mve_rsrc_dma_mem_t *host,
                                             struct mve_rsrc_dma_mem_t *mve,
                                             enum mve_log_fwif_channel channel,
                                             struct mve_com_buffer_bitstream *bitstream)
{
    mve_base_error ret;
    struct mve_buffer_bitstream mve_bitstream;

    /* Convert com- to fw buffer. */
    mve_bitstream.host_handle = bitstream->nHandle;
    mve_bitstream.user_data_tag = bitstream->timestamp;
    mve_bitstream.bitstream_alloc_bytes = bitstream->nAllocLen;
    mve_bitstream.bitstream_offset = bitstream->nOffset;
    mve_bitstream.bitstream_filled_len = bitstream->nFilledLen;
    mve_bitstream.bitstream_buf_addr = bitstream->pBufferData;

    mve_bitstream.bitstream_flags = 0;
    mve_bitstream.bitstream_flags |= (bitstream->nFlags & MVE_BASE_BUFFERFLAG_EOS) ? MVE_BUFFER_BITSTREAM_FLAG_EOS : 0;
    mve_bitstream.bitstream_flags |= (bitstream->nFlags & MVE_BASE_BUFFERFLAG_ENDOFFRAME) ? MVE_BUFFER_BITSTREAM_FLAG_ENDOFFRAME : 0;
    mve_bitstream.bitstream_flags |= (bitstream->nFlags & MVE_BASE_BUFFERFLAG_SYNCFRAME) ? MVE_BUFFER_BITSTREAM_FLAG_SYNCFRAME : 0;
    mve_bitstream.bitstream_flags |= (bitstream->nFlags & MVE_BASE_BUFFERFLAG_CODECCONFIG) ? MVE_BUFFER_BITSTREAM_FLAG_CODECCONFIG : 0;
    mve_bitstream.bitstream_flags |= (bitstream->nFlags & MVE_BASE_BUFFERFLAG_ENDOFSUBFRAME) ? MVE_BUFFER_BITSTREAM_FLAG_ENDOFSUBFRAME : 0;

    ret = write_to_queue_v2(session, host, mve, channel, MVE_BUFFER_CODE_BITSTREAM, sizeof(mve_bitstream), &mve_bitstream);

    return ret;
}

static mve_base_error write_buffer_roi(struct mve_session *session,
                                       struct mve_rsrc_dma_mem_t *host,
                                       struct mve_rsrc_dma_mem_t *mve,
                                       enum mve_log_fwif_channel channel,
                                       struct mve_com_buffer_roi *roi)
{
    mve_base_error ret;
    struct mve_buffer_param mve_regions;
    int i;

    mve_regions.type = MVE_BUFFER_PARAM_TYPE_REGIONS;
    mve_regions.data.regions.n_regions = roi->nRegions;

    for (i = 0; i < roi->nRegions; ++i)
    {
        mve_regions.data.regions.region[i].mbx_left   = roi->regions[i].mbx_left;
        mve_regions.data.regions.region[i].mbx_right  = roi->regions[i].mbx_right;
        mve_regions.data.regions.region[i].mby_top    = roi->regions[i].mby_top;
        mve_regions.data.regions.region[i].mby_bottom = roi->regions[i].mby_bottom;
        mve_regions.data.regions.region[i].qp_delta   = roi->regions[i].qp_delta;
    }

    ret = write_to_queue_v2(session,
                            host,
                            mve,
                            channel,
                            MVE_BUFFER_CODE_PARAM,
                            sizeof(mve_regions),
                            &mve_regions);

    return ret;
}

static mve_base_error read_buffer_frame(struct mve_session *session,
                                        struct mve_rsrc_dma_mem_t *host,
                                        struct mve_rsrc_dma_mem_t *mve,
                                        enum mve_log_fwif_channel channel,
                                        struct mve_com_buffer_frame *frame)
{
    mve_base_error ret;
    struct mve_msg_header msg_header;
    struct mve_buffer_frame mve_frame;

    ret = read_from_queue_v2(session, host, mve, channel, &msg_header, &mve_frame, sizeof(mve_frame));
    if (ret != MVE_BASE_ERROR_NONE)
    {
        return ret;
    }

    /* Convert com- to fw buffer. */
    frame->nHandle = mve_frame.host_handle;
    frame->nMVEFlags = mve_frame.frame_flags;
    frame->timestamp = mve_frame.user_data_tag;
    frame->pic_index = 0;
    frame->decoded_height = mve_frame.visible_frame_height;
    frame->decoded_width = mve_frame.visible_frame_width;

    frame->nFlags = 0;
    frame->nFlags |= (mve_frame.frame_flags & MVE_BUFFER_FRAME_FLAG_EOS) ? MVE_BASE_BUFFERFLAG_EOS : 0;
    frame->nFlags |= (mve_frame.frame_flags & MVE_BUFFER_FRAME_FLAG_DECODE_ONLY) ? MVE_BASE_BUFFERFLAG_DECODEONLY : 0;
    frame->nFlags |= (mve_frame.frame_flags & MVE_BUFFER_FRAME_FLAG_CORRUPT) ? MVE_BASE_BUFFERFLAG_DATACORRUPT : 0;
    frame->nFlags |= (mve_frame.frame_flags & MVE_BUFFER_FRAME_FLAG_REF_FRAME) ? MVE_BASE_BUFFERFLAG_READONLY : 0;

    if ((mve_frame.format & MVE_FORMAT_BF_A) != 0)
    {
        frame->data.afbc.plane_top = mve_frame.data.afbc.plane[0];
        frame->data.afbc.plane_bot = mve_frame.data.afbc.plane[1];
        frame->data.afbc.alloc_bytes_top = mve_frame.data.afbc.alloc_bytes[0];
        frame->data.afbc.alloc_bytes_bot = mve_frame.data.afbc.alloc_bytes[1];
        frame->data.afbc.cropx = mve_frame.data.afbc.cropx;
        frame->data.afbc.cropy = mve_frame.data.afbc.cropy;
        frame->data.afbc.y_offset = 0;
        frame->data.afbc.rangemap = 0;
    }
    else
    {
        frame->data.planar.plane_top[0] = mve_frame.data.planar.plane_top[0];
        frame->data.planar.plane_top[1] = mve_frame.data.planar.plane_top[1];
        frame->data.planar.plane_top[2] = mve_frame.data.planar.plane_top[2];
        frame->data.planar.plane_bot[0] = mve_frame.data.planar.plane_bot[0];
        frame->data.planar.plane_bot[1] = mve_frame.data.planar.plane_bot[1];
        frame->data.planar.plane_bot[2] = mve_frame.data.planar.plane_bot[2];
        frame->data.planar.stride[0] = mve_frame.data.planar.stride[0];
        frame->data.planar.stride[1] = mve_frame.data.planar.stride[1];
        frame->data.planar.stride[2] = mve_frame.data.planar.stride[2];
        mve_frame.data.planar.max_frame_width = frame->decoded_width;
        mve_frame.data.planar.max_frame_height = frame->decoded_height;
        frame->crc_top = 0;
        frame->crc_bot = 0;
    }

    return ret;
}

static mve_base_error read_buffer_bitstream(struct mve_session *session,
                                            struct mve_rsrc_dma_mem_t *host,
                                            struct mve_rsrc_dma_mem_t *mve,
                                            enum mve_log_fwif_channel channel,
                                            struct mve_com_buffer_bitstream *bitstream)
{
    mve_base_error ret;
    struct mve_msg_header msg_header;
    struct mve_buffer_bitstream mve_bitstream;

    ret = read_from_queue_v2(session, host, mve, channel, &msg_header, &mve_bitstream, sizeof(mve_bitstream));
    if (ret != MVE_BASE_ERROR_NONE)
    {
        return ret;
    }

    /* Convert com- to fw buffer. */
    bitstream->nHandle = mve_bitstream.host_handle;
    bitstream->nFlags = mve_bitstream.bitstream_flags;
    bitstream->timestamp = mve_bitstream.user_data_tag;
    bitstream->nAllocLen = mve_bitstream.bitstream_alloc_bytes;
    bitstream->nOffset = mve_bitstream.bitstream_offset;
    bitstream->nFilledLen = mve_bitstream.bitstream_filled_len;
    bitstream->pBufferData = mve_bitstream.bitstream_buf_addr;

    bitstream->nFlags = 0;
    bitstream->nFlags |= (mve_bitstream.bitstream_flags & MVE_BUFFER_BITSTREAM_FLAG_EOS) ? MVE_BASE_BUFFERFLAG_EOS : 0;
    bitstream->nFlags |= (mve_bitstream.bitstream_flags & MVE_BUFFER_BITSTREAM_FLAG_ENDOFFRAME) ? MVE_BASE_BUFFERFLAG_ENDOFFRAME : 0;
    bitstream->nFlags |= (mve_bitstream.bitstream_flags & MVE_BUFFER_BITSTREAM_FLAG_SYNCFRAME) ? MVE_BASE_BUFFERFLAG_SYNCFRAME : 0;
    bitstream->nFlags |= (mve_bitstream.bitstream_flags & MVE_BUFFER_BITSTREAM_FLAG_CODECCONFIG) ? MVE_BASE_BUFFERFLAG_CODECCONFIG : 0;
    bitstream->nFlags |= (mve_bitstream.bitstream_flags & MVE_BUFFER_BITSTREAM_FLAG_ENDOFSUBFRAME) ? MVE_BASE_BUFFERFLAG_ENDOFSUBFRAME : 0;

    return ret;
}

static mve_base_error add_message(struct mve_session *session,
                                  uint16_t code,
                                  uint16_t size,
                                  uint32_t *data)
{
    mve_base_error ret;

    switch (code)
    {
        case MVE_REQUEST_CODE_GO:
        case MVE_REQUEST_CODE_STOP:
        case MVE_REQUEST_CODE_INPUT_FLUSH:
        case MVE_REQUEST_CODE_OUTPUT_FLUSH:
        case MVE_REQUEST_CODE_SWITCH:
        case MVE_REQUEST_CODE_PING:
        case MVE_REQUEST_CODE_DUMP:
        case MVE_REQUEST_CODE_IDLE_ACK:
        {
            ret = write_to_queue_v2(session, session->msg_in_queue, session->msg_out_queue, MVE_LOG_FWIF_CHANNEL_MESSAGE, code, 0, NULL);
            break;
        }
        case MVE_REQUEST_CODE_JOB:
        case MVE_REQUEST_CODE_SET_OPTION:
        case MVE_REQUEST_CODE_RELEASE_REF_FRAME:
        {
            ret = write_to_queue_v2(session, session->msg_in_queue, session->msg_out_queue, MVE_LOG_FWIF_CHANNEL_MESSAGE, code, size, data);
            break;
        }
        case MVE_BUFFER_CODE_PARAM:
        {
            ret = write_to_queue_v2(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER, code, size, data);
            break;
        }
        default:
        {
            ret = MVE_BASE_ERROR_BAD_PARAMETER;
            break;
        }
    }

    return ret;
}

static void copy_identical_message(struct mve_session *session,
                                   struct mve_msg_header *header,
                                   void **out,
                                   void *in,
                                   size_t size)
{
    /* Verify that size matches expected size. */
    if (header->size != size)
    {
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_WARNING, session, "Com message size did not match expected size. code=%u, size=%u, expected=%u.",
                              header->code, header->size, size);
        header->size = 0;
        return;
    }

    /* Allocate message structure. */
    *out = MVE_RSRC_MEM_CACHE_ALLOC(size, GFP_KERNEL);
    if (*out == NULL)
    {
        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_WARNING, session, "Failed to allocate memory for com message.");
        header->size = 0;
        return;
    }

    memcpy(*out, in, size);
}

static uint32_t *get_message(struct mve_session *session,
                             struct mve_msg_header *header)
{
    struct mve_com_v2 *com = (struct mve_com_v2 *)session->com;
    mve_base_error ret;
    union
    {
        uint32_t *ret;
        mve_base_error *stream_error;
        struct mve_response_switched_in *switched_in;
        struct mve_response_switched_out *switched_out;
        struct mve_response_error *error;
        struct mve_response_state_change *state_change;
    }
    response = { .ret = NULL };
    union
    {
        struct mve_response_switched_in switched_in;
        struct mve_response_switched_out switched_out;
        struct mve_response_state_change state_change;
        struct mve_response_job_dequeued job_dequeue;
        struct mve_response_error error;
        struct mve_response_frame_alloc_parameters alloc_parameters;
        struct mve_response_sequence_parameters sequence_parameters;
        struct mve_response_event event;
        struct mve_buffer_param buffer_param;
        struct mve_event_processed event_processed;
        struct mve_response_set_option_fail set_option_fail;
        struct mve_response_ref_frame_unused ref_frame_unused;
    }
    response_v2;

    ret = read_from_queue_v2(session, session->msg_in_queue, session->msg_out_queue, MVE_LOG_FWIF_CHANNEL_MESSAGE, header, &response_v2, sizeof(response_v2));
    if (ret != MVE_BASE_ERROR_NONE)
    {
        return NULL;
    }

    switch (header->code)
    {
        case MVE_RESPONSE_CODE_SWITCHED_IN:
        {
            copy_identical_message(session, header, (void **)&response.switched_in, &response_v2.switched_in, sizeof(response_v2.switched_in));
            break;
        }
        case MVE_RESPONSE_CODE_SWITCHED_OUT:
        {
            copy_identical_message(session, header, (void **)&response.switched_out, &response_v2.switched_out, sizeof(response_v2.switched_out));
            break;
        }
        case MVE_RESPONSE_CODE_SET_OPTION_CONFIRM:
        {
            header->size = sizeof(uint32_t);

            response.ret = MVE_RSRC_MEM_CACHE_ALLOC(header->size, GFP_KERNEL);
            if (response.ret == NULL)
            {
                ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                break;
            }
            *response.ret = MVE_BASE_ERROR_NONE;

            break;
        }
        case MVE_RESPONSE_CODE_SET_OPTION_FAIL:
        {
            /*
             * This is a pending response, so still need to process it as the normal response with error return code.
             */
            header->size = sizeof(uint32_t);

            response.ret = MVE_RSRC_MEM_CACHE_ALLOC(header->size, GFP_KERNEL);
            if (response.ret == NULL)
            {
                ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                break;
            }
            *response.ret = MVE_BASE_ERROR_FIRMWARE;
            break;
        }
        case MVE_RESPONSE_CODE_JOB_DEQUEUED:
        case MVE_RESPONSE_CODE_INPUT:
        case MVE_RESPONSE_CODE_OUTPUT:
        case MVE_RESPONSE_CODE_INPUT_FLUSHED:
        case MVE_RESPONSE_CODE_OUTPUT_FLUSHED:
        case MVE_RESPONSE_CODE_PONG:
        case MVE_RESPONSE_CODE_DUMP:
        case MVE_RESPONSE_CODE_IDLE:
        {
            header->size = 0;
            response.ret = MVE_RSRC_MEM_CACHE_ALLOC(0, GFP_KERNEL);
            break;
        }
        case MVE_RESPONSE_CODE_ERROR:
        {
            /*
             * Allocate is according to the size specified in the header, not the size of the
             * actual structure to match the size used in MVE_RSRC_MEM_CACHE_FREE later on.
             */
            copy_identical_message(session, header, (void **)&response.error, &response_v2.error, header->size);
            break;
        }
        case MVE_RESPONSE_CODE_STATE_CHANGE:
        {
            copy_identical_message(session, header, (void **)&response.state_change, &response_v2.state_change, sizeof(response_v2.state_change));
            break;
        }
        case MVE_RESPONSE_CODE_FRAME_ALLOC_PARAM:
        {
            struct mve_base_fw_frame_alloc_parameters *dst;
            struct mve_response_frame_alloc_parameters *src;

            header->size = sizeof(struct mve_base_fw_frame_alloc_parameters);

            response.ret = MVE_RSRC_MEM_CACHE_ALLOC(header->size, GFP_KERNEL);
            if (NULL == response.ret)
            {
                ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                break;
            }

            src = &response_v2.alloc_parameters;
            dst = (struct mve_base_fw_frame_alloc_parameters *)response.ret;

            dst->planar_alloc_frame_width             = src->planar_alloc_frame_width;
            dst->planar_alloc_frame_height            = src->planar_alloc_frame_height;
            dst->afbc_alloc_bytes                     = src->afbc_alloc_bytes;
            dst->afbc_alloc_bytes_downscaled          = src->afbc_alloc_bytes_downscaled;
            dst->afbc_width_in_superblocks            = src->afbc_width_in_superblocks;
            dst->afbc_width_in_superblocks_downscaled = src->afbc_width_in_superblocks_downscaled;
            dst->cropx                                = src->cropx;
            dst->cropy                                = src->cropy;
            dst->mbinfo_alloc_bytes                   = src->mbinfo_alloc_bytes;
            /* Need to store the alloc parameters here as well because they are needed
             * when calculating filled_len */
            com->alloc_params = response_v2.alloc_parameters;
            break;
        }
        case MVE_RESPONSE_CODE_SEQUENCE_PARAMETERS:
        {
            copy_identical_message(session, header, (void **)&response.ret, &response_v2.sequence_parameters, sizeof(response_v2.sequence_parameters));
            break;
        }
        case MVE_BUFFER_CODE_PARAM:
        {
            copy_identical_message(session, header, (void **)&response.ret, &response_v2.buffer_param, header->size);
            break;
        }
        case MVE_RESPONSE_CODE_EVENT:
        {
            header->code = MVE_RESPONSE_CODE_EVENT;

            switch (response_v2.event.event_code)
            {
                case MVE_EVENT_ERROR_STREAM_CORRUPT:
                case MVE_EVENT_ERROR_STREAM_NOT_SUPPORTED:
                {
                    header->code = MVE_RESPONSE_CODE_STREAM_ERROR;
                    header->size = sizeof(*response.stream_error);
                    response.stream_error = MVE_RSRC_MEM_CACHE_ALLOC(header->size, GFP_KERNEL);
                    *response.stream_error = response_v2.event.event_code == MVE_EVENT_ERROR_STREAM_CORRUPT ?
                                             MVE_BASE_ERROR_STREAM_CORRUPT : MVE_BASE_ERROR_STREAM_NOTSUPPORTED;
                    break;
                }
                case MVE_EVENT_PROCESSED:
                {
                    header->code = MVE_RESPONSE_CODE_EVENT_PROCESSED;
                    header->size -= sizeof(response_v2.event.event_code);

                    if (header->size == sizeof(response_v2.event.event_data.event_processed))
                    {
                        copy_identical_message(session, header, (void **)&response.ret, &response_v2.event.event_data.event_processed,
                                               sizeof(response_v2.event.event_data.event_processed));
                    }
                    else if (0 == header->size)
                    {
                        response.ret = MVE_RSRC_MEM_CACHE_ALLOC(0, GFP_KERNEL);
                    }
                    else
                    {
                        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_ERROR, session, "Com event processed message should have size 0 or %u. size=%u.",
                                              sizeof(response_v2.event.event_data.event_processed), header->size);
                    }
                    break;
                }
                case MVE_EVENT_REF_FRAME:
                {
                    header->size = 0;
                    response.ret = MVE_RSRC_MEM_CACHE_ALLOC(0, GFP_KERNEL);
                    break;
                }
                case MVE_EVENT_TRACE_BUFFERS:
                {
                    header->code = MVE_RESPONSE_CODE_TRACE_BUFFERS;
                    header->size -= sizeof(response_v2.event.event_code);

                    copy_identical_message(session, header, (void **)&response.ret, &response_v2.event.event_data.event_trace_buffers,
                                           sizeof(response_v2.event.event_data.event_trace_buffers));
                    break;
                }
                default:
                {
                    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_ERROR, session, "Received com message with illegal event code. code=%u.", response_v2.event.event_code);
                    break;
                }
            }
            break;
        }
        case MVE_RESPONSE_CODE_REF_FRAME_UNUSED:
        {
            copy_identical_message(session, header, (void **)&response.ret, &response_v2.ref_frame_unused, sizeof(response_v2.ref_frame_unused));
            break;
        }
        default:
        {
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session4, MVE_LOG_ERROR, session, "Received com message with illegal code. code=%u.", header->code);
            break;
        }
    }

    return response.ret;
}

static mve_base_error add_input_buffer(struct mve_session *session,
                                       mve_com_buffer *buffer,
                                       enum mve_com_buffer_type type)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    switch (type)
    {
        case MVE_COM_BUFFER_TYPE_FRAME:
            ret = write_buffer_frame(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->frame);
            break;
        case MVE_COM_BUFFER_TYPE_BITSTREAM:
            ret = write_buffer_bitstream(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->bitstream);
            break;
        case MVE_COM_BUFFER_TYPE_ROI:
            ret = write_buffer_roi(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->roi);
            break;
        default:
            WARN_ON(true);
            ret = MVE_BASE_ERROR_BAD_PARAMETER;
            break;
    }

    return ret;
}

static mve_base_error add_output_buffer(struct mve_session *session,
                                        mve_com_buffer *buffer,
                                        enum mve_com_buffer_type type)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    switch (type)
    {
        case MVE_COM_BUFFER_TYPE_FRAME:
            ret = write_buffer_frame(session, session->buf_output_in, session->buf_output_out, MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER, &buffer->frame);
            break;
        case MVE_COM_BUFFER_TYPE_BITSTREAM:
            ret = write_buffer_bitstream(session, session->buf_output_in, session->buf_output_out, MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER, &buffer->bitstream);
            break;
        case MVE_COM_BUFFER_TYPE_ROI: /* Intentional fall-through */
        default:
            WARN_ON(true);            /* Should never end up here */
            ret = MVE_BASE_ERROR_BAD_PARAMETER;
            break;
    }

    return ret;
}

static mve_base_error get_input_buffer(struct mve_session *session,
                                       mve_com_buffer *buffer)
{
    mve_base_error ret;

    if (is_frame(session, true) == true)
    {
        ret = read_buffer_frame(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->frame);
    }
    else
    {
        ret = read_buffer_bitstream(session, session->buf_input_in, session->buf_input_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->bitstream);
    }

    return ret;
}

static mve_base_error get_output_buffer(struct mve_session *session,
                                        mve_com_buffer *buffer)
{
    mve_base_error ret;

    if (is_frame(session, false) == true)
    {
        ret = read_buffer_frame(session, session->buf_output_in, session->buf_output_out, MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER, &buffer->frame);
    }
    else
    {
        ret = read_buffer_bitstream(session, session->buf_output_in, session->buf_output_out, MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER, &buffer->bitstream);
    }

    return ret;
}

static mve_base_error get_rpc_message(struct mve_session *session,
                                      mve_com_rpc *rpc)
{
    struct mve_rpc_communication_area *rpc_area;
    mve_base_error err = MVE_BASE_ERROR_NOT_READY;

    rpc_area = mve_rsrc_dma_mem_map4(session->rpc_area);
    if (NULL == rpc_area)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    mve_rsrc_dma_mem_invalidate_cache4(session->rpc_area);

    if (MVE_RPC_STATE_PARAM == rpc_area->state)
    {
        /* Copy RPC details to the client supplied structure */
        rpc->state   = rpc_area->state;
        rpc->call_id = rpc_area->call_id;
        rpc->size    = rpc_area->size;

        switch (rpc_area->call_id)
        {
            case MVE_RPC_FUNCTION_DEBUG_PRINTF:
                memcpy(rpc->params.debug_print.string,
                       rpc_area->params.debug_print.string,
                       MVE_RPC_DATA_SIZE_IN_WORDS * 4);
                mve_rsrc_log_trim(rpc_area->params.debug_print.string);
                break;
            case MVE_RPC_FUNCTION_MEM_ALLOC:
                rpc->params.mem_alloc.size           = rpc_area->params.mem_alloc.size;
                rpc->params.mem_alloc.max_size       = rpc_area->params.mem_alloc.max_size;
                rpc->params.mem_alloc.region         = rpc_area->params.mem_alloc.region;
                rpc->params.mem_alloc.log2_alignment = rpc_area->params.mem_alloc.log2_alignment;
                break;
            case MVE_RPC_FUNCTION_MEM_RESIZE:
                rpc->params.mem_resize.ve_pointer = rpc_area->params.mem_resize.ve_pointer;
                rpc->params.mem_resize.new_size   = rpc_area->params.mem_resize.new_size;
                break;
            case MVE_RPC_FUNCTION_MEM_FREE:
                rpc->params.mem_free.ve_pointer = rpc_area->params.mem_free.ve_pointer;
                break;
        }

        MVE_LOG_EXECUTE(&mve_rsrc_log_fwif4, MVE_LOG_INFO,
                        log_rpc(session, MVE_LOG_FWIF_DIRECTION_FIRMWARE_TO_HOST, rpc_area));
        err = MVE_BASE_ERROR_NONE;
    }

    mve_rsrc_dma_mem_unmap4(session->rpc_area);

    return err;
}

static mve_base_error put_rpc_message(struct mve_session *session,
                                      mve_com_rpc *rpc)
{
    struct mve_rpc_communication_area *rpc_area;

    rpc_area = mve_rsrc_dma_mem_map4(session->rpc_area);
    if (NULL == rpc_area)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    /* Copy RPC details to the client supplied structure */
    switch (rpc->call_id)
    {
        case MVE_COM_RPC_FUNCTION_DEBUG_PRINTF:
            break;
        case MVE_COM_RPC_FUNCTION_MEM_ALLOC:
            rpc_area->params.data[0] = rpc->params.data[0];
            break;
        case MVE_COM_RPC_FUNCTION_MEM_RESIZE:
            rpc_area->params.data[0] = rpc->params.data[0];
            break;
        case MVE_COM_RPC_FUNCTION_MEM_FREE:
            break;
    }

    rpc_area->call_id = rpc->call_id;
    rpc_area->size    = rpc->size;
    wmb();
    rpc_area->state   = rpc->state;

    wmb();
    mve_rsrc_dma_mem_clean_cache4(session->rpc_area);

    MVE_LOG_EXECUTE(&mve_rsrc_log_fwif4, MVE_LOG_INFO,
                    log_rpc(session, MVE_LOG_FWIF_DIRECTION_HOST_TO_FIRMWARE, rpc_area));

    mve_rsrc_dma_mem_unmap4(session->rpc_area);

    return MVE_BASE_ERROR_NONE;
}

static void mve_com_host_interface_v2_construct(struct mve_com_v2 *com)
{
    memset(com, 0, sizeof(*com));

    com->base.host_interface.prot_ver = MVE_BASE_FW_MAJOR_PROT_V2;
    com->base.host_interface.add_message = add_message;
    com->base.host_interface.get_message = get_message;
    com->base.host_interface.add_input_buffer = add_input_buffer;
    com->base.host_interface.add_output_buffer = add_output_buffer;
    com->base.host_interface.get_input_buffer = get_input_buffer;
    com->base.host_interface.get_output_buffer = get_output_buffer;
    com->base.host_interface.get_rpc_message = get_rpc_message;
    com->base.host_interface.put_rpc_message = put_rpc_message;
}

struct mve_com *mve_com_host_interface_v2_new4(void)
{
    struct mve_com_v2 *com;

    /* Allocate com object. */
    com = MVE_RSRC_MEM_ZALLOC(sizeof(*com), GFP_KERNEL);
    if (com == NULL)
    {
        MVE_LOG_PRINT(&mve_rsrc_log4, MVE_LOG_WARNING, "Failed to allocate com object.");
        return NULL;
    }

    /* Run constructor. */
    mve_com_host_interface_v2_construct(com);

    return &com->base;
}

const struct mve_mem_virt_region *mve_com_hi_v2_get_mem_virt_region4(void)
{
    return mem_regions;
}
