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

#include <linux/ioport.h>
#include <linux/platform_device.h>

#define VEXP6 0
#define VEXP7 1
#define JUNO 2

#if (VEXP6 == HW) || (VEXP7 == HW) || (JUNO == HW)
#include "../mve_rsrc_log.h"

#include "mve_power_management.h"
#include "mve_dvfs.h"
#include "mve_config.h"

#if defined(ENABLE_DVFS_FREQ_SIM)
#include "../mve_rsrc_register.h"

#if defined(EMULATOR)
#include "emulator_userspace.h"
#else
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#endif /* EMULATOR */

#endif /* ENABLE_DVFS_FREQ_SIM */

/* BEGIN: Move this to mach/arm/board-... */

/* Called by the driver when power to the MVE needs to be turned on */
static int power_on(void)
{
    MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Enable power.");
    /* No need to do anything on the FPGA */
    return 0;
}

/* Called by the driver when MVE no longer needs to be powered */
static void power_off(void)
{
    MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Disable power.");
    /* No need to do anything on the FPGA */
}

static struct mve_pm_callback_conf pm_callbacks =
{
    .power_off_callback = power_off,
    .power_on_callback  = power_on,
};

/* Callback function for retrieving AXI bus memory access settings */
static uint32_t bus_attributes_callback(int attribute)
{
    uint32_t ret = 0;

    switch (attribute)
    {
        case 0:
        /* Intentional fallthrough */
        case 1:
            ret = 0;
            break;
        case 2:
        /* Intentional fallthrough */
        case 3:
            ret = 0x33;
            break;
        default:
            WARN_ON(true);
    }

    return ret;
}

/**
 * Currently the only supported value for DVFS_FREQ_MAX is 100.
 * This is to simplify calculation of sleep duration in dvfs_thread().
 */
#define DVFS_FREQ_MAX (100)
static uint32_t dvfs_freq = DVFS_FREQ_MAX;

#if defined(ENABLE_DVFS_FREQ_SIM)
static bool dvfs_is_started = false;

static struct task_struct *dvfs_task = NULL;
static struct semaphore dvfs_sem;
static struct completion dvfs_ping;
static int dvfs_ncores;

/**
 * Pause all MVE cores.
 *
 * This function tries to pause all MVE cores. It will try to make sure
 * all cores are on pause before it returns, but it will not wait forever
 * and will return anyway after some time.
 */
static void dvfs_cores_pause(void)
{
    tCS *regs;
    uint32_t req;
    uint32_t val;
    int retries = 1000;

    req = (1 << dvfs_ncores) - 1;

    regs = mver_reg_get_coresched_bank();
    mver_reg_write32(&regs->CLKPAUSE, req);
    do
    {
        val = mver_reg_read32(&regs->CLKIDLE);
    }
    while (req != val && retries--);
    mver_reg_put_coresched_bank(&regs);

    if (req != val)
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_WARNING,
                      "Cannot pause all cores. dvfs_ncores=%d, req=%08x, val=%08x",
                      dvfs_ncores, req, val);
    }
}

/**
 * Resume MVE cores from pause.
 *
 * This function will resume MVE cores from pause and return immediately.
 */
static void dvfs_cores_resume(void)
{
    tCS *regs;

    regs = mver_reg_get_coresched_bank();
    mver_reg_write32(&regs->CLKPAUSE, 0);
    mver_reg_put_coresched_bank(&regs);
}

/**
 * DVFS clock simulation thread.
 *
 * This function is started in a separate kernel thread. It puts MVE cores on
 * pause for a fraction of a millisecond. Exact sleep time is determined from
 * requested clock frequency.
 */
static int dvfs_thread(void *v)
{
    int sem_failed;
    uint32_t freq;
    unsigned long timeout = msecs_to_jiffies(5000);

    while (!kthread_should_stop())
    {
        sem_failed = down_interruptible(&dvfs_sem);
        freq = dvfs_freq;
        if (!sem_failed)
        {
            up(&dvfs_sem);
        }

        if (DVFS_FREQ_MAX == freq)
        {
            wait_for_completion_interruptible_timeout(&dvfs_ping, timeout);
        }
        else
        {
            unsigned int pause_us = 1000 - 10 * freq;
            if (pause_us > 980)
            {
                pause_us = 980;
            }
            dvfs_cores_pause();
            usleep_range(pause_us, pause_us + 20);
            dvfs_cores_resume();
            usleep_range(1000 - pause_us, 1000 - pause_us + 20);
        }
    }
    return 0;
}

