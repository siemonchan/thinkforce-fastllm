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

#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_register.h"
#include "mve_rsrc_irq.h"
#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_circular_buffer.h"
#include "mve_rsrc_log.h"
#include "mve_rsrc_dvfs.h"

#ifdef EMULATOR
#else
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/sysfs.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/stat.h>
#endif

#define MAX_LSID 4                      /* Maximum supported LSIDs */
#define JOBQUEUE_SIZE 4                 /* Number of entries in the MVE job queue */
#define CORESCHED_CORELSID_ENTRY_SIZE 4 /* Number of bits used by each entry in the CORELSID */
#define CORESCHED_JOBQUEUE_ENTRY_SIZE 8 /* Number of bits used by each entry in the JOBQUEUE */

#define MAX_SESSION_INSTANCES_IN_HW_QUEUE 2

/* When attempting to schedule a new session, if the session is not scheduled
 * immediately, wait for 15 ms until giving up and returning to the caller. */
#define UNSCHEDULED_WAIT_MS 15

/* When switching out a session, the driver repeatedly reads the CORELSID
 * register to find out when a LSID is no longer executed. This define
 * controls how many times this process shall be iterated before giving up. */
#ifdef EMULATOR
#define WAIT_FOR_REBOOT_NTRIES 100000
#define WAIT_FOR_TERMINATE_NTRIES 100000
#else
#define WAIT_FOR_REBOOT_NTRIES 100
#define WAIT_FOR_TERMINATE_NTRIES 1000
#endif

/* Constant defining an empty HW job queue */
#define JOBQUEUE_EMPTY ((JOBQUEUE_JOB_INVALID << 24) | (JOBQUEUE_JOB_INVALID << 16) | (JOBQUEUE_JOB_INVALID << 8) | (JOBQUEUE_JOB_INVALID))

/**
 * Structure used by the scheduler to keep track of the active sessions.
 */
struct mve_session_entry
{
    mver_session_id session_id;                                            /**< Base session ID */
    enum LSID lsid;                                                        /**< LSID allocated to the session */

    uint32_t mmu_ctrl;                                                     /**< The value to write to the MMU_CTRL register */
    int ncores;                                                            /**< Number of cores this session is allowed to use */
    bool secure;                                                           /**< Session is secure */
    int enqueues;                                                          /**< Number of times this session is enqueued */

    irq_callback_fptr irq_callback;                                        /**< IRQ callback function */

    session_has_work_fptr has_work_callback;                               /**< Query the session if it can be switched out */
    session_switchout_fptr switchout_callback;                             /**< Notify the session that it's about to be switched out */
    session_switchin_fptr switchin_callback;                               /**< Notify the session that it's about to be switched in */
    session_switchout_completed_fptr switchout_complete_callback;          /**< Notify session that it has been switched out */
    session_get_restricting_buffer_count_fptr
        get_restricting_buffer_count_callback;                             /**< Get amount of restricting buffers enqueued to the FW */

    struct list_head sessions_list;                                        /**< Sessions linked list */
    struct semaphore scheduled_sem;                                        /**< Semaphore that signals whether the session is scheduled or not */
};

/* List of sessions registered with the scheduler */
static struct list_head sessions;

/* List of sessions that are waiting to be scheduled for execution but are currently
 * not executing. This is because there are no available LSIDs or hardware job queue entries. */
static struct mver_circular_buffer *queued_sessions;

/* LSID to session mapping */
static struct mve_session_entry *session_by_lsid[MAX_LSID] = { NULL };

/* Semaphore used to prevent concurrent access to the scheduler. Note that
 * this mutex must not be taken while making irq_callback calls. */
static struct semaphore scheduler_sem;

/* Used by the suspend/resume code to prevent new sessions from being
 * scheduled when the system is being suspended. */
static bool scheduling_enabled = true;

/* Indicates when a session couldn't be switched in because all LSIDs are
 * taken. */
static bool no_free_lsid = false;

/**
 * Returns the session instance corresponding to the supplied base session ID.
 * @param session_id Base ID of the session.
 * @return The session instance if one exists, NULL otherwise.
 */
static struct mve_session_entry *get_session_cache_entry(mver_session_id session_id)
{
    struct mve_session_entry *entry = NULL;
    struct list_head *pos;

    list_for_each(pos, &sessions)
    {
        entry = container_of(pos, struct mve_session_entry, sessions_list);

        if (entry->session_id == session_id)
        {
            return entry;
        }
    }

    return NULL;
}

/**
 * Disable hardware job scheduling.
 * @param regs Pointer to register mapping.
 */
static void disable_scheduling(tCS *regs)
{
    uint32_t val;

    mver_reg_write326(&regs->ENABLE, 0);
    do
    {
        val = mver_reg_read326(&regs->ENABLE);
    }
    while (0 != val);
}

/**
 * Enable hardware job scheduling.
 * @param regs Pointer to register mapping.
 */
static void enable_scheduling(tCS *regs)
{
    wmb();
    mver_reg_write326(&regs->ENABLE, 1);
}

/**
 * Returns the currently executing LSID. Note that this function assumes that
 * only one LSID can execute at a time which is true for this implementation.
 * @return The currently executing LSID
 */
static enum LSID jobqueue_currently_executing(void)
{
    tCS *regs;
    uint32_t core_lsid;
    enum LSID lsid = NO_LSID;
    int ncores, i;

    regs = mver_reg_get_coresched_bank6();
    core_lsid = mver_reg_read326(&regs->CORELSID);
    mver_reg_put_coresched_bank6(&regs);

#ifdef EMULATOR
    ncores = 8;
#else
    ncores = mver_scheduler_get_ncores6();
#endif

