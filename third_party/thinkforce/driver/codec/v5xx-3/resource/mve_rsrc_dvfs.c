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
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/export.h>
#endif

#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_dvfs.h"
#include "mve_rsrc_pm.h"

#if defined(DISABLE_DVFS)
uint32_t max_freq3 = 0;

void mver_dvfs_init3(struct device *dev,
                    const mver_dvfs_get_session_status_fptr get_session_status_callback)
{
    max_freq3 = mver_pm_read_max_frequency3();
}

void mver_dvfs_deinit3(struct device *dev)
{
    /* Nothing to do */
}

bool mver_dvfs_register_session3(const mver_session_id session_id)
{
    /* Request the maximum frequency. It's then up to the integration layer to
     * decide which frequency is actually used */
    mver_pm_request_frequency3(max_freq3);

    return true;
}

void mver_dvfs_unregister_session3(const mver_session_id session_id)
{
    /* Nothing to do */
}

void mver_dvfs_request_max_frequency3(void)
{
    /* Nothing to do */
}

#else /* DISABLE_DVFS */

#include "mve_rsrc_driver.h"
#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_log.h"

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
#define DVFS_DEBUG_MODE 1
#else
#define DVFS_DEBUG_MODE 0
#endif

/**
 * Default value for an interval between frequency updates in milliseconds.
 * It could be overwritten by user in debug build when sysfs is enabled.
 */
#define POLL_INTERVAL_MS (100)

/* Adjustment step in percents of maximum supported frequency */
#define UP_STEP_PERCENT (10)
#define DOWN_STEP_PERCENT (5)

/**
 * Structure used by DVFS module to keep track of session usage and to
 * take decisions about power management.
 *
 * Currently the only parameter taken into consideration is an amount of
 * output buffers enqueued in FW for each session. DVFS tries to keep this
 * parameter equal to 1 for all sessions. If some session has more than one
 * enqueued buffer, it means that a client is waiting for more than one
 * frame and the clock frequency should be increased. If some session has
 * no buffers enqueued, it means that the client is not waiting for
 * anything and the clock frequency could be decreased. Priority is given
 * to frequency increasing (when more than one session is registered).
 */
struct session
{
    mver_session_id session_id;
    bool to_remove;
#if defined(CONFIG_SYSFS)
    struct mver_dvfs_session_status status;
#endif
    struct list_head list;
};

/* A list containing all registered sessions */
static LIST_HEAD(sessions);

/* Semaphore used to prevent concurrent access to DVFS internal structures */
static struct semaphore dvfs_sem;

/* DVFS polling task */
static struct task_struct *dvfs_task = NULL;
static wait_queue_head_t dvfs_wq;

/**
 * A callback used to query session status. DVFS uses sessions status to
 * decide if clock frequency should be updated.
 */
static mver_dvfs_get_session_status_fptr get_session_status;

/* Frequency limits */
static uint32_t max_freq;
static uint32_t up_step_freq;
static uint32_t down_step_freq;

/* Flag used to prevent usage of DVFS module when it was not initialized */
static bool initialized = false;

/* Flag used to indicate that DVFS module is going to shut itself down */
static bool shutdown = false;

/**
 * DVFS polling interval - an interval between frequency updates in milliseconds.
 * It is a constant value for non-debug and non-sysfs builds.
 */
#if (1 == DVFS_DEBUG_MODE)
static int poll_interval_ms = POLL_INTERVAL_MS;
#else
static const int poll_interval_ms = POLL_INTERVAL_MS;
#endif

#if (1 == DVFS_DEBUG_MODE)
/* Flag used to enable/disable DVFS in debug builds */
static atomic_t control_enabled = ATOMIC_INIT(1);

/**
 * Counters used for debugging/verification purposes.
 */

/* Amount of times clock frequency was changed by DVFS */
static atomic_long_t changes_cnt = ATOMIC_LONG_INIT(0);

/* Amount of times burst mode was used by DVFS */
static atomic_long_t burst_cnt = ATOMIC_LONG_INIT(0);
#endif

