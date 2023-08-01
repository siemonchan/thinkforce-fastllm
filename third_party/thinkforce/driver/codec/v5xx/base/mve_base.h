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

#ifndef MVE_BASE_H
#define MVE_BASE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define IOCTL_MAGIC 251
#define MVE_BASE_COMMAND _IOWR(IOCTL_MAGIC, 0, struct mve_base_command_header)

#define MVE_BASE_FLAGS_DMABUF_DISABLE_CACHE_MAINTENANCE 0x80000000

#define MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_90   0x00000010  /* Frame is rotated 90 degrees */
#define MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_180  0x00000020  /* Frame is rotated 180 degrees */
#define MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_270  0x00000030  /* Frame is rotated 270 degrees */
#define MVE_BASE_BUFFER_FRAME_FLAG_ROTATION_MASK 0x00000030
#define MVE_BASE_BUFFER_FRAME_FLAG_SCALING_2     0x00000040  /* Frame is scaled by half */
#define MVE_BASE_BUFFER_FRAME_FLAG_SCALING_4     0x00000080  /* Frame is scaled by quarter */
#define MVE_BASE_BUFFER_FRAME_FLAG_SCALING_MASK  0x000000C0
#define MVE_BASE_BUFFER_FRAME_FLAG_ROI           0x00020000
#define MVE_BASE_BUFFER_FRAME_FLAG_CRC           0x00040000
#define MVE_BASE_BUFFER_FRAME_FLAG_AFBC_TILED    0x00080000  /* AFBC uses tiling for headers and blocks */
#define MVE_BASE_BUFFER_FRAME_FLAG_AFBC_WIDEBLK  0x00100000  /* AFBC uses wide super block 32x8, default is 16x16 */

#define MVE_BASE_RGB_TO_YUV_BT601_STUDIO        0

/* Buffer Flags for OMX Use */
#define MVE_BASE_BUFFERFLAG_EOS              0x00000001
#define MVE_BASE_BUFFERFLAG_STARTTIME        0x00000002
#define MVE_BASE_BUFFERFLAG_DECODEONLY       0x00000004
#define MVE_BASE_BUFFERFLAG_DATACORRUPT      0x00000008
#define MVE_BASE_BUFFERFLAG_ENDOFFRAME       0x00000010
#define MVE_BASE_BUFFERFLAG_SYNCFRAME        0x00000020
#define MVE_BASE_BUFFERFLAG_EXTRADATA        0x00000040
#define MVE_BASE_BUFFERFLAG_CODECCONFIG      0x00000080
#define MVE_BASE_BUFFERFLAG_TIMESTAMPINVALID 0x00000100
#define MVE_BASE_BUFFERFLAG_READONLY         0x00000200
#define MVE_BASE_BUFFERFLAG_ENDOFSUBFRAME    0x00000400
#define MVE_BASE_BUFFERFLAG_SKIPFRAME        0x00000800

/**
 * Commands from user- to kernel space.
 */
enum mve_base_command_type
{
    MVE_BASE_CREATE_SESSION,
    MVE_BASE_DESTROY_SESSION,
    MVE_BASE_ACTIVATE_SESSION,

    MVE_BASE_ENQUEUE_FLUSH_BUFFERS,
    MVE_BASE_ENQUEUE_STATE_CHANGE,

    MVE_BASE_GET_EVENT,
    MVE_BASE_SET_OPTION,
    MVE_BASE_BUFFER_PARAM,

    MVE_BASE_REGISTER_BUFFER,
    MVE_BASE_UNREGISTER_BUFFER,
    MVE_BASE_FILL_THIS_BUFFER,
    MVE_BASE_EMPTY_THIS_BUFFER,

    MVE_BASE_NOTIFY_REF_FRAME_RELEASE,

    MVE_BASE_REQUEST_MAX_FREQUENCY,

    MVE_BASE_READ_HW_INFO,

    MVE_BASE_RPC_MEM_ALLOC,
    MVE_BASE_RPC_MEM_RESIZE,