    /* Find a LSID that is currently executing */
    for (i = 0; i < ncores; ++i)
    {
        if ((core_lsid & 0xF) != 0xF)
        {
            lsid = core_lsid & 0xF;
            break;
        }

        core_lsid >>= CORESCHED_CORELSID_ENTRY_SIZE;
    }

    return lsid;
}

/**
 * Check if the supplied session is enqueued in the HW job queue or is currently
 * executing.
 * @param session The session to check
 * @return True if the session can be found in the HW job queue or in the CORELSID.
 *         False if not.
 */
static bool session_is_executing(struct mve_session_entry *session)
{
    tCS *regs;
    uint32_t job_queue;
    uint32_t core_lsid;
    int i;
    int ncores;

#ifdef EMULATOR
    ncores = 8;
#else
    ncores = mver_scheduler_get_ncores6();
#endif

    regs = mver_reg_get_coresched_bank6();
    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    core_lsid = mver_reg_read326(&regs->CORELSID);
    mver_reg_put_coresched_bank6(&regs);

    /* Is the LSID currently executing? */
    for (i = 0; i < ncores; ++i)
    {
        if (((core_lsid >> (i * CORESCHED_CORELSID_ENTRY_SIZE)) & 0xF) == session->lsid)
        {
            return true;
        }
    }

    /* Is the LSID present in the HW job queue? */
    for (i = 0; i < JOBQUEUE_SIZE; ++i)
    {
        if (((job_queue >> (i * CORESCHED_JOBQUEUE_ENTRY_SIZE)) & 0x0F) == session->lsid)
        {
            return true;
        }
    }

    return false;
}

/**
 * Checks whether the session is enqueued to the HW job queue or not.
 * @param session The session to check.
 * @return True if the session is enqueued, false if not.
 */
static bool jobqueue_is_enqueued(struct mve_session_entry *session)
{
    tCS *regs;
    uint32_t job_queue;
    int i;

    regs = mver_reg_get_coresched_bank6();
    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    mver_reg_put_coresched_bank6(&regs);

    /* Is the LSID present in the HW job queue? */
    for (i = 0; i < JOBQUEUE_SIZE; ++i)
    {
        if (((job_queue >> (i * CORESCHED_JOBQUEUE_ENTRY_SIZE)) & 0x0F) == session->lsid)
        {
            return true;
        }
    }

    return false;
}

/**
 * Check if there are any vacant positions in the HW job queue.
 * @return True if there is at least one vacant position in the HW job queue.
 *         False otherwise.
 */
static bool jobqueue_is_free(void)
{
    tCS *regs;
    uint32_t job_queue;
    bool is_free;

    regs = mver_reg_get_coresched_bank6();
    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    mver_reg_put_coresched_bank6(&regs);

    is_free = (((job_queue >> 24) & 0xFF) == JOBQUEUE_JOB_INVALID);

    return is_free;
}

/**
 * Check whether the job queue is empty or not.
 * @return True if the job queue is empty, false otherwise.
 */
static bool jobqueue_is_empty(void)
{
    tCS *regs;
    uint32_t job_queue;

    regs = mver_reg_get_coresched_bank6();
    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    mver_reg_put_coresched_bank6(&regs);

    return JOBQUEUE_EMPTY == job_queue;
}

/**
 * Returns the number of times this LSID can be found in the HW job queue.
 * @param lsid The lsid
 * @return The number of times this LSID can be found in the HW job queue
 */
static int jobqueue_number_enqueues(enum LSID lsid)
{
    tCS *regs;
    uint32_t job_queue;
    int nr = 0;
    int i;

    regs = mver_reg_get_coresched_bank6();
    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    mver_reg_put_coresched_bank6(&regs);

    for (i = 0; i < JOBQUEUE_SIZE; ++i)
    {
        if (((job_queue >> (i * CORESCHED_JOBQUEUE_ENTRY_SIZE)) & 0x0F) == lsid)
        {
            nr++;
        }
    }

    return nr;
}

/**
 * Enqueue the session to the HW job queue.
 * @param session The session to enqueue.
 * @return True if the session was enqueued to the HW job queue, false otherwise.
 */
static bool jobqueue_enqueue_session(struct mve_session_entry *session)
{
    tCS *regs;
    enum LSID lsid;
    uint32_t job_queue, ncores;
    unsigned int i;
    bool job_added = false;

    lsid = session->lsid;
    ncores = session->ncores;

    regs = mver_reg_get_coresched_bank6();
    disable_scheduling(regs);

    job_queue = mver_reg_read326(&regs->JOBQUEUE);

    for (i = 0; i < JOBQUEUE_SIZE; ++i)
    {
        if (((job_queue >> (i * CORESCHED_JOBQUEUE_ENTRY_SIZE)) & 0xFF) == JOBQUEUE_JOB_INVALID)
        {
            /* This job queue entry is unused! */
            job_queue = (job_queue & (~(0xFF << (i * 8)))) | /* Clear all bits for this job entry */
                        (lsid << (i * 8)) |                  /* Add the LSID number */
                        ((ncores - 1) << (i * 8 + 4));       /* Add the number of cores to use */
            job_added = true;
            break;
        }
    }

    if (false != job_added)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_DEBUG, "Enqueueing session. session=%p, lsid=%u, jobqueue=0x%08X, corelsid=0x%X.",
                      session->session_id, lsid, job_queue, mver_reg_read326(&regs->CORELSID));
        mver_reg_write326(&regs->JOBQUEUE, job_queue);
    }

    enable_scheduling(regs);
    mver_reg_put_coresched_bank6(&regs);

    return job_added;
}

/**
 * Remove all occurences of the LSID from the HW job queue. This function packs
 * the remaining job queue entries in the lower bits of the JOBQUEUE register.
 * This is needed since the MVE does not skip empty entries in the queue.
 * Note that this function assumes scheduling has been disabled!
 * @param regs Pointer to register mapping.
 * @param lsid The LSID to dequeue.
 */