static bool allocate_session(const mver_session_id session_id);
static void free_session(struct session *session);
static struct session *get_session(const mver_session_id session_id);

/**
 * Warm up VPU.
 *
 * This function increases VPU clock frequency for requested amount
 * of steps when possible.
 *
 * @param steps Requested amount of steps.
 */
static void warm_up(const int steps)
{
    uint32_t old = mver_pm_read_frequency3();
    uint32_t new;
#if (1 == DVFS_DEBUG_MODE)
    bool do_burst = false;
#endif

    /**
     * If 3 or more steps are requested, we are far behind required
     * performance level.
     */
    if (steps > 2)
    {
        new = max_freq;
#if (1 == DVFS_DEBUG_MODE)
        do_burst = true;
#endif
    }
    else
    {
        new = min(old + steps * up_step_freq, max_freq);
    }

    if (old != new)
    {
        mver_pm_request_frequency3(new);
#if (1 == DVFS_DEBUG_MODE)
        atomic_long_inc(&changes_cnt);
        if (do_burst)
        {
            atomic_long_inc(&burst_cnt);
        }
#endif
    }
}

/**
 * Cool down VPU.
 *
 * This function increases VPU clock frequency if possible.
 */
static void cool_down(void)
{
    int old_freq = (int)mver_pm_read_frequency3();
    int new_freq;

    new_freq = max(down_step_freq, old_freq - down_step_freq);

    if (old_freq != new_freq)
    {
        mver_pm_request_frequency3(new_freq);
#if (1 == DVFS_DEBUG_MODE)
        atomic_long_inc(&changes_cnt);
#endif
    }
}

/**
 * Update sessions list and VPU clock frequency.
 *
 * This function queries the state of all registered sessions and adjusts
 * VPU clock frequency to meet their needs when dvfs_control is enabled.
 * When SYSFS is enabled, the function also stores the status of all sessions
 * so it could be retrieved by the user.
 *
 * This function must be called when dvfs_sem semaphore IS NOT locked.
 */
static void update_sessions(void)
{
    struct list_head *entry;
    struct list_head *safe;
    struct session *session;
    struct mver_dvfs_session_status status;
    bool status_received;
    unsigned int buf_max = 0;
    unsigned int buf_min = UINT_MAX;
    int sem_failed;

    sem_failed = down_interruptible(&dvfs_sem);
    if (sem_failed)
    {
        return;
    }

    list_for_each_safe(entry, safe, &sessions)
    {
        session = list_entry(entry, struct session, list);

        /**
         * To avoid potential dead lock we release dvfs_sem before a call to
         * get_session_status() callback. After a return from the callback
         * we have to take dvfs_sem again and to verify that current session
         * was not unregistered by the scheduler while we were sleeping.
         */
        up(&dvfs_sem);
        status_received = get_session_status(session->session_id, &status);
        sem_failed = down_interruptible(&dvfs_sem);
        if (sem_failed)
        {
            return;
        }

        if (shutdown)
        {
            up(&dvfs_sem);
            return;
        }

        if (session->to_remove)
        {
            free_session(session);
            continue;
        }

        if (!status_received)
        {
#if 0
            MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_WARNING,
                          "DVFS failed to retrieve status for the session. Session was removed? session=%p",
                          session->session_id);
#endif
            continue;
        }

        if (status.restricting_buffer_count > buf_max)
        {
            buf_max = status.restricting_buffer_count;
        }
        if (status.restricting_buffer_count < buf_min)
        {
            buf_min = status.restricting_buffer_count;
        }

#if defined(CONFIG_SYSFS)
        session->status = status;
#endif
    }

    up(&dvfs_sem);

#if (1 == DVFS_DEBUG_MODE)
    if (0 == atomic_read(&control_enabled))
    {
        return;
    }
#endif

    if (buf_max > 1)
    {
        warm_up(buf_max);
    }
    else if (buf_min < 1)
    {
        cool_down();
    }
}