    MVE_BASE_DEBUG_READ_REGISTER,
    MVE_BASE_DEBUG_WRITE_REGISTER,
    MVE_BASE_DEBUG_INTERRUPT_COUNT,
    MVE_BASE_DEBUG_SEND_COMMAND,
    MVE_BASE_DEBUG_FIRMWARE_HUNG_SIMULATION,

    MVE_BASE_DMA_ALLOC,
    MVE_BASE_VERSION,
};

/**
 * External buffer identifier. The data stored by this type is specific for each
 * buffer API implementation. The mve_buffer_valloc implementation uses this
 * type to store addresses to user space allocated memory (virtual addresses). A buffer
 * API implementation for Android will use this type to store gralloc handles. */
typedef uint64_t mve_base_buffer_handle_t;

/**
 * @brief Represents a command sent to the driver for processing.
 */
struct mve_base_command_header
{
    uint32_t cmd;       /**< Which command to execute */
    uint32_t size;      /**< Size of the data section (excluding header size) */
    uint8_t data[0];    /**< First byte of the data section. */
};

/**
 * @brief Region of interest structure.
 *
 * The region is macroblock positions (x,y) in the range
 * mbx_left <= x < mbx_right
 * mby_top  <= y < mby_bottom
 */
struct mve_base_roi_region
{
    uint16_t mbx_left;   /**< X coordinate of left macro block */
    uint16_t mbx_right;  /**< X coordinate of right macro block */
    uint16_t mby_top;    /**< Y coordinate of top macro block */
    uint16_t mby_bottom; /**< Y coordinate of bottom macro block */
    int16_t qp_delta;    /**< Delta relative the default QP value */
};

#define MVE_BASE_ROI_REGIONS_MAX 16

/**
 * @brief Instances of this structure are sent along with fill/empty this buffer messages.
 */
struct mve_base_buffer_details
{
    mve_base_buffer_handle_t buffer_id;                           /**< Buffer unique ID */
    mve_base_buffer_handle_t handle;                              /**< Handle to buffer */
    uint32_t filled_len;                                          /**< Size of the contents in the buffer in bytes */
    uint32_t flags;                                               /**< OMX buffer flags */
    uint32_t mve_flags;                                           /**< MVE buffer flags */
    uint32_t crc_offset;                                          /**< Offset of the CRC data in the buffer. Only valid
                                                                   *   for CRC buffers using the attachment allocator. */
    uint64_t timestamp;                                           /**< Buffer timestamp */
    uint8_t nRegions;                                             /**< Number of ROI regions */
    uint8_t reserved[3];                                          /**< Unused but required for alignment reasons */
    struct mve_base_roi_region regions[MVE_BASE_ROI_REGIONS_MAX]; /**< ROI data */
};

/**
 * Events from kernel- to user space.
 */
enum mve_base_event_code
{
    MVE_BASE_EVENT_RPC_PRINT          = 0,  /**< Data contains a NULL terminated string */
    MVE_BASE_EVENT_SWITCHED_IN        = 1,
    MVE_BASE_EVENT_SWITCHED_OUT       = 2,
    MVE_BASE_EVENT_PONG               = 3,
    MVE_BASE_EVENT_STATE_CHANGED      = 4,
    MVE_BASE_EVENT_FW_ERROR           = 5,
    MVE_BASE_EVENT_STREAM_ERROR       = 6,
    MVE_BASE_EVENT_PROCESSED          = 7,
    MVE_BASE_EVENT_INPUT              = 8,
    MVE_BASE_EVENT_OUTPUT             = 9,
    MVE_BASE_EVENT_SET_PARAMCONFIG    = 10,
    MVE_BASE_EVENT_INPUT_FLUSHED      = 11,
    MVE_BASE_EVENT_OUTPUT_FLUSHED     = 12,
    MVE_BASE_EVENT_CODE_DUMP          = 13,
    MVE_BASE_EVENT_SESSION_HUNG       = 14,
    MVE_BASE_EVENT_ALLOC_PARAMS       = 15,
    MVE_BASE_EVENT_SEQUENCE_PARAMS    = 16,
    MVE_BASE_EVENT_BUFFER_PARAM       = 17,
    MVE_BASE_EVENT_REF_FRAME_RELEASED = 18,
    MVE_BASE_EVENT_RPC_MEM_ALLOC      = 19,
    MVE_BASE_EVENT_RPC_MEM_RESIZE     = 20,

