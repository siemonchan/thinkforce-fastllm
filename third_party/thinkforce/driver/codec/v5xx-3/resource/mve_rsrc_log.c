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

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "mve_rsrc_log.h"
#include "mve_rsrc_log_ram.h"
#include "mve_rsrc_mem_frontend.h"

#ifdef __KERNEL__
#include <asm/uaccess.h>
#include <linux/aio.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/un.h>
#include <linux/version.h>
#include <linux/poll.h>
#else
#include "emulator_userspace.h"
#endif /* __KERNEL */

/******************************************************************************
 * Defines
 ******************************************************************************/

#ifndef UNUSED
#define UNUSED(x) (void)x
#endif /* UNUSED */

#ifdef _BullseyeCoverage
    #define BullseyeCoverageOff \
    # pragma BullseyeCoverage off
    #define BullseyeCoverageOn \
    # pragma BullseyeCoverage on
#else
    #define BullseyeCoverageOff
    #define BullseyeCoverageOn
#endif

/******************************************************************************
 * Types
 ******************************************************************************/

/******************************************************************************
 * Variables
 ******************************************************************************/

static struct mve_rsrc_log log;

/* When adding a new log group, please update the functions
 * suppress_kernel_logs() and restore_kernel_logs()
 * which are defined in driver/test/system/mve_suite_common.c
 * accordingly.
 */

struct mve_rsrc_log_group mve_rsrc_log3;
struct mve_rsrc_log_group mve_rsrc_log_scheduler3;
struct mve_rsrc_log_group mve_rsrc_log_fwif3;
struct mve_rsrc_log_group mve_rsrc_log_session3;

EXPORT_SYMBOL(mve_rsrc_log3);
EXPORT_SYMBOL(mve_rsrc_log_fwif3);
EXPORT_SYMBOL(mve_rsrc_log_session3);

static struct mve_rsrc_log_drain drain_dmesg;
static struct mve_rsrc_log_drain_ram drain_ram0;
#ifndef __KERNEL__
static struct mve_rsrc_log_drain_file drain_file;
#endif

#ifdef MVE_LOG_ALOG_ENABLE
static struct mve_rsrc_log_drain_alog drain_alog;
#endif /* MVE_LOG_ALOG_ENABLE */

#ifdef MVE_LOG_FTRACE_ENABLE
static struct mve_rsrc_log_drain drain_ftrace;

/**
 * Map severity to string.
 */
static const char *severity_to_name[] =
{
    "Panic",
    "Error",
    "Warning",
    "Info",
    "Debug",
    "Verbose"
};
#endif /* MVE_LOG_FTRACE_ENABLE */

/**
 * Map severity to kernel log level.
 */
static const char *severity_to_kern_level[] =
{
    KERN_EMERG,
    KERN_ERR,
    KERN_WARNING,
    KERN_NOTICE,
    KERN_INFO,
    KERN_DEBUG
};

#ifdef EMULATOR
static const char *severity_to_a_level[] =
{
    "F",
    "E",
    "W",
    "N",
    "I",
    "D"
};
#endif

/******************************************************************************
 * Static functions
 ******************************************************************************/

/******************************************************************************
 * Log
 *
 * Directory                    i_node->i_private
 * --------------------------------------------------------
 * mve                          struct mve_rsrc_log *
 * +-- group
 * |   +-- <group>              struct mve_rsrc_log_group *
 * |       +-- severity
 * |       +-- drain
 * +-- drain
 *     +-- <drain>              struct mve_rsrc_log_drain *
 *
 ******************************************************************************/

BullseyeCoverageOff

/**
 * Search for child dentry with matching name.
 *
 * @param parent        Pointer to parent dentry.
 * @param name          Name of dentry to look for.
 * @return Pointer to dentry, NULL if not found.
 */
static struct dentry *lookup(struct dentry *parent, const char *name)
{
    struct dentry *child;

    /* Loop over directory entries in mve/drain/. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) || defined(EMULATOR)
    list_for_each_entry(child, &parent->d_subdirs, d_child)
#else
    list_for_each_entry(child, &parent->d_subdirs, d_u.d_child)
#endif
    {
        if (strcmp(name, child->d_name.name) == 0)
        {
            return child;
        }
    }

    return NULL;
}

/**
 * Read handle function for mve/group/<group>/drain. The function returns the name
 * of the currently configured drain.
 */
static ssize_t readme_read(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    static const char msg[] =
        "LOG GROUPS\n"
        "\n"
        "The avaible log groups can be found under 'group'.\n"
        "$ ls group\n"
        "\n"
        "SEVERITY LEVELS\n"
        "    0 - Panic\n"
        "    1 - Error\n"
        "    2 - Warning\n"
        "    3 - Info\n"
        "    4 - Debug\n"
        "    5 - Verbose\n"
        "\n"
        "The severity level for a log group can be read and set at runtime.\n"
        "$ cat group/general/severity\n"
        "$ echo 3 > group/general/severity\n";

    return simple_read_from_buffer(user_buffer, count, position, msg, sizeof(msg));
}

/**
 * Read handle function for mve/group/<group>/drain. The function returns the name
 * of the currently configured drain.
 */
static ssize_t group_drain_read(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    /* File path mve/group/<group>/drain. */
    struct mve_rsrc_log_group *group = file->f_path.dentry->d_parent->d_inode->i_private;
    struct mve_rsrc_log_drain *drain = group->drain;
    char name[100];
    size_t len;

    if (drain == NULL || drain->dentry == NULL)
    {
        printk(KERN_ERR "MVE: No drain assigned to log group.\n");
        return -EINVAL;
    }

    len = scnprintf(name, sizeof(name), "%s\n", drain->dentry->d_name.name);

    return simple_read_from_buffer(user_buffer, count, position, name, len);
}

/**
 * Write handle function for mve/group/<group>/drain. The function sets the drain
 * for the group. If the drain does not match any registered drain, then error is
 * returned to user space.
 */
static ssize_t group_drain_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *position)
{
    /* File path mve/group/<group>/drain. */
    struct mve_rsrc_log_group *group = file->f_path.dentry->d_parent->d_inode->i_private;
    struct mve_rsrc_log *log = file->f_path.dentry->d_parent->d_parent->d_parent->d_inode->i_private;
    struct dentry *dentry;
    char drain_str[100];
    ssize_t size;

    /* Check that input is not larger that path buffer. */
    if (count > (sizeof(drain_str) - 1))
    {
        printk(KERN_ERR "MVE: Input overflow.\n");

        return -EINVAL;
    }

    /* Append input to path. */
    size = simple_write_to_buffer(drain_str, sizeof(drain_str) - 1, position, user_buffer, count);
    drain_str[count] = '\0';
    mve_rsrc_log_trim(drain_str);

    dentry = lookup(log->drain_dir, drain_str);

    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: No drain matching '%s'.\n", drain_str);
        return -EINVAL;
    }

    /* Assign drain to log group. */
    group->drain = dentry->d_inode->i_private;

    return size;
}

