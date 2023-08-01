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

#ifndef MVE_COM_H
#define MVE_COM_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

#include "mve_base.h"
#include "mve_mem_region.h"
#include "mve_fw.h"

/* Additional response codes for events collated as one in HIv2 */
#define MVE_RESPONSE_CODE_TRACE_BUFFERS   2999
#define MVE_RESPONSE_CODE_STREAM_ERROR    2998
#define MVE_RESPONSE_CODE_EVENT_PROCESSED 2997

/* Forward declarations */
struct mve_session;
struct mve_msg_header;

/* The buffer representation (MVE_BUFFER) for the different host interface
 * versions can differ a lot. The following structures are therefore used by
 * the driver and converted to the correct format by the interface implementations.
 */
struct mve_com_buffer_planar
{
    int16_t stride[3];          /* Stride between rows for 0 and 180 deg rotation */
    int16_t stride_90[3];       /* Stride between rows for 90 and 270 deg rotation */
    uint32_t plane_top[3];      /* Y,Cb,Cr top field */
    uint32_t plane_bot[3];      /* Y,Cb,Cr bottom field (interlace only) */
};

struct mve_com_buffer_afbc
{
    uint32_t plane_top;                    /* Top field (interlace) or frame (progressive) */
    uint32_t plane_bot;                    /* Bottom field (interlace only) */
    uint16_t cropx;                        /* Luma x crop */
    uint16_t cropy;                        /* Luma y crop */
    uint8_t y_offset;                      /* Deblocking y offset of picture */
    uint8_t rangemap;                      /* Packed VC-1 Luma and Chroma range map coefs */
    uint32_t alloc_bytes_top;              /* Buffer size for top field (interlace) or frame (progressive) */
    uint32_t alloc_bytes_bot;              /* Buffer size for bottom field (interlace only) */
    uint32_t afbc_alloc_bytes;             /* AFBC buffer size requested by the firmware */
    uint32_t afbc_width_in_superblocks[2]; /* Width in superblocks */
};

struct mve_com_buffer_frame
{
    uint16_t nHandle;                       /* Host buffer handle number / ** WARNING: struct mve_core_buffer_header relies on having the same nHandle position as MVE_BUFFER ** / */
    enum mve_base_buffer_format format;     /* Buffer format. */
    uint32_t pic_index;                     /* Picture index in decode order */
    uint16_t decoded_height;                /* Decoded height may be smaller than display height */
    uint16_t decoded_width;                 /* Decoded width may be smaller than display width */
    uint16_t max_height;                    /* Max height of frame that the buffer can accommodate */
    uint16_t max_width;                     /* Max width of frame that the buffer can accommodate */
    uint32_t nFlags;                        /* OMX BufferFlags */
    uint32_t nMVEFlags;                     /* MVE sideband information; */
    uint32_t nStreamStartOffset;            /* Start of picture stream byte offset */
    uint32_t nStreamEndOffset;              /* End of picture stream byte offset */
    union
    {
        struct mve_com_buffer_planar planar;
        struct mve_com_buffer_afbc afbc;
    }
    data;
    /* Below fields are valid since Host interface spec v0.1 */
    uint32_t crc_top;            /* CRC map address top field or frame */
    uint32_t crc_bot;            /* CRC map bottom field */

    uint64_t timestamp;          /* Host supplied buffer timestamp */
};

struct mve_com_buffer_bitstream
{
    uint16_t nHandle;            /* Host buffer handle number / ** WARNING: struct mve_core_buffer_header relies on having the same nHandle position as MVE_BUFFER ** / */
    uint32_t pBufferData;        /* Buffer start */
    uint32_t nAllocLen;          /* Length of allocated buffer */
    uint32_t nFilledLen;         /* Number of bytes in the buffer */
    uint32_t nOffset;            /* Byte offset from start to first byte */
    uint32_t nFlags;             /* OMX BufferFlags */
    uint64_t timestamp;          /* Host supplied buffer timestamp */
};

#define MAX_ROI_REGIONS 16
struct mve_com_buffer_roi
{
    uint8_t nRegions;
    struct mve_base_roi_region regions[MAX_ROI_REGIONS];
};