static void jobqueue_dequeue_lsid(tCS *regs, enum LSID lsid)
{
    uint32_t job_queue;
    uint32_t new_job_queue = 0x0F0F0F0F;
    int i;

    job_queue = mver_reg_read326(&regs->JOBQUEUE);
    for (i = 0; i < 4; ++i)
    {
        uint32_t job = job_queue >> 24;
        if (lsid != (job & 0x0F))
        {
            new_job_queue = (new_job_queue << 8) | job;
        }
        job_queue <<= 8;
    }
    mver_reg_write326(&regs->JOBQUEUE, new_job_queue);
}

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
#include "mve_rsrc_pm.h"

static ssize_t sysfs_print_registered_sessions(struct device *dev,
                                               struct device_attribute *attr,
                                               char *buf)
{
    ssize_t num = 0;
    struct mve_session_entry *entry = NULL;
    struct list_head *pos;
    int i = 0;

    num += snprintf(buf, PAGE_SIZE, "Registered sessions\n");
    list_for_each(pos, &sessions)
    {
        entry = container_of(pos, struct mve_session_entry, sessions_list);

        num += snprintf(buf + num, PAGE_SIZE - num, "%d: ID %p\n", i, entry->session_id);
        i++;
    }

    return num;
}

static ssize_t sysfs_print_queued_sessions(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    ssize_t num = 0;
    struct mve_session_entry *entry = NULL;
    int i;

    num += snprintf(buf, PAGE_SIZE, "Queued sessions\n");
    mver_circular_buffer_for_each(i, entry, struct mve_session_entry *, queued_sessions)
    {
        num += snprintf(buf + num, PAGE_SIZE - num, "%d: ID %p\n", i, entry->session_id);
    }

    return num;
}

static ssize_t sysfs_print_running_sessions(struct device *dev,
                                            struct device_attribute *attr,
                                            char *buf)
{
    ssize_t num = 0;
    struct mve_session_entry *entry = NULL;
    int i = 0;

    mver_pm_request_resume6();

    num += snprintf(buf, PAGE_SIZE, "Running sessions\n");
    for (i = 0; i < mve_rsrc_data6.nlsid; ++i)
    {
        entry = session_by_lsid[i];

        if (NULL != entry)
        {
            num += snprintf(buf + num, PAGE_SIZE - num, "%d: ID %p State %d enqueues: %d running: %s\n", i,
                            entry->session_id, entry->has_work_callback(entry->session_id),
                            entry->enqueues,
                            session_is_executing(entry) ? "true" : "false");
        }
        else
        {
            num += snprintf(buf + num, PAGE_SIZE - num, "%d:\n", i);
        }
    }

    mver_pm_request_suspend6();

    return num;
}

static ssize_t sysfs_print_hw_registers(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
    ssize_t num = 0;
    tCS *regs;
    int i;

    mver_pm_request_resume6();

    num += snprintf(buf, PAGE_SIZE, "HW registers\n");

    regs = mver_reg_get_coresched_bank6();

    num += snprintf(buf + num, PAGE_SIZE - num, "VERSION  0x%X\n", mver_reg_read326(&regs->VERSION));
    num += snprintf(buf + num, PAGE_SIZE - num, "ENABLE   0x%X\n", mver_reg_read326(&regs->ENABLE));
    num += snprintf(buf + num, PAGE_SIZE - num, "NCORES   0x%X\n", mver_reg_read326(&regs->NCORES));
    num += snprintf(buf + num, PAGE_SIZE - num, "CORELSID 0x%X\n", mver_reg_read326(&regs->CORELSID));
    num += snprintf(buf + num, PAGE_SIZE - num, "JOBQUEUE 0x%X\n", mver_reg_read326(&regs->JOBQUEUE));
    num += snprintf(buf + num, PAGE_SIZE - num, "IRQVE    0x%X\n", mver_reg_read326(&regs->IRQVE));
    num += snprintf(buf + num, PAGE_SIZE - num, "CLKFORCE 0x%X\n", mver_reg_read326(&regs->CLKFORCE));

    for (i = 0; i < mve_rsrc_data6.nlsid; ++i)
    {
        num += snprintf(buf + num, PAGE_SIZE - num, "\nLSID %d\n", i);
        num += snprintf(buf + num, PAGE_SIZE - num, "    CTRL       0x%X\n", mver_reg_read326(&regs->LSID[i].CTRL));
        num += snprintf(buf + num, PAGE_SIZE - num, "    MMU_CTRL   0x%X\n", mver_reg_read326(&regs->LSID[i].MMU_CTRL));
        num += snprintf(buf + num, PAGE_SIZE - num, "    NPROT      0x%X\n", mver_reg_read326(&regs->LSID[i].NPROT));
        num += snprintf(buf + num, PAGE_SIZE - num, "    ALLOC      0x%X\n", mver_reg_read326(&regs->LSID[i].ALLOC));
        num += snprintf(buf + num, PAGE_SIZE - num, "    SCHED      0x%X\n", mver_reg_read326(&regs->LSID[i].SCHED));
        num += snprintf(buf + num, PAGE_SIZE - num, "    TERMINATE  0x%X\n", mver_reg_read326(&regs->LSID[i].TERMINATE));
        num += snprintf(buf + num, PAGE_SIZE - num, "    IRQVE      0x%X\n", mver_reg_read326(&regs->LSID[i].IRQVE));
        num += snprintf(buf + num, PAGE_SIZE - num, "    IRQHOST    0x%X\n", mver_reg_read326(&regs->LSID[i].IRQHOST));
        num += snprintf(buf + num, PAGE_SIZE - num, "    STREAMID   0x%X\n", mver_reg_read326(&regs->LSID[i].STREAMID));
        num += snprintf(buf + num, PAGE_SIZE - num, "    BUSATTR[0] 0x%X\n", mver_reg_read326(&regs->LSID[i].BUSATTR[0]));
        num += snprintf(buf + num, PAGE_SIZE - num, "    BUSATTR[1] 0x%X\n", mver_reg_read326(&regs->LSID[i].BUSATTR[1]));
        num += snprintf(buf + num, PAGE_SIZE - num, "    BUSATTR[2] 0x%X\n", mver_reg_read326(&regs->LSID[i].BUSATTR[2]));
        num += snprintf(buf + num, PAGE_SIZE - num, "    BUSATTR[3] 0x%X\n", mver_reg_read326(&regs->LSID[i].BUSATTR[3]));
        num += snprintf(buf + num, PAGE_SIZE - num, "\n");
    }

    mver_reg_put_coresched_bank(&regs);

    mver_pm_request_suspend();

    return num;
}