/**
 * Read the RAM buffer.
 */
static ssize_t drain_ram_read(struct mve_rsrc_log_drain_ram *drain, char __user *user_buffer, size_t count, loff_t *position, size_t pos)
{
    ssize_t n = 0;

    /* Make sure position is not beyond end of file. */
    if (*position > pos)
    {
        return -EINVAL;
    }

    /* If position is more than BUFFER_SIZE bytes behind, then fast forward to current position minus BUFFER_SIZE. */
    if ((pos - *position) > drain->buffer_size)
    {
        *position = pos - drain->buffer_size;
    }

    /* Copy data to user space. */
    while ((n < count) && (*position < pos))
    {
        size_t offset;
        size_t length;

        /* Offset in circular buffer. */
        offset = *position & (drain->buffer_size - 1);

        /* Available number of bytes. */
        length = min((size_t)(pos - *position), count - n);

        /* Make sure length does not go beyond end of circular buffer. */
        length = min(length, drain->buffer_size - offset);

        /* Copy data from kernel- to user space. */
        length -= copy_to_user(&user_buffer[n], &drain->buf[offset], length);

        /* No bytes were copied. Return error. */
        if (length == 0)
        {
            return -EINVAL;
        }

        *position += length;
        n += length;
    }

    return n;
}

/**
 * Blocking read of the RAM buffer.
 */
static ssize_t drain_ram_read_block(struct mve_rsrc_log_drain_ram *drain, char __user *user_buffer, size_t count, loff_t *position, size_t *pos)
{
    while (*position == *pos)
    {
        int ret;

        ret = wait_event_interruptible(drain->queue, *position < *pos);
        if (ret != 0)
        {
            return -EINTR;
        }
    }

    return drain_ram_read(drain, user_buffer, count, position, *pos);
}

/**
 * Non blocking read of the RAM buffer.
 */
static ssize_t drain_ram_read_msg(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    struct mve_rsrc_log_drain_ram *drain = file->f_path.dentry->d_parent->d_inode->i_private;

    return drain_ram_read(drain, user_buffer, count, position, drain->write_pos);
}

/**
 * Blocking read of the RAM buffer.
 */
static ssize_t drain_ram_read_pipe(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    struct mve_rsrc_log_drain_ram *drain = file->f_path.dentry->d_parent->d_inode->i_private;

    return drain_ram_read_block(drain, user_buffer, count, position, &drain->write_pos);
}

#ifdef __KERNEL__
static unsigned int drain_ram_poll_pipe(struct file *file, poll_table *wait)
{
    unsigned int mask = 0;
    struct mve_rsrc_log_drain_ram *drain = file->f_path.dentry->d_parent->d_inode->i_private;

    poll_wait(file, &drain->queue, wait);
    if (file->f_pos < drain->write_pos)
    {
        mask |= POLLIN | POLLRDNORM;
    }
    else if (file->f_pos > drain->write_pos)
    {
        mask |= POLLERR;
    }

    return mask;
}
#endif

/**
 * Blocking read of the RAM buffer. Write position is only updated when an error is reported.
 */
static ssize_t drain_ram_read_flush_on_error(struct file *file, char __user *user_buffer, size_t count, loff_t *position)
{
    struct mve_rsrc_log_drain_ram *drain = file->f_path.dentry->d_parent->d_inode->i_private;

    return drain_ram_read_block(drain, user_buffer, count, position, &drain->write_error_pos);
}

/**
 * Copy data from iterator.
 */
static ssize_t copy_iterator(void *dst, struct iov_iter *iter, size_t size)
{
    size_t s = 0;

    while ((s < size) && (iter->nr_segs > 0))
    {
        size_t len = min(size - s, iter->iov->iov_len - iter->iov_offset);
        int ret;

        ret = copy_from_user((char *)dst + s, (char *)iter->iov->iov_base + iter->iov_offset, len);
        if (ret != 0)
        {
            return -EFAULT;
        }

        s += len;
        iter->iov_offset += len;

        if (iter->iov_offset >= iter->iov->iov_len)
        {
            iter->iov_offset = 0;
            iter->iov++;
            iter->nr_segs--;
        }
    }

    return s;
}

/**
 * Write data to RAM buffer.
 */