/**
 * Allocate and register a session in DVFS module.
 *
 * This function allocates needed resources for the session and registers
 * it in the module.
 *
 * This function must be called when dvfs_sem semaphore IS locked.
 *
 * @param session_is Session id
 * @return True when registration was successful,
 *         False otherwise.
 */
static bool allocate_session(const mver_session_id session_id)
{
    struct session *session;

    session = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(*session), GFP_KERNEL);
    if (NULL == session)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_WARNING,
                      "DVFS is unable to allocate memory for a new session. session=%p",
                      session_id);
        return false;
    }

    session->session_id = session_id;

    INIT_LIST_HEAD(&session->list);
    list_add(&session->list, &sessions);

    return true;
}

/**
 * Unregister a session from DVFS module.
 *
 * When session is not NULL, the function releases all previously allocated
 * resources for the session and unregisters it from DVFS.
 *
 * This function must be called when dvfs_sem semaphore IS locked.
 *
 * @param session Session or NULL
 */
static void free_session(struct session *session)
{
    if (NULL == session)
    {
        return;
    }
    list_del(&session->list);
    MVE_RSRC_MEM_CACHE_FREE(session, sizeof(*session));
}

/**
 * Find a session with provided session_id.
 *
 * This function tries to find previously registered session with provided
 * session_id.
 *
 * This function must be called when dvfs_sem semaphore IS locked.
 *
 * @param session_id Session id
 * @return pointer to session structure when a session was found,
 *         NULL when a session was not found.
 */
static struct session *get_session(const mver_session_id session_id)
{
    struct list_head *entry;
    struct session *session;
    list_for_each(entry, &sessions)
    {
        session = list_entry(entry, struct session, list);
        if (session->session_id == session_id)
        {
            return session;
        }
    }
    return NULL;
}

/**
 * DVFS polling thread.
 *
 * This function is executed in a separate kernel thread. It updates clock
 * frequency every poll_interval_ms milliseconds.
 */
static int dvfs_thread(void *v)
{
    MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_INFO, "DVFS polling thread started");
    while (!kthread_should_stop())
    {
        wait_event_interruptible(dvfs_wq, list_empty(&sessions) == 0 || shutdown);
        update_sessions();
        msleep_interruptible(poll_interval_ms);
    }

    MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_INFO, "DVFS polling thread finished");
    return 0;
}

/**
 *  Return percent percents from a value val.
 */