/**
 * Backup the contents of the session RAM for a specified LSID.
 * @param session_ram Buffer to store the session RAM in.
 * @param regs        Pointer to register mapping.
 * @param lsid        The LSID of the session to backup.
 */
static void session_ram_preserve(uint32_t *session_ram, tCS *regs, int lsid)
{
    uint32_t size_in_words = SESSIONRAM_SIZE_PER_LSID / sizeof(uint32_t);
    uint32_t offset = lsid * size_in_words;
    uint32_t i;

    for (i = 0; i < size_in_words; i++)
    {
        session_ram[i] = mver_reg_read326(&regs->SESSIONRAM[offset + i]);
    }
}

static ssize_t sysfs_dump_session_ram(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
    ssize_t num = 0;

    if (0 == list_empty(&sessions))
    {
        tCS *regs;
        char *tmpBuf = MVE_RSRC_MEM_ZALLOC(PAGE_SIZE, GFP_KERNEL);

        mver_pm_request_resume6();

        regs = mver_reg_get_coresched_bank6();
        session_ram_preserve((uint32_t *)tmpBuf, regs, 0);
        mver_reg_put_coresched_bank6(&regs);

        /*
         * Binary dump of 4095 bytes
         */
        for (num = 0; num < PAGE_SIZE - 1; num++)
        {
            *(buf + num) = tmpBuf[num];
        }

        mver_pm_request_suspend6();

        MVE_RSRC_MEM_FREE(tmpBuf);
    }

    return num;
}

#ifdef UNIT
static ssize_t sysfs_print_cores(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    ssize_t num = 0;

    num += snprintf(buf, PAGE_SIZE, "%d\n", mve_rsrc_data6.ncore);
    return num;
}

ssize_t sysfs_set_cores6(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    uint32_t num_cores;
    uint32_t phys_cores;
    int ret;
    tCS *regs;

    /* Convert string to int. */
    ret = kstrtouint(buf, 10, &num_cores);

    mver_pm_request_resume6();

    /* Read number of cores from register. */
    regs = mver_reg_get_coresched_bank6();
    phys_cores = mver_reg_read326(&regs->NCORES);
    mver_reg_put_coresched_bank6(&regs);

    mver_pm_request_suspend6();

    /* If requested number of cores is invalid, then set max number of physical cores. */
    if (0 != ret || 0 == num_cores || num_cores > phys_cores)
    {
        num_cores = phys_cores;
    }

    /* Remember core count. */
    mve_rsrc_data6.ncore = num_cores;

    return count;
}
#endif

static struct device_attribute sysfs_files[] =
{
    __ATTR(registered_sessions, S_IRUGO, sysfs_print_registered_sessions, NULL),
    __ATTR(queued_sessions,     S_IRUGO, sysfs_print_queued_sessions, NULL),
    __ATTR(running_sessions,    S_IRUGO, sysfs_print_running_sessions, NULL),
    __ATTR(hw_regs,             S_IRUGO, sysfs_print_hw_registers, NULL),
    __ATTR(dump_session_ram,    S_IRUGO, sysfs_dump_session_ram, NULL),
#ifdef UNIT
    __ATTR(num_cores,           S_IRUGO | S_IWUSR, sysfs_print_cores, sysfs_set_cores),
#endif
};

#endif /* CONFIG_SYSFS && _DEBUG */

/**
 * Get session status for DVFS purposes.
 *
 * This function will be used by DVFS module to retrieve a status of sessions.
 * DVFS will use this information to control clock frequency of MVE cores.
 *
 * @param session_id Session id
 * @param status Pointer to a structure where to put the status
 * @return True when session status was retrieved successfully,
 *         False, otherwise
 */
static bool get_session_status(mver_session_id session_id,
                               struct mver_dvfs_session_status *status)
{
    bool res = false;
    struct mve_session_entry *entry;
    int sem_taken;
    session_get_restricting_buffer_count_fptr get_restricting_buffer_count_callback = NULL;

    sem_taken = down_interruptible(&scheduler_sem);

    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        get_restricting_buffer_count_callback = entry->get_restricting_buffer_count_callback;
    }

    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }

    if (NULL != get_restricting_buffer_count_callback)
    {
        status->restricting_buffer_count = get_restricting_buffer_count_callback(session_id);
        if (status->restricting_buffer_count >= 0)
        {
            res = true;
        }
    }

    return res;
}

void mver_scheduler_init6(struct device *dev)
{
    INIT_LIST_HEAD(&sessions);
    sema_init(&scheduler_sem, 1);
    queued_sessions = mver_circular_buffer_create6(64);

    mver_dvfs_init6(dev, get_session_status);
#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    {
        int err;
        int i;

        for (i = 0; i < NELEMS(sysfs_files); ++i)
        {
            err = device_create_file(dev, &sysfs_files[i]);
            if (err < 0)
            {
                MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_ERROR, "Unable to create sysfs file.");
            }
        }
    }