static ssize_t drain_ram_write_iter(struct kiocb *iocb, struct iov_iter *iov_iter)
{
    struct file *file = iocb->ki_filp;
    struct mve_rsrc_log_drain_ram *drain_ram = file->f_path.dentry->d_parent->d_inode->i_private;
    struct mve_log_header header;
    struct iov_iter iter;
    size_t length;
    size_t len;
    size_t pos;
    size_t ret;
    int sem_taken;

    /* Calculate total length. */
    length = iov_length(iov_iter->iov, iov_iter->nr_segs);

    iter = *iov_iter;
    ret = copy_iterator(&header, &iter, sizeof(header));
    if (ret != sizeof(header))
    {
        printk(KERN_ERR "MVE: Not enough data to read RAM header.\n");
        return -EFAULT;
    }

    /* Check that magic is correct. */
    if (header.magic != MVE_LOG_MAGIC)
    {
        printk(KERN_ERR "MVE: RAM header does not contain magic word.\n");
        return -EFAULT;
    }

    /* Verify length of header. */
    if ((header.length + sizeof(header)) != length)
    {
        printk(KERN_ERR "MVE: RAM header has incorrect length. header.length+%zu=%zu, length=%zu.\n", sizeof(header), header.length + sizeof(header), length);
        return -EFAULT;
    }

    /* Check that message length is not larger than RAM buffer. */
    if (length > drain_ram->buffer_size)
    {
        printk(KERN_ERR "MVE: Logged data larger than output buffer. length=%zu, buffer_length=%zu.\n", length, (size_t)drain_ram->buffer_size);
        return -EFAULT;
    }

    sem_taken = down_interruptible(&drain_ram->sem);

    pos = drain_ram->write_pos & (drain_ram->buffer_size - 1);
    len = length;

    /* Loop over scatter input. */
    while (len > 0)
    {
        size_t n = min(len, drain_ram->buffer_size - pos);

        ret = copy_iterator(&drain_ram->buf[pos], iov_iter, n);
        if (ret != n)
        {
            printk(KERN_ERR "MVE: Failed to copy data from user space.\n");
            if (0 == sem_taken)
            {
                up(&drain_ram->sem);
            }
            return -EFAULT;
        }

        len -= ret;
        pos = (pos + ret) & (drain_ram->buffer_size - 1);
    }

    /* Update write_pos to 4 byte aligned length */
    drain_ram->write_pos += (length + 3) & ~3;

    /* Flush RAM buffer if severity exceeds configured severity. */
    if (header.severity <= drain_ram->severity)
    {
        drain_ram->write_error_pos = drain_ram->write_pos;
    }

    if (0 == sem_taken)
    {
        up(&drain_ram->sem);
    }
    wake_up_interruptible(&drain_ram->queue);

    return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
/**
 * Write data to RAM buffer.
 */
static ssize_t drain_ram_aio_write(struct kiocb *iocb, const struct iovec *vec, unsigned long count, loff_t offset)
{
    struct iov_iter iter = { 0 };

    /* iter.type = ITER_IOVEC; */
    iter.iov_offset = 0;
    iter.count = iov_length(vec, count);
    iter.iov = vec;
    iter.nr_segs = count;

    return drain_ram_write_iter(iocb, &iter);
}
#endif

/**
 * Handle IOCTL.
 */
static long drain_ram_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct mve_rsrc_log_drain_ram *drain_ram = file->f_path.dentry->d_parent->d_inode->i_private;

    switch (cmd)
    {
        case MVE_LOG_IOCTL_CLEAR:
            drain_ram->read_pos = drain_ram->write_pos;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

/**
 * Open file handle function.
 */
static int drain_ram_open(struct inode *inode, struct file *file)
{
    struct mve_rsrc_log_drain_ram *drain_ram = file->f_path.dentry->d_parent->d_inode->i_private;

    file->f_pos = drain_ram->read_pos;

    return 0;
}

/**
 * Add a group with given name to log.
 *
 * @param log       Pointer to log.
 * @param name      Name of group.
 * @param group     Pointer to group.
 */
static void mve_rsrc_log_group_add(struct mve_rsrc_log *log, const char *name, struct mve_rsrc_log_group *group)
{
    static const struct file_operations group_drain_fops =
    {
        .read = group_drain_read,
        .write = group_drain_write
    };
    struct dentry *dentry;

    /* Create <group> directory. */
    group->dentry = debugfs_create_dir(name, log->group_dir);
    if (IS_ERR_OR_NULL(group->dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s' dir.\n", name);
        return;
    }

    /* Store reference to group object in inode private data. */
    group->dentry->d_inode->i_private = group;

    /* Create <group>/severity. */
    debugfs_create_u32("severity", 0600, group->dentry, &group->severity);
/*
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/severity' value.\n", name);
        return;
    }
*/
    /* Create <group>/drain. */
    dentry = debugfs_create_file("drain", 0600, group->dentry, NULL, &group_drain_fops);
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/severity' value.\n", name);
        return;
    }
}

/**
 * Add drain to log.
 *
 * @param log       Pointer to log.
 * @param name      Name of drain.
 * @param drain     Pointer to drain.
 */
static void mve_rsrc_log_drain_add(struct mve_rsrc_log *log, const char *name, struct mve_rsrc_log_drain *drain)
{
    /* Create <drain> directory. */
    drain->dentry = debugfs_create_dir(name, log->drain_dir);
    if (IS_ERR_OR_NULL(drain->dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s' dir.\n", name);
        return;
    }

    /* Store pointer to drain object in inode private data. */
    drain->dentry->d_inode->i_private = drain;
}

/**
 * Derived function to add RAM drain to log.
 *
 * @param log       Pointer to log.
 * @param name      Name of drain.
 * @param drain     Pointer to drain.
 */
static void mve_rsrc_log_drain_ram_add(struct mve_rsrc_log *log, const char *name, struct mve_rsrc_log_drain_ram *drain)
{
    static const struct file_operations drain_ram_msg =
    {
        .read = drain_ram_read_msg,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
        .aio_write = drain_ram_aio_write,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
        .write_iter = drain_ram_write_iter,
#endif
        .open = drain_ram_open,
        .unlocked_ioctl = drain_ram_ioctl
    };
    static const struct file_operations drain_ram_pipe =
    {
        .read = drain_ram_read_pipe,
#ifdef __KERNEL__
        .poll = drain_ram_poll_pipe,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
        .aio_write = drain_ram_aio_write,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
        .write_iter = drain_ram_write_iter,
#endif
        .open = drain_ram_open,
        .unlocked_ioctl = drain_ram_ioctl
    };
    static const struct file_operations drain_ram_flush_on_error =
    {
        .read = drain_ram_read_flush_on_error,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
        .aio_write = drain_ram_aio_write,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
        .write_iter = drain_ram_write_iter,
#endif
        .open = drain_ram_open,
        .unlocked_ioctl = drain_ram_ioctl
    };
    struct dentry *dentry;

    mve_rsrc_log_drain_add(log, name, &drain->base);

    /* Create non blocking dentry. */
    dentry = debugfs_create_file("msg", 0622, drain->base.dentry, NULL, &drain_ram_msg);
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/msg.\n", name);
        return;
    }

    /* Create blocking dentry. */
    dentry = debugfs_create_file("pipe", 0622, drain->base.dentry, NULL, &drain_ram_pipe);
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/pipe.\n", name);
        return;
    }

    /* Create blocking dentry. */
    dentry = debugfs_create_file("pipe_on_error", 0622, drain->base.dentry, NULL, &drain_ram_flush_on_error);
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/pipe_on_error.\n", name);
        return;
    }

    /* Create <group>/severity. */
    debugfs_create_u32("severity", 0600, drain->base.dentry, &drain->severity);
/*
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create '%s/severity' value.\n", name);
        return;
    }
*/
}
BullseyeCoverageOn

