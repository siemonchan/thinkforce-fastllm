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

#ifndef MVE_RSRC_LOG_RAM_H
#define MVE_RSRC_LOG_RAM_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#ifndef __KERNEL__
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#else
#include <linux/types.h>
#include <linux/time.h>
#endif

/******************************************************************************
 * Defines
 ******************************************************************************/

/**
 * Magic word "MVEL" that prefix all messages.
 *
 * Messages are stored in native byte order. The magic word can be used to
 * detect if the log has been stored in the same byte order as the application
 * unpacking the log is using.
 */
#define MVE_LOG_MAGIC                   0x4d56454c

/**
 * The maximum message length.
 */
#define MVE_LOG_MESSAGE_LENGTH_MAX      4096

/******************************************************************************
 * Types
 ******************************************************************************/

/**
 * IOCTL commands.
 */
enum mve_log_ioctl
{
    MVE_LOG_IOCTL_CLEAR /**< Clear the log. */
};

/**
 * Message type. The definitions are assigned values that are not allowed to change.
 */
enum mve_log_type
{
    MVE_LOG_TYPE_TEXT = 0,
    MVE_LOG_TYPE_FWIF = 1,
    MVE_LOG_TYPE_FW_BINARY = 2,
    MVE_LOG_TYPE_FW_RPC = 3,
    MVE_LOG_TYPE_MAX
};

/**
 * Portable time value format.
 */
struct mve_log_timeval
{
    uint64_t sec;                     /**< Seconds since 1970-01-01, Unix time epoch. */
    uint64_t nsec;                    /**< Nano seconds. */
}
__attribute__((packed));

/**
 * Common header for all messages stored in RAM buffer.
 */
struct mve_log_header
{
    uint32_t magic;                   /**< Magic word. */
    uint16_t length;                  /**< Length of message, excluding this header. */
    uint8_t type;                     /**< Message type. */
    uint8_t severity;                 /**< Message severity. */
    struct mve_log_timeval timestamp; /**< Time stamp. */
}
__attribute__((packed));

/******************************************************************************
 * Text message
 ******************************************************************************/

/**
 * ASCII text message.
 *
 * The message shall be header.length long and should end with a standard ASCII
 * character. The parser of the log will add new line and null terminate
 * the string.
 */
struct mve_log_text
{
    char message[0];                  /**< ASCII text message. */
}
__attribute__((packed));

/******************************************************************************
 * Firmware interface
 ******************************************************************************/

/**
 * Firmware interface message types.
 */
enum mve_log_fwif_channel
{
    MVE_LOG_FWIF_CHANNEL_MESSAGE,
    MVE_LOG_FWIF_CHANNEL_INPUT_BUFFER,
    MVE_LOG_FWIF_CHANNEL_OUTPUT_BUFFER,
    MVE_LOG_FWIF_CHANNEL_RPC
};

/**
 * Firmware interface message types.
 */
enum mve_log_fwif_direction
{
    MVE_LOG_FWIF_DIRECTION_HOST_TO_FIRMWARE,
    MVE_LOG_FWIF_DIRECTION_FIRMWARE_TO_HOST
};

/**
 * Special message codes for message types not defined by the firmware interface.
 */
enum mve_log_fwif_code
{
    MVE_LOG_FWIF_CODE_STAT = 16000
};

/**
 * Firmware interface header type.
 */
struct mve_log_fwif
{
    uint8_t version_minor;          /**< Protocol version. */
    uint8_t version_major;          /**< Protocol version. */
    uint8_t channel;                /**< @see enum mve_log_fwif_channel. */
    uint8_t direction;              /**< @see enum mve_log_fwif_direction. */
    uint64_t session;               /**< Session id. */
    uint8_t data[0];                /**< Data following the firmware interface message header. */
}
__attribute__((packed));

/**
 * Firmware interface statistics.
 */
struct mve_log_fwif_stat
{
    uint64_t handle;                /**< Buffer handle. */
    uint32_t queued;                /**< Number of buffers currently queued to the firmware. */
}
__attribute__((packed));

/******************************************************************************
 * Firmware binary header
 ******************************************************************************/

/**
 * Firmware binary header.
 *
 * The first ~100 bytes of the firmware binary contain information describing
 * the codec.
 */
struct mve_log_fw_binary
{
    uint32_t length;                /**< Number of bytes copied from the firmware binary. */
    uint64_t session;               /**< Session id. */
    uint8_t data[0];                /**< Firmware binary, byte 0..length. */
};

#endif /* MVE_RSRC_LOG_RAM_H */