    /* These messages are not forwarded to userspace and must therefore
     * be places last in this enum. */
    MVE_BASE_EVENT_JOB_DEQUEUED     = 22,
    MVE_BASE_EVENT_IDLE             = 23,
    MVE_BASE_EVENT_FW_TRACE_BUFFERS = 24,
};

/**
 * The event header of an event. Contains the event code and
 * size of the data attached to the event.
 */
struct mve_base_event_header
{
    uint16_t code;   /**< Event code */
    uint16_t size;   /**< Size of the data attached to the event in bytes */
    uint8_t data[0]; /**< First byte of the data attached to the event */
};

/**
 * Error codes for MVE responses.
 */
typedef enum
{
    MVE_BASE_ERROR_NONE,
    MVE_BASE_ERROR_UNDEFINED,
    MVE_BASE_ERROR_BAD_PARAMETER,
    MVE_BASE_ERROR_BAD_PORT_INDEX,
    MVE_BASE_ERROR_FIRMWARE,
    MVE_BASE_ERROR_HARDWARE,
    MVE_BASE_ERROR_INSUFFICIENT_RESOURCES,
    MVE_BASE_ERROR_NOT_IMPLEMENTED,
    MVE_BASE_ERROR_NOT_READY,
    MVE_BASE_ERROR_TIMEOUT,
    MVE_BASE_ERROR_VERSION_MISMATCH,
    MVE_BASE_ERROR_STREAM_CORRUPT,
    MVE_BASE_ERROR_STREAM_NOTSUPPORTED,
} mve_base_error;

/**
 * @brief Represents the result of an executed command.
 */
struct mve_base_response_header
{
    uint32_t error;             /**< MVE error code */
    uint32_t firmware_error;    /**< Firmware error code */
    uint32_t size;              /**< Size of the data section (excluding header size) */
    uint8_t data[0];            /**< First byte of the data section */
};

/**
 * This enum lists the different formats of supplied buffers.
 */
enum mve_base_buffer_format
{
    MVE_BASE_BUFFER_FORMAT_YUV420_PLANAR      = 0x7f000100, /**< Planar YUV buffer (3 planes) */
    MVE_BASE_BUFFER_FORMAT_YUV420_SEMIPLANAR  = 0x7f000101, /**< Semiplanar YUV (2 planes) */
    MVE_BASE_BUFFER_FORMAT_YUYYVY_10B         = 0x7f000102, /**< ARM 10-bit YUV 420 format */
    MVE_BASE_BUFFER_FORMAT_YUV420_AFBC        = 0x7f000103, /**< YUV buffer compressed with AFBC */
    MVE_BASE_BUFFER_FORMAT_YUV420_AFBC_10B    = 0x7f000104, /**< 10-bit YUV buffer compressed with AFBC */
    MVE_BASE_BUFFER_FORMAT_YUV422_1P          = 0x7f000105, /**< YUV 422 buffer (1 plane, YUY2) */
    MVE_BASE_BUFFER_FORMAT_YVU422_1P          = 0x7f000106, /**< YVU 422 buffer (1 plane, UYVY) */
    MVE_BASE_BUFFER_FORMAT_BITSTREAM          = 0x7f000107, /**< Compressed bitstream data */
    MVE_BASE_BUFFER_FORMAT_CRC                = 0x7f000108, /**< CRC buffer */
    MVE_BASE_BUFFER_FORMAT_YV12               = 0x7f000109, /**< Planar YV12 buffer (3 planes) */
    MVE_BASE_BUFFER_FORMAT_YVU420_SEMIPLANAR  = 0x7f00010a, /**< Semilanar YVU (2 planes) */
    MVE_BASE_BUFFER_FORMAT_RGBA_8888          = 0x7f00010b, /**< RGB format with 32 bit as Red 31:24, Green 23:16, Blue 15:8, Alpha 7:0 */
    MVE_BASE_BUFFER_FORMAT_BGRA_8888          = 0x7f00010c, /**< RGB format with 32 bit as Blue 31:24, Green 23:16, Red 15:8, Alpha 7:0 */
    MVE_BASE_BUFFER_FORMAT_ARGB_8888          = 0x7f00010d, /**< RGB format with 32 bit as Alpha 31:24, Red 23:16, Green 15:8, Blue 7:0 */
    MVE_BASE_BUFFER_FORMAT_ABGR_8888          = 0x7f00010e, /**< RGB format with 32 bit as Alpha 31:24, Blue 23:16, Green 15:8, Red 7:0 */
    MVE_BASE_BUFFER_FORMAT_YUV420_I420_10     = 0x7f00010f, /**< YUV420 16bit per component (3 planes) */
};