/******************************************************************************
 * Log Group
 ******************************************************************************/

/**
 * Group constructor.
 *
 * @param group     Pointer to group.
 * @param tag       Name of group, to be used in log messages.
 * @param severity  Minimum severity to output log message.
 * @param drain     Pointer to drain.
 */
static void mve_rsrc_log_group_construct(struct mve_rsrc_log_group *group, const char *tag, const enum mve_rsrc_log_severity severity, struct mve_rsrc_log_drain *drain)
{
    group->tag = tag;
    group->severity = severity;
    group->drain = drain;
}

/**
 * Group destructor.
 *
 * @param group     Pointer to group.
 */
static void mve_rsrc_log_group_destruct(struct mve_rsrc_log_group *group)
{
    UNUSED(group);
}

/******************************************************************************
 * Log Drain
 ******************************************************************************/

BullseyeCoverageOff

#ifdef MVE_LOG_FTRACE_ENABLE
static void mve_rsrc_log_drain_ftrace_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *msg, const unsigned n_args, ...)
{
    va_list args;
    char fmt[1000];

    severity = min((int)severity, MVE_LOG_VERBOSE);

    snprintf(fmt, sizeof(fmt), "%s %s: %s\n", severity_to_name[severity], tag, msg);
    fmt[sizeof(fmt) - 1] = '\0';

    va_start(args, n_args);
    ftrace_vprintk(fmt, args);
    va_end(args);
}

static void mve_rsrc_log_drain_ftrace_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    size_t i;

    trace_printk("count=%zu\n", count);

    for (i = 0; i < count; ++i)
    {
        const char *p = vec[i].iov_base;
        size_t length = vec[i].iov_len;

        trace_printk("  length=%zu\n", length);

        while (length > 0)
        {
            size_t j = min(length, (size_t)32);
            char buf[3 + j * 3 + 1];
            size_t n = 0;

            length -= j;

            n += scnprintf(&buf[n], sizeof(buf) - n, "   ");

            while (j-- > 0)
            {
                n += scnprintf(&buf[n], sizeof(buf) - n, " %02x", *p++);
            }

            trace_printk("%s\n", buf);
        }
    }
}
#endif /* MVE_LOG_FTRACE_ENABLE */

/* The dmesg print functions are designed as fallback functions if the system doesn't
 * have alog functionality. These functions will therefore most likely never be used.
 * Removing them from code coverage */

static void mve_rsrc_log_drain_dmesg_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *msg, const unsigned n_args, ...)
{
    va_list args;
    char fmt[950];

    severity = min((int)severity, MVE_LOG_VERBOSE);

    snprintf(fmt, sizeof(fmt), "%s%s: %s\n", severity_to_kern_level[severity], tag, msg);
    fmt[sizeof(fmt) - 1] = '\0';

    va_start(args, n_args);
    vprintk(fmt, args);
    va_end(args);
}

static void mve_rsrc_log_drain_dmesg_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    size_t i;

    printk("count=%zu\n", count);

    for (i = 0; i < count; ++i)
    {
        const char *p = vec[i].iov_base;
        size_t length = vec[i].iov_len;

        printk("  length=%zu\n", length);

        while (length > 0)
        {
            size_t j = min(length, (size_t)32);
//            char buf[3 + j * 3 + 1];
            char buf[100];
            size_t n = 0;

            length -= j;

            n += scnprintf(&buf[n], sizeof(buf) - n, "   ");

            while (j-- > 0)
            {
                n += scnprintf(&buf[n], sizeof(buf) - n, " %02x", *p++);
            }

            printk("%s\n", buf);
        }
    }
}
BullseyeCoverageOn