#endif
}

void mver_scheduler_deinit6(struct device *dev)
{
    int sem_taken;

    mver_dvfs_deinit6(dev);
    /* Destroy queued sessions. */
    sem_taken = down_interruptible(&scheduler_sem);

    mver_circular_buffer_destroy6(queued_sessions);
    queued_sessions = NULL;

    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }

#ifndef EMULATOR
#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    {
        int i;

        for (i = 0; i < NELEMS(sysfs_files); ++i)
        {
            device_remove_file(dev, &sysfs_files[i]);
        }
    }
#endif
#endif
}

/**
 * Switch out a running session and unmap the LSID. The session is notified
 * that it has been switched out.
 * @param session The session to switch out.
 */
static void switch_out_session(struct mve_session_entry *session)
{
    tCS *regs;
    enum LSID lsid = session->lsid;
    uint32_t val;

    regs = mver_reg_get_coresched_bank6();

    val = mver_reg_read326(&regs->LSID[lsid].ALLOC);
    WARN_ON(val == 0);
    if (0 != val)
    {
        int sem_taken;
        uint32_t ncore;
        uint32_t core;
        uint32_t corelsid_mask;
        int ntries = WAIT_FOR_REBOOT_NTRIES;
        enum SCHEDULE_STATE state;

        /* Unallocate the LSID using the MVE register interface */
        /* Remove all occurences of this sessions LSID from the job queues */
        disable_scheduling(regs);
        jobqueue_dequeue_lsid(regs, lsid);
        enable_scheduling(regs);

        /* Disable job scheduling for this LSID */
        mver_reg_write326(&regs->LSID[lsid].SCHED, 0);

        ncore = mver_reg_read326(&regs->NCORES);

        /* Make sure all firmware memory writes have completed */
        do
        {
            /* Get the current mask of which core is running what */
            corelsid_mask =  mver_reg_read326(&regs->CORELSID);

            /* Iterate over all cores in the mask and check if they
             * are running our lsid. */
            for (core = 0; core < ncore; ++core)
            {
                if ((corelsid_mask & 0xF) == lsid)
                {
                    /* Our lsid is still running */
                    break;
                }
                /* Shift to next core */
                corelsid_mask = corelsid_mask >> 4;
            }
        }
        while (core != ncore && --ntries > 0);

        WARN_ON(ntries == 0);

        mver_reg_write326(&regs->LSID[lsid].ALLOC, 0);
        session_by_lsid[session->lsid] = NULL;
        session->lsid = NO_LSID;

        MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_DEBUG, "Switching out session. session=%p, lsid=%u, corelsid=0x%X.",
                      session->session_id, lsid, mver_reg_read326(&regs->CORELSID));

        mver_reg_put_coresched_bank6(&regs);

        /* The sessions's scheduled_sem must be taken here or the session
         * will think it is scheduled while in fact it is not. */
        sem_taken = down_interruptible(&session->scheduled_sem);

        session->switchout_complete_callback(session->session_id);

        state = session->has_work_callback(session->session_id);
        if (SCHEDULE_STATE_SLEEP != state && 0 == session->enqueues)
        {
            /* The session has just been switched out but it has more work to do. */
            mver_circular_buffer_add6(queued_sessions, session);
            session->enqueues++;
        }

        no_free_lsid = false;
    }
    else
    {
        mver_reg_put_coresched_bank6(&regs);
    }
}

/**
 * Performs all the required HW interaction except enqueuing an entry to the
 * hardware job queue to switch in a session.
 * @param session The session to switch in.
 * @param lsid Logical session ID to use for the session.
 * @return True if the session was mapped, false otherwise.
 */
static bool map_session(struct mve_session_entry *session, enum LSID lsid)
{
    tCS *regs;
    uint32_t val;
    int ntries;
    bool ret = true;
    uint32_t disallow_cores = 0;
    int ncores;

    session->lsid = lsid;
    session_by_lsid[lsid] = session;

    regs = mver_reg_get_coresched_bank6();

    WARN_ON(mver_reg_read326(&regs->LSID[lsid].ALLOC) != 0);

    if (false == session->secure)
    {
        mver_reg_write326(&regs->LSID[lsid].ALLOC, 1);
    }
    else
    {
        mver_reg_write326(&regs->LSID[lsid].ALLOC, 2);
    }

    /* Mali-V550: the session RAM is cleared when the ALLOC register is written.
     * Poll the TERMINATE register to find out when the RAM has been cleared. */
    mver_reg_write326(&regs->LSID[lsid].TERMINATE, 1);
    ntries = WAIT_FOR_TERMINATE_NTRIES;
    do
    {
        val = mver_reg_read326(&regs->LSID[lsid].TERMINATE);
    }
    while (0 != val && --ntries);

    if (0 != val)
    {
        /* Hardware doesn't respond */
        WARN_ON(true);
        mver_reg_write326(&regs->LSID[lsid].ALLOC, 0);
        ret = false;
        goto out;
    }

    /* Mask out cores that should not be used. */
    ncores = mver_scheduler_get_ncores6();
    disallow_cores = (0xFFFFFFFF << ncores) & ((1 << CORESCHED_LSID_CTRL_DISALLOW_SZ) - 1);

    /* Write address of the L1 page */
    mver_reg_write326(&regs->LSID[lsid].MMU_CTRL, session->mmu_ctrl);
    mver_reg_write326(&regs->LSID[lsid].CTRL, disallow_cores | (session->ncores << CORESCHED_LSID_CTRL_MAXCORES));

    mver_reg_write326(&regs->LSID[lsid].BUSATTR[0], mve_rsrc_data6.port_attributes[0]);
    mver_reg_write326(&regs->LSID[lsid].BUSATTR[1], mve_rsrc_data6.port_attributes[1]);
    mver_reg_write326(&regs->LSID[lsid].BUSATTR[2], mve_rsrc_data6.port_attributes[2]);
    mver_reg_write326(&regs->LSID[lsid].BUSATTR[3], mve_rsrc_data6.port_attributes[3]);

    mver_reg_write326(&regs->LSID[lsid].FLUSH_ALL, 0);

    /* Clear all interrupts */
    mver_reg_write326(&regs->LSID[lsid].IRQVE, 0);
    mver_reg_write326(&regs->LSID[lsid].IRQHOST, 0);
    wmb();
    mver_reg_write326(&regs->LSID[lsid].SCHED, 1);

out:
    mver_reg_put_coresched_bank6(&regs);
    return ret;
}