/**
 * Allocator used to allocate a user-space allocated buffer.
 */
enum mve_base_buffer_allocator
{
    MVE_BASE_BUFFER_ALLOCATOR_VMALLOC,    /**< Memory allocated by valloc or malloc. */
    MVE_BASE_BUFFER_ALLOCATOR_ATTACHMENT, /**< Represents a buffer that is part of another buffer. */
    MVE_BASE_BUFFER_ALLOCATOR_DMABUF,     /**< Memory wrapped by dma_buf. */
    MVE_BASE_BUFFER_ALLOCATOR_ASHMEM,     /**< Memory wrapped by ashmem. */
};

/**
 * This structure is used to transfer buffer information between users- and kernel space.
 */
struct mve_base_buffer_userspace
{
    uint64_t timestamp;                           /**< Buffer timestamp. */
    mve_base_buffer_handle_t buffer_id;           /**< Buffer unique ID. */
    mve_base_buffer_handle_t handle;              /**< Handle to the external buffer. */
    mve_base_buffer_handle_t crc_handle;          /**< Handle to the external CRC buffer. */

    enum mve_base_buffer_allocator allocator;     /**< Specifies which allocator was used to allocate
                                                   *   the buffer. */
    uint32_t size;                                /**< Size of the external buffer in bytes. */
    uint32_t width;                               /**< Width of the buffer (only for pixel formats). */
    uint32_t height;                              /**< Height of the buffer (only for pixel formats). */
    uint32_t max_height;                          /**< Maximum Height of the buffer (only for pixel formats). */
    uint32_t stride;                              /**< Stride of the buffer (only for pixel formats). */
    uint32_t stride_alignment;                    /**< Aligntment of the stride in bytes (only for pixel formats). */
    enum mve_base_buffer_format format;           /**< Format of the buffer. */

    uint32_t decoded_width;                       /**< Width of the decoded frame. Only valid for a returned frame buffer */
    uint32_t decoded_height;                      /**< Height of the decoded frame. Only valid for a returned frame buffer */

    uint32_t afbc_width_in_superblocks;           /**< Width of the AFBC buffer in superblocks (only for AFBC formats) */
    uint32_t afbc_alloc_bytes;                    /**< Size of the AFBC frame */

    uint32_t filled_len;                          /**< Number of bytes worth of data in the buffer. */
    uint32_t offset;                              /**< Offset from start of buffer to first byte. */
    uint32_t flags;                               /**< Flags for OMX use. */
    uint32_t mve_flags;                           /**< MVE sideband information. */
    uint32_t pic_index;                           /**< Picture index in decode order. Output from FW. */

    uint16_t cropx;                               /**< Luma x crop. */
    uint16_t cropy;                               /**< Luma y crop. */
    uint8_t y_offset;                             /**< Deblocking y offset of picture. */

    enum mve_base_buffer_allocator crc_allocator; /**< CRC buffer allocator. */
    uint32_t crc_size;                            /**< Size of the CRC buffer. */
    uint32_t crc_offset;                          /**< Offset of the CRC data in the buffer. */
};

enum mve_base_hw_state
{
    MVE_BASE_HW_STATE_STOPPED = 0,                /**< HW in STOPPED state. */
    MVE_BASE_HW_STATE_RUNNING = 2,                /**< HW in RUNNING state. */
    MVE_BASE_HW_STATE_PENDING = 4                 /**< Requested for HW State change and waiting for the response. */
};

/**
 * Defines what port(s) to flush.
 */