#ifdef MVE_LOG_ALOG_ENABLE
enum android_log_id
{
    LOG_ID_MAIN = 0,
    LOG_ID_RADIO = 1,
    LOG_ID_EVENTS = 2,
    LOG_ID_SYSTEM = 3,
    LOG_ID_CRASH = 4,
    LOG_ID_KERNEL = 5,
    LOG_ID_MAX
};

enum android_log_priority
{
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT
};

struct android_log_time
{
    uint32_t tv_sec;
    uint32_t tv_nsec;
}
__attribute__((__packed__));

struct android_log_header_t
{
    uint8_t id;
    uint16_t tid;
    struct android_log_time realtime;
    unsigned char priority;
}
__attribute__((__packed__));

#ifdef EMULATOR
static void mve_rsrc_log_drain_alog_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *msg, const unsigned n_args, ...)
{
    va_list args;
    char fmt[1000];
    char buf[64];
    struct timespec64 timespec;
    struct tm tm;

    ktime_get_ts64(&timespec);

    localtime_r(&timespec.tv_sec, &tm);

    snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d:%02d.%03u %5u %5u ", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned)(timespec.tv_nsec / 1000000),
             (unsigned short)getpid(), (unsigned short)pthread_self());

    severity = min((int)severity, MVE_LOG_VERBOSE);

    snprintf(fmt, sizeof(fmt), "%s%s%s %s: %s\n", severity_to_kern_level[severity], buf, severity_to_a_level[severity], tag, msg);
    fmt[sizeof(fmt) - 1] = '\0';

    va_start(args, n_args);
    vprintf(fmt, args);
    va_end(args);
}

static void mve_rsrc_log_drain_alog_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    size_t i;

    printf("count=%zu\n", count);

    for (i = 0; i < count; ++i)
    {
        const char *p = vec[i].iov_base;
        size_t length = vec[i].iov_len;

        printf("  length=%zu\n", length);

        while (length > 0)
        {
            size_t j = min(length, (size_t)32);
            char buf[3 + j * 3 + 1];
            size_t n = 0;

            length -= j;

            n += scnprintf(&buf[n], sizeof(buf) - n, "   ");

            while (j-- > 0)
            {
                n += scnprintf(&buf[n], sizeof(buf) - n, " %02x", *p++);
            }

            printf("%s\n", buf);
        }
    }
}
#else
static int mve_rsrc_log_drain_alog_connect(struct mve_rsrc_log_drain_alog *drain)
{
    struct socket *socket;
    struct sockaddr_un addr;
    int ret;

    /* Create Unix socket. */
    ret = sock_create(PF_UNIX, SOCK_DGRAM, 0, &socket);
    if (ret < 0)
    {
        printk(KERN_ERR "MVE: Failed to create socket. error=%d.\n", ret);
        return ret;
    }

    /* Connect to socket. */
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/dev/socket/logdw");

    ret = socket->ops->connect(socket, (struct sockaddr *)&addr, sizeof(addr), 0);
    if (ret < 0)
    {
        printk(KERN_ERR "MVE: Failed to connect to socket. error=%d.\n", ret);
        sock_release(socket);
        return ret;
    }

    drain->socket = socket;

    return 0;
}

static int mve_rsrc_log_drain_alog_send(struct mve_rsrc_log_drain_alog *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    static const int severity_to_priority[] = { ANDROID_LOG_FATAL, ANDROID_LOG_ERROR, ANDROID_LOG_WARN, ANDROID_LOG_INFO, ANDROID_LOG_DEBUG, ANDROID_LOG_VERBOSE };
    struct kvec kvec[count + 1];
    struct android_log_header_t header;
    struct msghdr msg;
    struct timespec64 timespec;
    int len;
    int ret;
    int i;

    ktime_get_ts64(&timespec);

    /* Make sure severity does not overflow. */
    severity = min((int)severity, MVE_LOG_VERBOSE);

    /* Fill in header. */
    header.id = LOG_ID_MAIN;
    header.tid = 0;
    header.realtime.tv_sec = timespec.tv_sec;
    header.realtime.tv_nsec = timespec.tv_nsec;
    header.priority = severity_to_priority[severity];

    /* Fill in io vector. */
    kvec[0].iov_base = &header;
    kvec[0].iov_len = sizeof(header);

    for (i = 0, len = sizeof(header); i < count; ++i)
    {
        kvec[i + 1].iov_base = vec[i].iov_base;
        kvec[i + 1].iov_len = vec[i].iov_len;
        len += vec[i].iov_len;
    }

    /* Initialize message. */
    memset(&msg, 0, sizeof(msg));

    ret = down_interruptible(&drain->sem);
    if (ret != 0)
    {
        return -EINTR;
    }

    /* Connect to socket if that has not been done before. */
    if (drain->socket == NULL)
    {
        ret = mve_rsrc_log_drain_alog_connect(drain);
        if (ret < 0)
        {
            printk(KERN_ERR "MVE: Failed to connect to Android log daemon socket. error=%d.\n", ret);
            goto out;
        }
    }

    ret = kernel_sendmsg(drain->socket, &msg, kvec, count + 1, len);
    if (ret < 0)
    {
        printk(KERN_ERR "MVE: Failed to send message to logd. error=%d.\n", ret);
        sock_release(drain->socket);
        drain->socket = NULL;
    }

out:
    up(&drain->sem);

    return ret;
}