typedef union mve_com_buffer
{
    struct mve_com_buffer_frame frame;
    struct mve_com_buffer_bitstream bitstream;
    struct mve_com_buffer_roi roi;
} mve_com_buffer;

enum mve_com_buffer_type
{
    MVE_COM_BUFFER_TYPE_FRAME,
    MVE_COM_BUFFER_TYPE_BITSTREAM,
    MVE_COM_BUFFER_TYPE_ROI
};

#define MVE_COM_RPC_AREA_SIZE_IN_WORDS 256
#define MVE_COM_RPC_DATA_SIZE_IN_WORDS (MVE_COM_RPC_AREA_SIZE_IN_WORDS - 3)
union mve_com_rpc_params
{
    volatile uint32_t data[MVE_COM_RPC_DATA_SIZE_IN_WORDS];
    struct
    {
        char string[MVE_COM_RPC_DATA_SIZE_IN_WORDS * 4];
    }
    debug_print;
    struct
    {
        uint32_t size;
        uint32_t max_size;
        enum mve_base_memory_region region; /* Memory region selection */
        /* The newly allocated memory must be placed
         * on (at least) a 2^(log2_alignment) boundary
         */
        uint8_t log2_alignment;
    }
    mem_alloc;
    struct
    {
        uint32_t ve_pointer;
        uint32_t new_size;
    }
    mem_resize;
    struct
    {
        uint32_t ve_pointer;
    }
    mem_free;
};

typedef struct mve_com_rpc
{
    volatile uint32_t state;
        #define MVE_COM_RPC_STATE_FREE   (0)
        #define MVE_COM_RPC_STATE_PARAM  (1)
        #define MVE_COM_RPC_STATE_RETURN (2)
    volatile uint32_t call_id;
        #define MVE_COM_RPC_FUNCTION_DEBUG_PRINTF (1)
        #define MVE_COM_RPC_FUNCTION_MEM_ALLOC    (2)
        #define MVE_COM_RPC_FUNCTION_MEM_RESIZE   (3)
        #define MVE_COM_RPC_FUNCTION_MEM_FREE     (4)
    volatile uint32_t size;
    union mve_com_rpc_params params;
} mve_com_rpc;

struct mve_com_error
{
    uint32_t error_code;
    char message[128];
};

struct mve_com_trace_buffers
{
    uint16_t reserved;
    uint8_t num_cores;
    uint8_t rasc_mask;
#define MVE_MAX_TRACE_BUFFERS 40
    /* this array will contain one buffer per rasc in rasc_mask per num_core */
    struct
    {
        uint32_t rasc_addr; /* rasc address of the buffer */
        uint32_t size;      /* size of the buffer in bytes */
    }
    buffers[MVE_MAX_TRACE_BUFFERS];
};

/**
 * Host interface function pointers.
 */
struct mve_com_host_interface
{
    enum mve_base_fw_major_prot_ver prot_ver;
    mve_base_error (*add_message)(struct mve_session *session, uint16_t code, uint16_t size, uint32_t *data);
    uint32_t *(*get_message)(struct mve_session *session, struct mve_msg_header *header);
    mve_base_error (*add_input_buffer)(struct mve_session *session, mve_com_buffer *buffer, enum mve_com_buffer_type type);
    mve_base_error (*add_output_buffer)(struct mve_session *session, mve_com_buffer *buffer, enum mve_com_buffer_type type);
    mve_base_error (*get_input_buffer)(struct mve_session *session, mve_com_buffer *buffer);
    mve_base_error (*get_output_buffer)(struct mve_session *session, mve_com_buffer *buffer);
    mve_base_error (*get_rpc_message)(struct mve_session *session, mve_com_rpc *rpc);
    mve_base_error (*put_rpc_message)(struct mve_session *session, mve_com_rpc *rpc);
};

/**
 * Host interface base class.
 */
struct mve_com
{
    struct mve_com_host_interface host_interface;
};

/**
 * Factory function. Set the interface version and creates the com class instance.
 *
 * @param session   Pointer to session object.
 * @param version   Protocol version.
 * @return MVE_BASE_ERROR_NONE or success, else error code.
 */