static uint32_t ratio(const uint32_t val, const uint32_t percent)
{
    return (val * percent) / 100;
}

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
/**
 * Print DVFS statistics to sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
static ssize_t sysfs_print_stats(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    ssize_t num = 0;
    struct list_head *entry;
    struct session *session;
    uint32_t freq = mver_pm_read_frequency3();

    num += scnprintf(buf + num, PAGE_SIZE - num,
                     "freq: %4u, max_freq: %4u, up_step_freq: %3u, down_step_freq: %3u",
                     freq, max_freq, up_step_freq, down_step_freq);
#if (1 == DVFS_DEBUG_MODE)
    num += scnprintf(buf + num, PAGE_SIZE - num,
                     ", enabled: %1u, poll_interval_ms: %3u, changes_cnt: %10lu, burst_cnt: %10lu",
                     atomic_read(&control_enabled), poll_interval_ms,
                     atomic_long_read(&changes_cnt), atomic_long_read(&burst_cnt));
#endif
    num += scnprintf(buf + num, PAGE_SIZE - num, "\n");
    list_for_each(entry, &sessions)
    {
        session = list_entry(entry, struct session, list);
        num += scnprintf(buf + num, PAGE_SIZE - num,
                         "%p: out_buf: %02u\n",
                         session->session_id, session->status.restricting_buffer_count);
    }

    return num;
}

#if (1 == DVFS_DEBUG_MODE)
/**
 * Print DVFS enabling status to sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
static ssize_t sysfs_print_enabled(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
    ssize_t num = 0;
    num += scnprintf(buf, PAGE_SIZE, "%u", atomic_read(&control_enabled) ? 1 : 0);
    return num;
}

/**
 * Set DVFS enabling status from sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
ssize_t sysfs_set_enabled3(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    int failed;
    int enabled;
    failed = kstrtouint(buf, 10, &enabled);
    if (!failed)
    {
        atomic_set(&control_enabled, enabled);
    }
    return (failed) ? failed : count;
}

/**
 * Print current clock frequency to sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
static ssize_t sysfs_print_freq(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    ssize_t num = 0;
    uint32_t freq = mver_pm_read_frequency3();
    num += scnprintf(buf, PAGE_SIZE, "%u", freq);
    return num;
}

/**
 * Set current clock frequency from sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
ssize_t sysfs_set_freq3(struct device *dev, struct device_attribute *attr,
                       const char *buf, size_t count)
{
    int failed;
    unsigned int freq;
    failed = kstrtouint(buf, 10, &freq);
    if (!failed)
    {
        mver_pm_request_frequency3((uint32_t)freq);
    }
    return (failed) ? failed : count;
}

/**
 * Set polling interval from sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
ssize_t sysfs_set_poll_interval_ms3(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
    int failed;
    failed = kstrtouint(buf, 10, &poll_interval_ms);
    return (failed) ? failed : count;
}

/**
 * Set up_step value from sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
ssize_t sysfs_set_up_step_percent3(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    int failed;
    unsigned int up_step_percent;
    failed = kstrtouint(buf, 10, &up_step_percent);
    if (!failed)
    {
        up_step_freq = ratio(max_freq, up_step_percent);
    }
    return (failed) ? failed : count;
}

/**
 * Set down_step value from sysfs attribute.
 *
 * Used for debugging/verification purposes.
 */
ssize_t sysfs_set_down_step_percent3(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    int failed;
    unsigned int down_step_percent;
    failed = kstrtouint(buf, 10, &down_step_percent);
    if (!failed)
    {
        down_step_freq = ratio(max_freq, down_step_percent);
    }
    return (failed) ? failed : count;
}
#endif /* DVFS_DEBUG_MODE */

/* Sysfs attributes used to debug/verify DVFS module */
static struct device_attribute sysfs_files[] =
{
    __ATTR(dvfs_stats, S_IRUGO, sysfs_print_stats, NULL),
#if (1 == DVFS_DEBUG_MODE)
    __ATTR(dvfs_enabled, (S_IRUGO | S_IWUSR), sysfs_print_enabled, sysfs_set_enabled),
    __ATTR(dvfs_freq,    (S_IRUGO | S_IWUSR), sysfs_print_freq, sysfs_set_freq),
    __ATTR(dvfs_poll_interval_ms,   S_IWUSR, NULL, sysfs_set_poll_interval_ms),
    __ATTR(dvfs_up_step_percent,    S_IWUSR, NULL, sysfs_set_up_step_percent),
    __ATTR(dvfs_down_step_percent,  S_IWUSR, NULL, sysfs_set_down_step_percent),
#endif
};

/**
 * Register all DVFS attributes in sysfs subsystem
 */
static void sysfs_register_devices(struct device *dev)
{
    int err;
    int i = NELEMS(sysfs_files);

    while (i--)
    {
        err = device_create_file(dev, &sysfs_files[i]);
        if (err < 0)
        {
            MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                          "DVFS is unable to create sysfs file. name=%s",
                          sysfs_files[i].attr.name);
        }
    }
}

/**
 * Remove DVFS attributes from sysfs subsystem
 */
static void sysfs_unregister_devices(struct device *dev)
{
    int i = NELEMS(sysfs_files);

    while (i--)
    {
        device_remove_file(dev, &sysfs_files[i]);
    }
}
#endif /* CONFIG_SYSFS && _DEBUG */

/**
 * Initialize the DVFS module.
 *
 * Must be called before any other function in this module.
 *
 * @param dev Device
 * @param get_session_status_callback Callback to query session status
 */