/**
 * Retrieve count of MVE cores.
 */
static int dvfs_get_ncores(void)
{
    tCS *regs;
    uint32_t ncores;

    regs = mver_reg_get_coresched_bank();
    ncores = mver_reg_read32(&regs->NCORES);
    mver_reg_put_coresched_bank(&regs);

    return ncores;
}

/**
 * Set requested clock frequency for clock simulation.
 */
static void dvfs_set_freq(const uint32_t freq)
{
    bool dvfs_thread_sleeps_now = false;
    int sem_failed = down_interruptible(&dvfs_sem);

    if (dvfs_freq >= DVFS_FREQ_MAX)
    {
        dvfs_thread_sleeps_now = true;
    }

    if (freq > DVFS_FREQ_MAX)
    {
        dvfs_freq = DVFS_FREQ_MAX;
    }
    else
    {
        dvfs_freq = freq;
    }

    if (dvfs_thread_sleeps_now && dvfs_freq < DVFS_FREQ_MAX)
    {
        complete(&dvfs_ping);
    }

    if (!sem_failed)
    {
        up(&dvfs_sem);
    }
}

/**
 * Retrieve simulated clock frequency value.
 */
static uint32_t dvfs_get_freq(void)
{
    uint32_t freq;
    int sem_failed = down_interruptible(&dvfs_sem);

    freq = dvfs_freq;

    if (!sem_failed)
    {
        up(&dvfs_sem);
    }

    return freq;
}

/**
 * Initialize clock frequency simulation.
 *
 * This function will simply exit if clock simulation was already initialized.
 */
void dvfs_sim_init(void)
{
    if (dvfs_is_started)
    {
        return;
    }

    sema_init(&dvfs_sem, 1);
    init_completion(&dvfs_ping);
    dvfs_ncores = dvfs_get_ncores();
    dvfs_task = kthread_run(dvfs_thread, NULL, "dvfs");

    dvfs_is_started = true;
}
#endif /* ENABLE_DVFS_FREQ_SIM */

static void enable_clock(void)
{
    MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Enable clock.");
}

static void disable_clock(void)
{
    MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Disable clock.");
}

static void set_clock_rate(uint32_t clk_rate)
{
    MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_DEBUG, "Setting clock rate. rate=%u (%u)", clk_rate, dvfs_freq);
#if defined(ENABLE_DVFS_FREQ_SIM)
    dvfs_sim_init();
    dvfs_set_freq(clk_rate);
#else
    dvfs_freq = clk_rate;
#endif
}

static uint32_t get_clock_rate(void)
{
#if defined(ENABLE_DVFS_FREQ_SIM)
    dvfs_sim_init();
    return dvfs_get_freq();
#else
    return dvfs_freq;
#endif
}

static uint32_t get_max_clock_rate(void)
{
    return DVFS_FREQ_MAX;
}

static void stop(void)
{
#if defined(ENABLE_DVFS_FREQ_SIM)
    if (dvfs_is_started)
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_DEBUG, "Stopping thread.");
        kthread_stop(dvfs_task);
        dvfs_is_started = false;
    }
#endif
}

static struct mve_dvfs_callback_conf dvfs_callbacks =
{
    .enable_clock  = enable_clock,
    .disable_clock = disable_clock,
    .set_rate      = set_clock_rate,
    .get_rate      = get_clock_rate,
    .get_max_rate  = get_max_clock_rate,
    .stop          = stop,
};

static struct mve_config_attribute attributes[] =
{
    {
        MVE_CONFIG_DEVICE_ATTR_POWER_CALLBACKS,
        (uint32_t *)&pm_callbacks
    },
    {
        MVE_CONFIG_DEVICE_ATTR_BUS_ATTRIBUTES,
        (uint32_t *)bus_attributes_callback
    },
    {
        MVE_CONFIG_DEVICE_ATTR_DVFS_CALLBACKS,
        (uint32_t *)&dvfs_callbacks
    },
    {
        MVE_CONFIG_DEVICE_ATTR_END,
        NULL
    }
};

struct mve_config_attribute *mve_device_get_config5(void)
{
    return attributes;
}
#endif /* (VEXP6 == HW) || (VEXP7 == HW) || (JUNO == HW) */