/**
 * Returns a free LSID.
 * @return Free LSID
 */
static enum LSID find_free_lsid(void)
{
    enum LSID lsid = NO_LSID;
    int i;

    for (i = 0; i < mve_rsrc_data6.nlsid; ++i)
    {
        if (NULL == session_by_lsid[i])
        {
            lsid = i;
            break;
        }
    }

    return lsid;
}

/**
 * Performs everything required to switch in a session. The session is
 * notified that it's about to be switched in.
 * @param session The session to switch in.
 * @param lsid    The LSID to schedule the session on.
 * @return True if this session was scheduled, false if not.
 */
static bool switch_in_session(struct mve_session_entry *session)
{
    bool queue_free;
    enum LSID free_lsid;
    int nr_enqueues;
    bool res;

    if (false == scheduling_enabled)
    {
        return false;
    }

    queue_free = jobqueue_is_free();
    free_lsid = find_free_lsid();

    if (false == queue_free)
    {
        return false;
    }

    if (NO_LSID == session->lsid)
    {
        bool res;

        if (NO_LSID == free_lsid)
        {
            no_free_lsid = true;
            return false;
        }

        res = map_session(session, free_lsid);
        if (false == res)
        {
            /* Failed to map session. Try again later */
            return false;
        }
        /* Notify that this session has been scheduled */
        up(&session->scheduled_sem);
    }

    nr_enqueues = jobqueue_number_enqueues(session->lsid);
    if (MAX_SESSION_INSTANCES_IN_HW_QUEUE <= nr_enqueues)
    {
        /* Don't let any session occupy more than MAX_SESSION_INSTANCES_IN_HW_QUEUE entries in the HW queue */
        return false;
    }

    /* Notify the session that it has been switched in */
    session->switchin_callback(session->session_id);

    MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_DEBUG, "Switching in session. session=%p, lsid=%u, enqueued=%u.", session->session_id, session->lsid, nr_enqueues);

    res = jobqueue_enqueue_session(session);
    WARN_ON(false == res);

    return true;
}

/**
 * Switch in as many pending sessions as possible.
 */
static void switch_in_pending_sessions(void)
{
    struct mve_session_entry *session;
    uint32_t num;
    bool res;
    bool queue_empty;

    num = mver_circular_buffer_get_num_entries6(queued_sessions);
    if (0 == num)
    {
        /* No pending sessions */
        return;
    }

    do
    {
        mver_circular_buffer_peek6(queued_sessions, (void **)&session);
        res = switch_in_session(session);
        if (false != res)
        {
            mver_circular_buffer_remove6(queued_sessions, (void **)&session);
            session->enqueues--;
            mver_irq_signal_mve6(session->session_id);
        }
    }
    while (false != res && --num > 0);

    /* Evict the currently executing session if it's idle */
    queue_empty = jobqueue_is_empty();
    if (queue_empty == false)
    {
        enum LSID lsid;

        lsid = jobqueue_currently_executing();
        if (lsid != NO_LSID)
        {
            enum SCHEDULE_STATE state;
            struct mve_session_entry *entry;

            entry = session_by_lsid[lsid];
            state = entry->has_work_callback(entry->session_id);
            if (SCHEDULE_STATE_IDLE == state)
            {
                entry->switchout_callback(entry->session_id, true);
            }
        }
    }
}

/**
 * Switch out sessions that are no longer executing and unmap their LSIDs
 */
static void switch_out_pending_sessions(void)
{
    enum LSID lsid;

    for (lsid = LSID_0; lsid < NELEMS(session_by_lsid); ++lsid)
    {
        struct mve_session_entry *entry = session_by_lsid[lsid];
        if (NULL != entry)
        {
            bool res;

            res = session_is_executing(entry);
            if (false == res)
            {
                switch_out_session(entry);
            }
        }
    }

    switch_in_pending_sessions();
}

/**
 * Stop a running session and attempt to start a pending session
 * on the same LSID as the stopped session. If the session is not running,
 * this function doesn't do anything.
 * @param session      The session to stop.
 */
static void stop_session(struct mve_session_entry *session)
{
    enum LSID lsid = session->lsid;

    if (NO_LSID != lsid)
    {
        tCS *regs;
        int ignore_me;

        regs = mver_reg_get_coresched_bank6();
        if (0 != mver_reg_read326(&regs->LSID[lsid].ALLOC))
        {
            uint32_t val;
            int ntries;

            /* Remove all occurences of this sessions LSID from the job queues */
            disable_scheduling(regs);
            jobqueue_dequeue_lsid(regs, lsid);
            enable_scheduling(regs);

            mver_reg_write326(&regs->LSID[lsid].SCHED, 0);

            mver_reg_write326(&regs->LSID[lsid].TERMINATE, 1);
            ntries = WAIT_FOR_TERMINATE_NTRIES;
            do
            {
                val = mver_reg_read326(&regs->LSID[lsid].TERMINATE);
            }
            while (0 != val && --ntries);
            /* Print a warning if hardware was unresponsive */
            WARN_ON(0 != val);

            mver_reg_write326(&regs->LSID[lsid].ALLOC, 0);

            MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_DEBUG, "Session stopped. Switching out session. session=%p.", session->session_id);
        }

        session_by_lsid[lsid] = NULL;
        session->lsid = NO_LSID;

        mver_reg_put_coresched_bank6(&regs);

        /* This session is no longer running */
        ignore_me = down_interruptible(&session->scheduled_sem);

        /* Switch in pending sessions */
        switch_in_pending_sessions();
    }
}