enum mve_base_flush
{
    MVE_BASE_FLUSH_INPUT_PORT = 1,  /**< Flush the input port */
    MVE_BASE_FLUSH_OUTPUT_PORT = 2, /**< Flush the output port */
    MVE_BASE_FLUSH_ALL_PORTS = MVE_BASE_FLUSH_INPUT_PORT | MVE_BASE_FLUSH_OUTPUT_PORT,
    /**< Flush input and output ports */
    MVE_BASE_FLUSH_QUICK = 1 << 2,  /**< Perform a quick flush. Quick flush means that
                                     *   all flushed buffers will automatically be
                                     *   re-enqueued once all buffers have been flushed.
                                     *   Userspace will not be notified of the flushed
                                     *   buffers or that the flush is complete. */
    MVE_BASE_FLUSH_QUICK_SET_INTERLACE = 1 << 3,
    /**< Makes all output buffers added as interlaced
     *   buffers once the quick flush is completed */
};

/* These constants are used to identify the firmware host interface protocol version */
enum mve_base_fw_major_prot_ver
{
    MVE_BASE_FW_MAJOR_PROT_V2 = 2,        /**< Protocol version v2 */
    MVE_BASE_FW_MAJOR_PROT_V3 = 3,        /**< Protocol version v3 */
    MVE_BASE_FW_MAJOR_PROT_UNKNOWN = 255  /**< Signals unknown protocol version or error reading
                                           *   protocol version */
};

enum mve_base_fw_minor_ver
{
    MVE_BASE_FW_MINOR_V0,                 /**< Firmware minor version v0 */
    MVE_BASE_FW_MINOR_V1,                 /**< Firmware minor version v1 */
    MVE_BASE_FW_MINOR_V2,                 /**< Firmware minor version v2 */
    MVE_BASE_FW_MINOR_V3,                 /**< Firmware minor version v3 */
    MVE_BASE_FW_MINOR_V4,                 /**< Firmware minor version v4 */
    MVE_BASE_FW_MINOR_V5,                 /**< Firmware minor version v5 */
    MVE_BASE_FW_MINOR_V6,                 /**< Firmware minor version v6 */
    MVE_BASE_FW_MINOR_V7,                 /**< Firmware minor version v7 */
    MVE_BASE_FW_MINOR_V8,                 /**< Firmware minor version v8 */
    MVE_BASE_FW_MINOR_V9,                 /**< Firmware minor version v9 */
    MVE_BASE_FW_MINOR_UNKNOWN = 255       /**< Signals unknown firmware minor version */
};

struct mve_base_hw_info
{
    uint32_t fuse;                     /**< Hardware fuse register. */
    uint32_t version;                  /**< Hardware version. */
    uint32_t ncores;                   /**< Number of MVE Cores*/
};

struct mve_base_fw_version
{
    uint8_t major;                    /**< Firmware major version. */
    uint8_t minor;                    /**< Firmware minor version. */
};

struct mve_base_fw_secure_descriptor
{
    struct mve_base_fw_version fw_version; /**< FW protocol version */
    uint32_t l2pages;                      /**< Physical address of l2pages created by secure OS */
};

struct mve_base_fw_frame_alloc_parameters
{
    uint16_t planar_alloc_frame_width;             /**< Width of planar YUV buffer */
    uint16_t planar_alloc_frame_height;            /**< Height of planar YUV buffer */

    uint32_t afbc_alloc_bytes;                     /**< Number of bytes needed for an AFBC buffer */

    uint32_t afbc_alloc_bytes_downscaled;          /**< Number of bytes needed for downscaled AFBC buffer */

    uint16_t afbc_width_in_superblocks;            /**< Width of the AFBC buffer needed by the FW */
    uint16_t afbc_width_in_superblocks_downscaled; /**< Width of the downscaled AFBC buffer needed by the FW */

    uint16_t cropx;                                /**< Hints on how much to adjust the plane addresses to get optimal AXI bursts */
    uint16_t cropy;                                /**< Hints on how much to adjust the plane addresses to get optimal AXI bursts */

    uint32_t mbinfo_alloc_bytes;                   /* Only for debugging */
};

