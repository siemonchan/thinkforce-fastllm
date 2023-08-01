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

#ifndef MVE_RSRC_LOG_H
#define MVE_RSRC_LOG_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#ifndef __KERNEL__
#include "emulator_userspace.h"
#include <sys/uio.h>
#else
#include <linux/net.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/wait.h>
#endif

/******************************************************************************
 * Defines
 ******************************************************************************/

/**
 * Print a log message.
 *
 * @param _lg           Pointer to log group.
 * @param _severity     Severity.
 * @param _fmt          Format string.
 */
#define MVE_LOG_PRINT(_lg, _severity, _fmt, ...)             \
    if ((_severity) <= (_lg)->severity)                      \
    {                                                        \
        __MVE_LOG_PRINT(_lg, _severity, _fmt,##__VA_ARGS__); \
    }

/**
 * Print a log message only if it's a debug build.
 *
 * @param _severity     Severity.
 * @param _fmt          Format string.
 */
#if defined(_DEBUG)
#define MVE_LOG_DEBUG_PRINT(_lg, _fmt, ...) \
    MVE_LOG_PRINT(_lg, MVE_LOG_DEBUG, _fmt,##__VA_ARGS__)
#else
#define MVE_LOG_DEBUG_PRINT(_lg, _fmt, ...)
#endif

/**
 * Print a log message for a session.
 *
 * @param _lg           Pointer to log group.
 * @param _severity     Severity.
 * @param _session      Pointer to session.
 * @param _fmt          Format string.
 */
#define MVE_LOG_PRINT_SESSION(_lg, _severity, _session, _fmt, ...) \
    if ((_severity) <= (_lg)->severity)                            \
    {                                                              \
        __MVE_LOG_PRINT(_lg, _severity, "%p " _fmt, _session,      \
                        ##__VA_ARGS__);                            \
    }

/**
 * Print a log message only if it's a debug build.
 *
 * @param _severity     Severity.
 * @param _fmt          Format string.
 */
#if defined(_DEBUG)
#define MVE_LOG_DEBUG_PRINT_SESSION(_lg, _session, _fmt, ...) \
    MVE_LOG_PRINT_SESSION(_lg, MVE_LOG_DEBUG, _session, _fmt,##__VA_ARGS__)
#else
#define MVE_LOG_DEBUG_PRINT_SESSION(_lg, _session, _fmt, ...)
#endif

/**
 * Print binary data.
 *
 * @param _lg           Pointer to log group.
 * @param _severity     Severity.
 * @param _vec          Scatter input vector data.
 * @param _size         _vec array size.
 */
#define MVE_LOG_DATA(_lg, _severity, _vec, _count)                 \
    if ((_severity) <= (_lg)->severity)                            \
    {                                                              \
        (_lg)->drain->data((_lg)->drain, _severity, _vec, _count); \
    }

/**
 * Check if severity level for log group is enabled.
 *
 * @param _lg           Pointer to log group.
 * @param _severity     Severity.
 */
#define MVE_LOG_ENABLED(_lg, _severity) \
    ((_severity) <= (_lg)->severity)

/**
 * Execute function if log group is enabled.
 *
 * @param _lg           Pointer to log group.
 * @param _severity     Severity.
 * @param _vec          Scatter input vector data.
 * @param _size         _vec array size.
 */
#define MVE_LOG_EXECUTE(_lg, _severity, _exec) \
    if (MVE_LOG_ENABLED(_lg, _severity))       \
    {                                          \
        _exec;                                 \
    }

#ifdef MVE_LOG_PRINT_FILE_ENABLE
#define __MVE_LOG_PRINT(_lg, _severity, _fmt, ...)           \
    (_lg)->drain->print((_lg)->drain, _severity, (_lg)->tag, \
                        _fmt " (%s:%d)",                     \
                        __MVE_LOG_N_ARGS(__VA_ARGS__),       \
                        ##__VA_ARGS__,                       \
                        mve_rsrc_log_strrchr6(__FILE__), __LINE__)
#else
#define __MVE_LOG_PRINT(_lg, _severity, _fmt, ...)                 \
    (_lg)->drain->print((_lg)->drain, _severity, (_lg)->tag, _fmt, \
                        __MVE_LOG_N_ARGS(__VA_ARGS__),             \
                        ##__VA_ARGS__)
#endif /* MVE_LOG_PRINT_FILE_ENABLE */

#define __MVE_LOG_N_ARGS(...) \
    __MVE_LOG_COUNT(dummy,##__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define __MVE_LOG_COUNT(_0, _1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

/******************************************************************************
 * Types
 ******************************************************************************/

/**
 * Severity levels.
 */
enum mve_rsrc_log_severity
{
    MVE_LOG_PANIC,
    MVE_LOG_ERROR,
    MVE_LOG_WARNING,
    MVE_LOG_INFO,
    MVE_LOG_DEBUG,
    MVE_LOG_VERBOSE,
    MVE_LOG_MAX
};

struct mve_rsrc_log_drain;

/**
 * Function pointer to output text messages.
 *
 * @param drain         Pointer to drain.
 * @param severity      Severity level.
 * @param tag           Log group tag.
 * @param fmt           Format string.
 * @param n_args        Number of arguments to format string.
 */
typedef void (*mve_rsrc_print_fptr)(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *fmt, const unsigned n_args, ...);

/**
 * Function pointer to output binary data.
 *
 * @param drain         Pointer to drain.
 * @param severity      Severity level.
 * @param tag           Log group tag.
 * @param data          Pointer to data.
 * @param length        Length to data.
 */
typedef void (*mve_rsrc_data_fptr)(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count);

/**
 * Structure with information about the drain. The drain handles the formatting
 * and redirection of the log messages.
 */
struct mve_rsrc_log_drain
{
    mve_rsrc_print_fptr print;  /**< Print function pointer. */
    mve_rsrc_data_fptr data;    /**< Data function pointer. */

    struct dentry *dentry;      /**< Debugfs dentry. */
};

/**
 * Structure describing a specialized RAM drain.
 */
struct mve_rsrc_log_drain_ram
{
    struct mve_rsrc_log_drain base;      /**< Base class. */

    char *buf;                           /**< Pointer to output buffer. */
    const size_t buffer_size;            /**< Size of the buffer. Must be power of 2. */
    size_t read_pos;                     /**< Read position when a new file handle is opened. Is updated when the buffer is cleared. */
    size_t write_pos;                    /**< Current write position in RAM buffer. */
    size_t write_error_pos;              /**< Current write position of last error in RAM buffer. */
    enum mve_rsrc_log_severity severity; /**< Severity required to flush RAM buffer on error. */
    wait_queue_head_t queue;             /**< Wait queue for blocking IO. */
    struct semaphore sem;                /**< Semaphore to prevent concurrent writes. */
};

#ifndef __KERNEL__
/**
 * Structure describing a file drain.
 */
struct mve_rsrc_log_drain_file
{
    struct mve_rsrc_log_drain base;

    sem_t file_sem;
    FILE *fp;
};
#endif

/**
 * Structure describing Android log drain.
 */
struct mve_rsrc_log_drain_alog
{
    struct mve_rsrc_log_drain base;     /**< Base class. */
    struct socket *socket;              /**< Socket to Android log daemon. */
    struct semaphore sem;
};

/**
 * Structure describing log group. The log group filters which log messages that
 * shall be forwarded to the drain.
 */
struct mve_rsrc_log_group
{
    const char *tag;                        /**< Name of log group. */
    enum mve_rsrc_log_severity severity;    /**< Severity level. */
    struct mve_rsrc_log_drain *drain;       /**< Drain. */

    struct dentry *dentry;                  /**< Debugfs dentry. */
};

/**
 * Log class that keeps track of registered groups and drains.
 */
struct mve_rsrc_log
{
    struct dentry *mve_dir;
    struct dentry *drain_dir;
    struct dentry *group_dir;
};

/******************************************************************************
 * Prototypes
 ******************************************************************************/

extern struct mve_rsrc_log_group mve_rsrc_log6;
extern struct mve_rsrc_log_group mve_rsrc_log_scheduler6;
extern struct mve_rsrc_log_group mve_rsrc_log_fwif6;
extern struct mve_rsrc_log_group mve_rsrc_log_session6;

/**
 * Initialize log module. This function must be called before any of the log
 * groups is used.
 */
void mve_rsrc_log_init6(void);

/**
 * Destroy log module.
 */
void mve_rsrc_log_destroy6(void);

/**
 * Find last occurrence of '/' in string.
 *
 * @param s         Pointer to string.
 * @return Pointer to '/'+1, or pointer to begin of string.
 */
const char *mve_rsrc_log_strrchr6(const char *s);

/**
 * Returns the dentry for the parent directory. You can use the return value
 * of this function if you want to add debug fs files from another module to
 * the same directory as the log files (e.g. /sys/kernel/debug/mve).
 */
struct dentry *mve_rsrc_log_get_parent_dir6(void);

/**
 * Trim of trailing new line.
 *
 * @param str       Pointer to string.
 */
inline static void mve_rsrc_log_trim(char *str)
{
    size_t len = strlen(str);

    while (len-- > 0)
    {
        if (str[len] != '\n')
        {
            break;
        }

        str[len] = '\0';
    }
}

#endif /* MVE_RSRC_LOG_H */