void mver_dvfs_init3(struct device *dev,
                    const mver_dvfs_get_session_status_fptr get_session_status_callback)
{
    if (!initialized)
    {
        sema_init(&dvfs_sem, 1);

        max_freq = mver_pm_read_max_frequency3();
        up_step_freq = ratio(max_freq, UP_STEP_PERCENT);
        down_step_freq = ratio(max_freq, DOWN_STEP_PERCENT);

        init_waitqueue_head(&dvfs_wq);

        dvfs_task = kthread_run(dvfs_thread, NULL, "dvfs");
#if defined(CONFIG_SYSFS) && defined(_DEBUG)
        if (NULL != dev)
        {
            sysfs_register_devices(dev);
        }
#endif
        get_session_status = get_session_status_callback;
        initialized = true;
        shutdown = false;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "Attempt to initialize DVFS twice");
    }
}

/**
 * Deinitialize the DVFS module.
 *
 * All remaining sessions will be unregistered.
 *
 * @param dev Device
 */
void mver_dvfs_deinit3(struct device *dev)
{
    int sem_failed;
    struct list_head *entry;
    struct list_head *safe;
    struct session *session;

    if (!initialized)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "Attempt to deinitialize DVFS when it was not initialized");
        return;
    }

    sem_failed = down_interruptible(&dvfs_sem);
    shutdown = true;
    if (!sem_failed)
    {
        up(&dvfs_sem);
    }

    wake_up_interruptible(&dvfs_wq);
    kthread_stop(dvfs_task);

    sem_failed = down_interruptible(&dvfs_sem);
    list_for_each_safe(entry, safe, &sessions)
    {
        session = list_entry(entry, struct session, list);
        free_session(session);
    }

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    if (NULL != dev)
    {
        sysfs_unregister_devices(dev);
    }
#endif

    initialized = false;
    if (!sem_failed)
    {
        up(&dvfs_sem);
    }
}

/**
 * Register session in the DFVS module.
 *
 * @param session_id Session id
 * @return True when registration was successful,
 *         False, otherwise
 */
bool mver_dvfs_register_session3(const mver_session_id session_id)
{
    bool success = false;
    int sem_failed;

    if (!initialized)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "DVFS module was not initialized");
        return false;
    }

    sem_failed = down_interruptible(&dvfs_sem);
    if (sem_failed)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "DVFS semaphore was not obtained, sem_failed=%d",
                      sem_failed);
        return false;
    }

    if (shutdown)
    {
        up(&dvfs_sem);
        return false;
    }

    success = allocate_session(session_id);
    up(&dvfs_sem);

    if (success)
    {
        mver_dvfs_request_max_frequency3();
    }

    wake_up_interruptible(&dvfs_wq);

    return success;
}

/**
 * Unregister session from the DFVS module.
 *
 * Usage of corresponding session is not permitted after this call.
 * @param session_id Session id
 */
void mver_dvfs_unregister_session3(const mver_session_id session_id)
{
    struct session *session;
    int sem_failed;

    if (!initialized)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "DVFS module was not initialized");
        return;
    }

    sem_failed = down_interruptible(&dvfs_sem);
    if (sem_failed)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_scheduler3, MVE_LOG_ERROR,
                      "DVFS semaphore was not obtained, %d",
                      sem_failed);
        return;
    }

    session = get_session(session_id);
    if (NULL != session)
    {
        session->to_remove = true;
    }

    up(&dvfs_sem);
}

/**
 * Request maximum clock frequency. This function is usually called in situations
 * when the client requests to increase the operating rate.
 */
void mver_dvfs_request_max_frequency3(void)
{
    bool adjust = true;

#if (1 == DVFS_DEBUG_MODE)
    /* Has DVFS been disabled through the sysfs interface? */
    adjust = atomic_read(&control_enabled);
#endif

    if (false != adjust)
    {
        mver_pm_request_frequency3(max_freq);
    }
}

#endif

EXPORT_SYMBOL(mver_dvfs_request_max_frequency3);