static void mve_rsrc_log_drain_alog_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    static const char tag[] = "MVE";
    struct mve_rsrc_log_drain_alog *alog_drain = (struct mve_rsrc_log_drain_alog *)drain;
    struct iovec v[2];
    size_t i;
    char buf[1000];
    int n = 0;

    n += scnprintf(&buf[n], sizeof(buf) - n, "count=%zu\n", count);

    for (i = 0; i < count; ++i)
    {
        const char *p = vec[i].iov_base;
        size_t length = vec[i].iov_len;

        n += scnprintf(&buf[n], sizeof(buf) - n, "  length=%zu\n", length);

        while (length > 0)
        {
            size_t j = min(length, (size_t)32);

            length -= j;

            n += scnprintf(&buf[n], sizeof(buf) - n, "    ");

            while (j-- > 0)
            {
                n += scnprintf(&buf[n], sizeof(buf) - n, "%02x ", *p++);
            }

            n += scnprintf(&buf[n], sizeof(buf) - n, "\n");
        }
    }

    buf[sizeof(buf) - 1] = '\0';

    v[0].iov_base = (void *)tag;
    v[0].iov_len = sizeof(tag);

    v[1].iov_base = buf;
    v[1].iov_len = n + 1;

    mve_rsrc_log_drain_alog_send(alog_drain, severity, v, 2);
}

static void mve_rsrc_log_drain_alog_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *fmt, const unsigned n_args, ...)
{
    struct mve_rsrc_log_drain_alog *alog_drain = (struct mve_rsrc_log_drain_alog *)drain;
    struct iovec vec[2];
    va_list args;
    char msg[1000];
    size_t n;

    va_start(args, n_args);
    n = vscnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    vec[0].iov_base = (void *)tag;
    vec[0].iov_len = strlen(tag) + 1;

    vec[1].iov_base = msg;
    vec[1].iov_len = n + 1;

    mve_rsrc_log_drain_alog_send(alog_drain, severity, vec, 2);
}
#endif
#endif /* MVE_LOG_ALOG_ENABLE */

BullseyeCoverageOff

/* The RAM drain is not used for the emulator. Therefore switching off code coverage for
 * the following two drain ram functions. */
static void mve_rsrc_log_drain_ram_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    struct mve_rsrc_log_drain_ram *drain_ram = (struct mve_rsrc_log_drain_ram *)drain;
    size_t i;
    size_t length;
    size_t pos;
    int sem_taken;

    /* Calculate the total length of the output. */
    for (i = 0, length = 0; i < count; ++i)
    {
        length += vec[i].iov_len;
    }

    /* Round up to next 32-bit boundary. */
    length = (length + 3) & ~3;

    if (length > drain_ram->buffer_size)
    {
        printk(KERN_ERR "MVE: Logged data larger than output buffer. length=%zu, buffer_length=%zu.\n", length, (size_t)drain_ram->buffer_size);
        return;
    }

    sem_taken = down_interruptible(&drain_ram->sem);

    pos = drain_ram->write_pos & (drain_ram->buffer_size - 1);

    /* Loop over scatter input. */
    for (i = 0; i < count; ++i)
    {
        const char *buf = vec[i].iov_base;
        size_t len = vec[i].iov_len;

        /* Copy log message to output buffer. */
        while (len > 0)
        {
            size_t n = min(len, drain_ram->buffer_size - pos);

            memcpy(&drain_ram->buf[pos], buf, n);

            len -= n;
            buf += n;
            pos = (pos + n) & (drain_ram->buffer_size - 1);
        }
    }

    /* Update write_pos. Length has already been 4 byte aligned */
    drain_ram->write_pos += length;

    /* Flush RAM buffer if severity exceeds configured severity. */
    if (severity <= drain_ram->severity)
    {
        drain_ram->write_error_pos = drain_ram->write_pos;
    }

    if (0 == sem_taken)
    {
        up(&drain_ram->sem);
    }

    wake_up_interruptible(&drain_ram->queue);
}

static void mve_rsrc_log_drain_ram_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *msg, const unsigned n_args, ...)
{
    char buf[800];
    va_list args;
    size_t n = 0;
    struct mve_log_header header;
    struct iovec vec[2];
    struct timespec64 timespec;

    /* Write the log message. */
    va_start(args, n_args);
    n += vscnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    ktime_get_ts64(&timespec);

    header.magic = MVE_LOG_MAGIC;
    header.length = n;
    header.type = MVE_LOG_TYPE_TEXT;
    header.severity = severity;
    header.timestamp.sec = timespec.tv_sec;
    header.timestamp.nsec = timespec.tv_nsec;

    vec[0].iov_base = &header;
    vec[0].iov_len = sizeof(header);

    vec[1].iov_base = buf;
    vec[1].iov_len = n;

    mve_rsrc_log_drain_ram_data(drain, severity, vec, 2);
}

BullseyeCoverageOn

/**
 * Drain constructor.
 *
 * @param drain     Pointer to drain.
 * @param print     Print function pointer.
 * @param data      Data function pointer.
 */
static void mve_rsrc_log_drain_construct(struct mve_rsrc_log_drain *drain, mve_rsrc_print_fptr print, mve_rsrc_data_fptr data)
{
    drain->print = print;
    drain->data = data;
}

/**
 * Drain destructor.
 */
static void mve_rsrc_log_drain_destruct(struct mve_rsrc_log_drain *drain)
{
    UNUSED(drain);
}

/**
 * RAM drain constructor.
 *
 * @param drain     Pointer to drain.
 * @param print     Print function pointer.
 * @param data      Data function pointer.
 */