bool mver_scheduler_register_session6(mver_session_id session_id,
                                     uint32_t mmu_ctrl,
                                     int ncores,
                                     bool secure,
                                     irq_callback_fptr irq_callback,
                                     session_has_work_fptr has_work_callback,
                                     session_switchout_fptr switchout_callback,
                                     session_switchin_fptr switchin_callback,
                                     session_switchout_completed_fptr switchout_complete_callback,
                                     session_get_restricting_buffer_count_fptr get_restricting_buffer_count_callback)
{
    struct mve_session_entry *entry;
    int sem_taken;

    entry = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_session_entry), GFP_KERNEL);
    if (NULL == entry)
    {
        return false;
    }

    entry->session_id         = session_id;
    entry->lsid               = NO_LSID;
    entry->mmu_ctrl           = mmu_ctrl;
    entry->ncores             = ncores;
    entry->secure             = secure;
    entry->enqueues           = 0;
    entry->irq_callback       = irq_callback;
    entry->has_work_callback  = has_work_callback;
    entry->switchout_callback = switchout_callback;
    entry->switchin_callback  = switchin_callback;
    entry->switchout_complete_callback = switchout_complete_callback;
    entry->get_restricting_buffer_count_callback = get_restricting_buffer_count_callback;

    INIT_LIST_HEAD(&entry->sessions_list);

    /* Initialize the semaphore to represent an unscheduled session */
    sema_init(&entry->scheduled_sem, 0);

    sem_taken = down_interruptible(&scheduler_sem);
    if (0 != sem_taken)
    {
        goto error;
    }

    mver_dvfs_register_session6(session_id);

    list_add(&entry->sessions_list, &sessions);
    up(&scheduler_sem);

    return true;

error:
    if (NULL != entry)
    {
        MVE_RSRC_MEM_FREE(entry);
    }

    return false;
}

void mver_scheduler_unregister_session6(mver_session_id session_id)
{
    struct mve_session_entry *entry;
    int sem_taken = 0;

//    sem_taken = down_interruptible(&scheduler_sem);
    down(&scheduler_sem);
    /* Continue event if the semaphore couldn't be acquired */

    mver_dvfs_unregister_session6(session_id);
    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        list_del(&entry->sessions_list);
        MVE_RSRC_MEM_FREE(entry);
    }

    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }
}

/**
 * Process IRQ for a session.
 * @param entry The session that has received an IRQ.
 */
static void process_session_irq(struct mve_session_entry *entry)
{
    enum SCHEDULE_STATE state = SCHEDULE_STATE_IDLE;

    /* Process IRQ */
    entry->irq_callback(entry->session_id);
    /* Does the session have any work to perform? */
    state = entry->has_work_callback(entry->session_id);

    if (SCHEDULE_STATE_IDLE == state)
    {
#if SCHEDULER_MODE_IDLE_SWITCHOUT == 1
        /* Switch out all sessions that report idleness. This is done to allow
         * the HW to power gate. */
        entry->switchout_callback(entry->session_id, true);
#else
        /* The session is idle and should be switched out if any other
         * session is enqueued */
        bool queue_empty = jobqueue_is_empty();
        if (!queue_empty)
        {
            /* There are items in the job queue. Switch out this session! */
            entry->switchout_callback(entry->session_id, true);
        }
#endif
    }
    else if (SCHEDULE_STATE_RESCHEDULE == state && 0 == entry->enqueues)
    {
        bool is_executing = jobqueue_is_enqueued(entry);
        if (false == is_executing)
        {
            /* Session has more work to do. Schedule it */
            mver_circular_buffer_add6(queued_sessions, entry);
            entry->enqueues++;
        }
    }
    else if (SCHEDULE_STATE_REQUEST_SWITCHOUT == state)
    {
        entry->switchout_callback(entry->session_id, true);
    }
}

bool mver_scheduler_execute6(mver_session_id session_id)
{
    struct mve_session_entry *entry;
    int sem_taken;
    bool ret = false;

    sem_taken = down_interruptible(&scheduler_sem);

    if (NULL == queued_sessions)
    {
        goto out;
    }

    if (false == scheduling_enabled)
    {
        /* Scheduling has been disabled. Just add the session to the queue
         * and it'll execute once scheduling is enabled. */
        entry = get_session_cache_entry(session_id);
        if (NULL != entry && 0 == entry->enqueues)
        {
            mver_circular_buffer_add6(queued_sessions, entry);
            entry->enqueues++;
        }

        if (0 == sem_taken)
        {
            up(&scheduler_sem);
        }

        return false;
    }

    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        /* Make sure all IRQs for the session have been processed */
        process_session_irq(entry);

        if (0 == entry->enqueues &&
            (NO_LSID == entry->lsid || false == session_is_executing(entry)))
        {
            mver_circular_buffer_add6(queued_sessions, entry);
            entry->enqueues++;
        }

        if (false != no_free_lsid)
        {
            /* No free LSID. Try to free a LSID that is not used */
            switch_out_pending_sessions();
        }
        else
        {
            /* Schedule the next session in line */
            switch_in_pending_sessions();
        }

        if (NO_LSID == entry->lsid)
        {
            /* This session wasn't switched in. Most likely due to
             * too many concurrent sessions. */
            int res;

            switch_out_pending_sessions();

            if (0 == sem_taken)
            {
                up(&scheduler_sem);
            }

            /* Don't return just yet. Wait on the scheduled semaphore
             * with a timeout. This will reduce the number of user<->kernel
             * space transitions. */
            res = down_timeout(&entry->scheduled_sem, msecs_to_jiffies(UNSCHEDULED_WAIT_MS));
            if (0 != res)
            {
                /* Failed to schedule the session within the time limit. */
                ret = false;
            }
            else
            {
                /* Release the semaphore immediately since we only
                 * use it to detect whether the session has been
                 * scheduled or not. */
                up(&entry->scheduled_sem);
                mver_irq_signal_mve6(session_id);
                ret = true;
            }

            return ret;
        }
        else
        {
            mver_irq_signal_mve6(session_id);
            ret = true;
        }
    }