mve_base_error mve_com_set_interface_version5(struct mve_session *session,
                                             enum mve_base_fw_major_prot_ver version);

/**
 * Delete com object.
 *
 * @param session   Pointer to session object.
 */
void mve_com_delete5(struct mve_session *session);

/**
 * Enqueues a message to the MVE in-queue. Note that this function does not
 * notify MVE that a message has been added.
 * @param session Pointer to the session which wants to send a message
 * @param code    Message code.
 * @param size    Size of the attached data.
 * @param data    Pointer to the message data.
 * @return MVE_BASE_ERROR_NONE if successful, otherwise a suitable error code.
 */
mve_base_error mve_com_add_message5(struct mve_session *session,
                                   uint16_t code,
                                   uint16_t size,
                                   uint32_t *data);

/**
 * Retrieves a message sent from the MVE to the host. Note that the client
 * must free the returned data to avoid memory leaks.
 * @param session Pointer to the session that is sent the message.
 * @param header  Pointer to a data area which will receive the message header.
 * @param The data associated to the message. NULL on failure. This pointer must
 *        be freed by the client.
 */
uint32_t *mve_com_get_message5(struct mve_session *session,
                              struct mve_msg_header *header);

/**
 * Enqueues an input buffer to the MVE in buffer queue. Note that this function
 * does not notify MVE that a buffer has been added.
 * @param session Pointer to the session which wants to add an input buffer.
 * @param buffer  Pointer to the MVE buffer to add.
 * @param type    Buffer type.
 * @return MVE_BASE_ERROR_NONE if the buffer was added successfully. Error code otherwise.
 */
mve_base_error mve_com_add_input_buffer5(struct mve_session *session,
                                        mve_com_buffer *buffer,
                                        enum mve_com_buffer_type type);

/**
 * Enqueues an output buffer to the MVE out buffer queue. Note that this function
 * does not notify MVE that a buffer has been added and does not clean the CPU
 * cache.
 * @param session Pointer to the session which wants to add an output buffer.
 * @param buffer  Pointer to the MVE buffer to add to the queue.
 * @param type    Buffer type.
 * @return MVE_BASE_ERROR_NONE if the buffer was added successfully. Error code otherwise.
 */
mve_base_error mve_com_add_output_buffer5(struct mve_session *session,
                                         mve_com_buffer *buffer,
                                         enum mve_com_buffer_type type);

/**
 * Retreives an input buffer that the MVE has returned to the host.
 * @param session Pointer to the session to retrieve the input buffer.
 * @param buffer  The returned buffer will be copied into this buffer.
 * @return MVE_BASE_ERROR_NONE if buffer contains a valid buffer. Error code otherwise.
 */
mve_base_error mve_com_get_input_buffer5(struct mve_session *session,
                                        mve_com_buffer *buffer);

/**
 * Retreives an output buffer that the MVE has returned to the host.
 * @param session Pointer to the session to retrieve the output buffer.
 * @param buffer  The returned buffer will be copied into this buffer.
 * @return MVE_BASE_ERROR_NONE if buffer contains a valid buffer. Error code otherwise.
 */
mve_base_error mve_com_get_output_buffer5(struct mve_session *session,
                                         mve_com_buffer *buffer);

/**
 * Retreives a RPC request from MVE.
 * @param session Pointer to the session to retreive the RPC.
 * @param rpc The details about the RPC call will be placed in this instance.
 * @return MVE_BASE_ERROR_NONE if rpc contains valid data. Error code otherwise.
 */
mve_base_error mve_com_get_rpc_message5(struct mve_session *session,
                                       mve_com_rpc *rpc);

/**
 * Stores the response to a RPC request from MVE.
 * @param session Pointer to the session to retreive the RPC.
 * @param rpc The RPC response to write to MVE.
 * @return MVE_BASE_ERROR_NONE if rpc contains valid data. Error code otherwise.
 */
mve_base_error mve_com_put_rpc_message5(struct mve_session *session,
                                       mve_com_rpc *rpc);

#endif /* MVE_COM_H */