static void mve_rsrc_log_drain_ram_construct(struct mve_rsrc_log_drain_ram *drain, mve_rsrc_print_fptr print, mve_rsrc_data_fptr data, size_t buffer_size, enum mve_rsrc_log_severity severity)
{
    mve_rsrc_log_drain_construct(&drain->base, print, data);

    drain->buf = MVE_RSRC_MEM_VALLOC(buffer_size);
    *(size_t *) &drain->buffer_size = buffer_size;
    drain->read_pos = 0;
    drain->write_pos = 0;
    drain->write_error_pos = 0;
    drain->severity = severity;
    init_waitqueue_head(&drain->queue);
    sema_init(&drain->sem, 1);
}

/**
 * RAM drain destructor.
 *
 * @param drain     Pointer to drain.
 */
static void mve_rsrc_log_drain_ram_destruct(struct mve_rsrc_log_drain_ram *drain)
{
    MVE_RSRC_MEM_VFREE(drain->buf);

    mve_rsrc_log_drain_destruct(&drain->base);
}

#ifndef __KERNEL__
static void mve_rsrc_log_drain_file_data(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, struct iovec *vec, size_t count)
{
    struct mve_rsrc_log_drain_file *drain_file = (struct mve_rsrc_log_drain_file *)drain;
    size_t i;
    size_t total_length = 0;
    size_t padding;
    uint8_t padding_data[4] = { 0 };

    sem_wait(&drain_file->file_sem);

    /* Loop over scatter input. */
    for (i = 0; i < count; ++i)
    {
        /* Write log message to output file */
        fwrite(vec[i].iov_base, 1, vec[i].iov_len, drain_file->fp);
        total_length += vec[i].iov_len;
    }

    /* Calculate the amount of padding that must be added to the end of the entry */
    padding = ((total_length + 3) & ~3) - total_length;
    fwrite(padding_data, 1, padding, drain_file->fp);

    /* Flush output to file. */
    fflush(drain_file->fp);

    sem_post(&drain_file->file_sem);
}

static void mve_rsrc_log_drain_file_print(struct mve_rsrc_log_drain *drain, enum mve_rsrc_log_severity severity, const char *tag, const char *msg, const unsigned n_args, ...)
{
    char buf[1000];
    va_list args;
    size_t n = 0;
    struct mve_log_header header;
    struct iovec vec[2];
    struct timespec64 timespec;

    /* Write the log message. */
    va_start(args, n_args);
    n += vscnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    ktime_get_ts64(&timespec);

    header.magic = MVE_LOG_MAGIC;
    header.length = n;
    header.type = MVE_LOG_TYPE_TEXT;
    header.severity = severity;
    header.timestamp.sec = timespec.tv_sec;
    header.timestamp.nsec = timespec.tv_nsec;

    vec[0].iov_base = &header;
    vec[0].iov_len = sizeof(header);

    vec[1].iov_base = buf;
    vec[1].iov_len = n;

    mve_rsrc_log_drain_file_data(drain, severity, vec, 2);
}

static void mve_rsrc_log_drain_file_construct(struct mve_rsrc_log_drain_file *drain, mve_rsrc_print_fptr print, mve_rsrc_data_fptr data, char *filename)
{
    mve_rsrc_log_drain_construct(&drain->base, print, data);

    sem_init(&drain->file_sem, 0, 1);
    drain->fp = fopen(filename, "ab");
    if (NULL == drain->fp)
    {
        printk(KERN_ERR "MVE: Failed to open the file %s\n", filename);
    }
}

static void mve_rsrc_log_drain_file_destruct(struct mve_rsrc_log_drain_file *drain)
{
    sem_wait(&drain->file_sem);
    fclose(drain->fp);
    sem_post(&drain->file_sem);
    sem_destroy(&drain->file_sem);
    mve_rsrc_log_drain_destruct(&drain->base);
}
#endif

#ifdef MVE_LOG_ALOG_ENABLE
/**
 * Android log drain constructor.
 *
 * @param drain     Pointer to drain.
 * @param print     Print function pointer.
 * @param data      Data function pointer.
 */
static void mve_rsrc_log_drain_alog_construct(struct mve_rsrc_log_drain_alog *drain, mve_rsrc_print_fptr print, mve_rsrc_data_fptr data)
{
    mve_rsrc_log_drain_construct(&drain->base, print, data);

    drain->socket = NULL;
    sema_init(&drain->sem, 1);
}

/**
 * Android log drain destructor.
 *
 * @param drain     Pointer to drain.
 */
static void mve_rsrc_log_drain_alog_destruct(struct mve_rsrc_log_drain_alog *drain)
{
    if (drain->socket != NULL)
    {
#ifndef EMULATOR
        sock_release(drain->socket);
#endif
        drain->socket = NULL;
    }

    mve_rsrc_log_drain_destruct(&drain->base);
}
#endif /* MVE_LOG_ALOG_ENABLE */

BullseyeCoverageOff
/**
 * Log constructor.
 *
 * @param log       Pointer to log.
 */
static void mve_rsrc_log_construct(struct mve_rsrc_log *log)
{
    static const struct file_operations readme_fops =
    {
        .read = readme_read
    };
    struct dentry *dentry;

    log->mve_dir = debugfs_create_dir("mve", NULL);
    if (IS_ERR_OR_NULL(log->mve_dir))
    {
        printk(KERN_ERR "MVE: Failed to create 'mve' dir.\n");
        return;
    }
    log->mve_dir->d_inode->i_private = log;

    log->drain_dir = debugfs_create_dir("drain", log->mve_dir);
    if (IS_ERR_OR_NULL(log->drain_dir))
    {
        printk(KERN_ERR "MVE: Failed to create 'drain' dir.\n");
        goto error;
    }

    log->group_dir = debugfs_create_dir("group", log->mve_dir);
    if (IS_ERR_OR_NULL(log->group_dir))
    {
        printk(KERN_ERR "MVE: Failed to create 'group' dir.\n");
        goto error;
    }

    /* Create <group>/drain. */
    dentry = debugfs_create_file("README", 0400, log->mve_dir, NULL, &readme_fops);
    if (IS_ERR_OR_NULL(dentry))
    {
        printk(KERN_ERR "MVE: Failed to create 'README'.\n");
        return;
    }

    return;

error:
    debugfs_remove_recursive(log->mve_dir);
}
BullseyeCoverageOn