out:
    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }

    return ret;
}

void mver_scheduler_stop6(mver_session_id session_id)
{
    struct mve_session_entry *entry;
    int sem_taken = 0;

//    sem_taken = down_interruptible(&scheduler_sem);
    down(&scheduler_sem);

    if (NULL == queued_sessions)
    {
        goto out;
    }

    /* Continue even in the case the semaphore wasn't acquired */
    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        mver_circular_buffer_remove_all_occurences6(queued_sessions, entry);
        stop_session(entry);
    }

out:
    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }
}

void mver_scheduler_handle_irq6(int lsid)
{
    struct mve_session_entry *entry;
    int sem_taken;

    if (lsid < 0 || lsid >= (int)NELEMS(session_by_lsid))
    {
        return;
    }

    sem_taken = down_interruptible(&scheduler_sem);

    if (NULL == queued_sessions)
    {
        goto out;
    }

    /* Continue even if the semaphore wasn't acquired */
    entry = session_by_lsid[lsid];

    /* entry may be NULL if the session is stopped before an interrupt
     * connected to the session is processed. This is not an error, just
     * skip processing the interrupt. */
    if (NULL != entry)
    {
        WARN_ON(lsid != entry->lsid);
        process_session_irq(entry);
    }

    if (false != no_free_lsid)
    {
        /* A session was previously prevented from scheduling because
         * of no free LSIDs. Unmap all sessions that no longer executes
         * on the HW. */
        switch_out_pending_sessions();
    }
    else
    {
        switch_in_pending_sessions();
    }

out:
    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }
}

void mver_scheduler_flush_tlb6(mver_session_id session_id)
{
    struct mve_session_entry *entry;

    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        enum LSID lsid = entry->lsid;

        if (NO_LSID != lsid)
        {
            tCS *regs = mver_reg_get_coresched_bank6();
            mver_reg_write326(&regs->LSID[lsid].FLUSH_ALL, 0);
            mver_reg_put_coresched_bank6(&regs);
        }
    }
}

enum LSID mver_scheduler_get_session_lsid6(mver_session_id session_id)
{
    struct mve_session_entry *entry;
    enum LSID lsid = NO_LSID;

    entry = get_session_cache_entry(session_id);
    if (NULL != entry)
    {
        lsid = entry->lsid;
    }

    return lsid;
}

void mver_scheduler_suspend6(void)
{
    int sem_taken;
    int i;
    bool done = false;

    sem_taken = down_interruptible(&scheduler_sem);
    scheduling_enabled = false;

    /* Instruct all running sessions to switch out */
    for (i = 0; i < MAX_LSID; ++i)
    {
        struct mve_session_entry *entry;

        entry = session_by_lsid[i];
        if (NULL != entry)
        {
            entry->switchout_callback(entry->session_id, false);
        }
    }

    if (0 == sem_taken)
    {
        up(&scheduler_sem);
    }

    /* Wait until all sessions have switched out */
    while (false == done)
    {
        done = true;

        sem_taken = down_interruptible(&scheduler_sem);
        /* Verify that no sessions are running */
        for (i = 0; i < mve_rsrc_data6.nlsid; ++i)
        {
            if (NULL != session_by_lsid[i])
            {
                /* This session has not yet switched out. When it no longer
                 * executes, force it to switch out */
                bool res = session_is_executing(session_by_lsid[i]);
                if (false == res)
                {
                    switch_out_session(session_by_lsid[i]);
                }

                done = false;
                break;
            }
        }
        if (0 == sem_taken)
        {
            up(&scheduler_sem);
        }

        if (false == done)
        {
            msleep(100);
        }
    }

    MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_INFO, "All sessions suspended.");
}

void mver_scheduler_resume6(void)
{
    /* No need to lock any mutexes now since userspace is frozen */
    scheduling_enabled = true;
    no_free_lsid = false;

    /* Resume sessions */
    switch_in_pending_sessions();

    MVE_LOG_PRINT(&mve_rsrc_log_scheduler6, MVE_LOG_INFO, "Sessions resumed.");
}

void mver_scheduler_lock6()
{
    down(&scheduler_sem);
}

void mver_scheduler_unlock6()
{
    up(&scheduler_sem);
}

int mver_scheduler_get_ncores6(void)
{
#ifdef EMULATOR
    return 1;
#else
    return mve_rsrc_data6.ncore;
#endif
}

EXPORT_SYMBOL(mver_scheduler_register_session6);
EXPORT_SYMBOL(mver_scheduler_unregister_session6);
EXPORT_SYMBOL(mver_scheduler_execute6);
EXPORT_SYMBOL(mver_scheduler_stop6);
EXPORT_SYMBOL(mver_scheduler_flush_tlb6);
EXPORT_SYMBOL(mver_scheduler_lock6);
EXPORT_SYMBOL(mver_scheduler_unlock6);
EXPORT_SYMBOL(mver_scheduler_get_ncores6);