/*
 * Defines the type of memory region to allocate in RPC memory calls.
 * Refer to mve_rpc_params.region from mve_protocol_def.h.
 */
enum mve_base_memory_region
{
    MVE_BASE_MEMORY_REGION_PROTECTED = 0,
    MVE_BASE_MEMORY_REGION_OUTBUF = 1,
    MVE_BASE_MEMORY_REGION_FRAMEBUF = MVE_BASE_MEMORY_REGION_OUTBUF
};

struct mve_base_rpc_memory
{
    enum mve_base_memory_region region;
    uint32_t size;
};

/*
 * Copied from mve_protocol_def.h.
 */
struct mve_base_response_sequence_parameters
{
    uint8_t interlace;
    uint8_t chroma_format;
        #define MVE_BASE_CHROMA_FORMAT_MONO 0x0
        #define MVE_BASE_CHROMA_FORMAT_420  0x1
        #define MVE_BASE_CHROMA_FORMAT_422  0x2
        #define MVE_BASE_CHROMA_FORMAT_440  0x3
        #define MVE_BASE_CHROMA_FORMAT_ARGB 0x4
    uint8_t bitdepth_luma;
    uint8_t bitdepth_chroma;
    uint8_t num_buffers_planar;
    uint8_t num_buffers_afbc;
    uint8_t range_mapping_enabled;
    uint8_t reserved0;
};

/*
 * Copied from mve_protocol_def.h.
 */

/* output from decoder */
struct mve_base_buffer_param_display_size
{
    uint16_t display_width;
    uint16_t display_height;
};

/* output from decoder, colour information needed for hdr */
struct mve_base_buffer_param_colour_description
{
    uint32_t flags;
        #define MVE_BASE_BUFFER_PARAM_COLOUR_FLAG_MASTERING_DISPLAY_DATA_VALID  (1)
        #define MVE_BASE_BUFFER_PARAM_COLOUR_FLAG_CONTENT_LIGHT_DATA_VALID      (2)

    uint8_t range;                     /* Unspecified=0, Limited=1, Full=2 */
        #define MVE_BASE_BUFFER_PARAM_COLOUR_RANGE_UNSPECIFIED  (0)
        #define MVE_BASE_BUFFER_PARAM_COLOUR_RANGE_LIMITED      (1)
        #define MVE_BASE_BUFFER_PARAM_COLOUR_RANGE_FULL         (2)

    uint8_t colour_primaries;                  /* see hevc spec. E.3.1 */
    uint8_t transfer_characteristics;          /* see hevc spec. E.3.1 */
    uint8_t matrix_coeff;                      /* see hevc spec. E.3.1 */

    uint16_t mastering_display_primaries_x[3]; /* see hevc spec. D.3.27 */
    uint16_t mastering_display_primaries_y[3]; /* see hevc spec. D.3.27 */
    uint16_t mastering_white_point_x;          /* see hevc spec. D.3.27 */
    uint16_t mastering_white_point_y;          /* see hevc spec. D.3.27 */
    uint32_t max_display_mastering_luminance;  /* see hevc spec. D.3.27 */
    uint32_t min_display_mastering_luminance;  /* see hevc spec. D.3.27 */

    uint32_t max_content_light_level;          /* unused */
    uint32_t avg_content_light_level;          /* unused */
};

/* Parameters that are sent in the same communication channels
 * as the buffers. A parameter applies to all subsequent buffers.
 * Some types are only valid for decode, and some only for encode.
 */
struct mve_base_buffer_param
{
    uint32_t type;
        #define MVE_BASE_BUFFER_PARAM_TYPE_DISPLAY_SIZE       (5)
        #define MVE_BASE_BUFFER_PARAM_TYPE_COLOUR_DESCRIPTION (15)
        #define MVE_BASE_BUFFER_PARAM_TYPE_FRAME_FIELD_INFO   (17)
        #define MVE_BASE_BUFFER_PARAM_TYPE_DPB_HELD_FRAMES    (19)

    union
    {
        struct mve_base_buffer_param_display_size display_size;
        struct mve_base_buffer_param_colour_description colour_description;
    }
    data;
};

#endif /* MVE_BASE_H */