/**
 * Log destructor.
 *
 * @param log       Pointer to log.
 */
static void mve_rsrc_log_destruct(struct mve_rsrc_log *log)
{
    debugfs_remove_recursive(log->mve_dir);
}

/******************************************************************************
 * External interface
 ******************************************************************************/

void mve_rsrc_log_init3(void)
{
    struct mve_rsrc_log_drain *drain_default = &drain_dmesg;
    struct mve_rsrc_log_drain *drain_ram = &drain_ram0.base;

#ifdef MVE_LOG_ALOG_ENABLE
    drain_default = &drain_alog.base;
#endif /* MVE_LOG_ALOG_ENABLE */

#ifdef MVE_LOG_FTRACE_ENABLE
    drain_default = &drain_ftrace;
#endif /* MVE_LOG_FTRACE_ENABLE */

#ifndef __KERNEL__
    drain_ram = &drain_file.base;
#endif /* __KERNEL__ */

    /* Construct log object. */
    mve_rsrc_log_construct(&log);

    /* Construct drain objects and add them to log. */
    mve_rsrc_log_drain_construct(&drain_dmesg, mve_rsrc_log_drain_dmesg_print, mve_rsrc_log_drain_dmesg_data);
    mve_rsrc_log_drain_add(&log, "dmesg", &drain_dmesg);

    mve_rsrc_log_drain_ram_construct(&drain_ram0, mve_rsrc_log_drain_ram_print, mve_rsrc_log_drain_ram_data, 64 * 1024, MVE_LOG_ERROR);
    mve_rsrc_log_drain_ram_add(&log, "ram0", &drain_ram0);

#ifndef __KERNEL__
    mve_rsrc_log_drain_file_construct(&drain_file, mve_rsrc_log_drain_file_print, mve_rsrc_log_drain_file_data, "fw.log");
#endif /* __KERNEL__ */

#ifdef MVE_LOG_ALOG_ENABLE
    mve_rsrc_log_drain_alog_construct(&drain_alog, mve_rsrc_log_drain_alog_print, mve_rsrc_log_drain_alog_data);
    mve_rsrc_log_drain_add(&log, "alog", &drain_alog.base);
#endif /* MVE_LOG_ALOG_ENABLE */

#ifdef MVE_LOG_FTRACE_ENABLE
    mve_rsrc_log_drain_construct(&drain_ftrace, mve_rsrc_log_drain_ftrace_print, mve_rsrc_log_drain_ftrace_data);
    mve_rsrc_log_drain_add(&log, "ftrace", &drain_ftrace);
#endif /* MVE_LOG_FTRACE_ENABLE */

    /* Construct group objects. */
    mve_rsrc_log_group_construct(&mve_rsrc_log3, "MVE", MVE_LOG_WARNING, drain_default);
    mve_rsrc_log_group_add(&log, "generic", &mve_rsrc_log3);

    mve_rsrc_log_group_construct(&mve_rsrc_log_scheduler3, "MVE scheduler", MVE_LOG_WARNING, drain_default);
    mve_rsrc_log_group_add(&log, "scheduler", &mve_rsrc_log_scheduler3);

    mve_rsrc_log_group_construct(&mve_rsrc_log_fwif3, "MVE fwif", MVE_LOG_INFO, drain_ram);
    mve_rsrc_log_group_add(&log, "firmware_interface", &mve_rsrc_log_fwif3);

    mve_rsrc_log_group_construct(&mve_rsrc_log_session3, "MVE session", MVE_LOG_WARNING, drain_default);
    mve_rsrc_log_group_add(&log, "session", &mve_rsrc_log_session3);
}

void mve_rsrc_log_destroy3(void)
{
    /* Destroy objects in reverse order. */
    mve_rsrc_log_destruct(&log);

    mve_rsrc_log_group_destruct(&mve_rsrc_log3);
    mve_rsrc_log_group_destruct(&mve_rsrc_log_scheduler3);
    mve_rsrc_log_group_destruct(&mve_rsrc_log_fwif3);
    mve_rsrc_log_group_destruct(&mve_rsrc_log_session3);

    mve_rsrc_log_drain_destruct(&drain_dmesg);
    mve_rsrc_log_drain_ram_destruct(&drain_ram0);

#ifndef __KERNEL__
    mve_rsrc_log_drain_file_destruct(&drain_file);
#endif /* __KERNEL__ */

#ifdef MVE_LOG_ALOG_ENABLE
    mve_rsrc_log_drain_alog_destruct(&drain_alog);
#endif /* MVE_LOG_ALOG_ENABLE */

#ifdef MVE_LOG_FTRACE_ENABLE
    mve_rsrc_log_drain_destruct(&drain_ftrace);
#endif /* MVE_LOG_FTRACE_ENABLE */
}

const char *mve_rsrc_log_strrchr3(const char *s)
{
    const char *p = strrchr(s, '/');

    return (p == NULL) ? s : p + 1;
}

struct dentry *mve_rsrc_log_get_parent_dir3(void)
{
    return log.mve_dir;
}

EXPORT_SYMBOL(mve_rsrc_log_strrchr3);
EXPORT_SYMBOL(mve_rsrc_log_get_parent_dir3);
