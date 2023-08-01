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
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/stat.h>
#endif
#ifdef INTERRUPT_POLLING
#include <linux/kthread.h>
#endif

#include "mve_rsrc_irq.h"
#include "mve_rsrc_register.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_log.h"

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x) ((void)(x))
#endif

#if defined(_DEBUG)
static uint32_t irq_delay = 0;
#endif

/** @brief Information required for the bottom half of the interrupt handler */
struct mver_irq_bottom_half
{
    struct work_struct work;    /**< Work-structure. */
    unsigned long irq;          /**< IRQ vector. */
};

static struct mver_irq_bottom_half bottom_half;
static struct workqueue_struct *mver_work_queue;

/**
 * This function is executed in process context.
 */
void mver_irq_handler_bottom4(struct work_struct *work)
{
    struct mver_irq_bottom_half *bottom = (struct mver_irq_bottom_half *)work;
    uint32_t lsid;

    CSTD_UNUSED(work);

    for (lsid = 0; lsid < mve_rsrc_data4.nlsid; lsid++)
    {
        int irq_set = test_and_clear_bit(lsid, &bottom->irq);

        if (irq_set != 0)
        {
#if defined(_DEBUG) && !defined(EMULATOR)
            mdelay(irq_delay);
#endif
            mver_scheduler_handle_irq4(lsid);
        }
    }
}

/**
 * This function is executed in interrupt context.
 */
irqreturn_t mver_irq_handler_top4(int irq, void *dev_id)
{
    uint32_t irq_vector;
    tCS *regs;
    unsigned lsid;
    irqreturn_t ret = IRQ_NONE;

    CSTD_UNUSED(irq);
    CSTD_UNUSED(dev_id);

    /* Read IRQ register to detect which LSIDs that triggered the interrupt. */
    regs = mver_reg_get_coresched_bank_irq4();
    irq_vector = mver_reg_read324(&regs->IRQVE);
    printk("%s : irq_vector is 0x%x\n", __func__, irq_vector);

    for (lsid = 0; irq_vector != 0; lsid++, irq_vector >>= 1)
    {
        if (irq_vector & 0x1)
        {
            /* Set bit that this IRQ has been triggered and needs processing by bottom half. */
            set_bit(lsid, &bottom_half.irq);

            /* LSID generated interrupt. Clear it! */
            mver_reg_write324(&regs->LSID[lsid].IRQVE, 0);
            ret = IRQ_HANDLED;
        }
    }

    queue_work(mver_work_queue, &bottom_half.work);

#ifdef UNIT
    mve_rsrc_data4.interrupts++;
#endif

    return ret;
}

#if defined(CONFIG_SYSFS) && defined(_DEBUG)

static ssize_t sysfs_read_irq_delay(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
    uint32_t delay;

    delay = irq_delay;

    return snprintf(buf, PAGE_SIZE, "%u\n", delay);
}

static ssize_t sysfs_write_irq_delay(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf,
                                     size_t count)
{
    long delay;

    if (0 == kstrtol(buf, 0, &delay))
    {
        irq_delay = delay;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Unable to apply value to irq_delay.");
    }

    return count;
}

static struct device_attribute sysfs_files[] =
{
    __ATTR(irq_delay, S_IRUGO, sysfs_read_irq_delay, sysfs_write_irq_delay)
};

#endif /* defined(CONFIG_SYSFS) && defined(_DEBUG) */

void mver_irq_handler_init4(struct device *dev)
{
#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    int err;
#endif
    bottom_half.irq = 0;
    INIT_WORK(&bottom_half.work, mver_irq_handler_bottom4);

    mver_work_queue = (struct workqueue_struct *)create_workqueue("mv500-4-wq");

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    err = device_create_file(dev, &sysfs_files[0]);
    if (0 > err)
    {
        MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Unable to create irq_delay sysfs file.");
    }
#endif

#ifdef UNIT
    mve_rsrc_data4.interrupts = 0;
#endif
}

void mver_irq_handler_deinit4(struct device *dev)
{
    flush_workqueue(mver_work_queue);
    destroy_workqueue(mver_work_queue);

#if defined(CONFIG_SYSFS) && defined(_DEBUG)
    device_remove_file(dev, &sysfs_files[0]);
#endif
}

void mver_irq_signal_mve4(mver_session_id session)
{
    if (false != mve_rsrc_data4.hw_interaction)
    {
        tCS *regs = mver_reg_get_coresched_bank4();
        enum LSID lsid;

        lsid = mver_scheduler_get_session_lsid4(session);

        if (lsid != NO_LSID)
        {
            mver_reg_write324(&regs->LSID[lsid].IRQHOST, 1);
        }

        mver_reg_put_coresched_bank4(&regs);
    }
}

#ifdef INTERRUPT_POLLING
static int mver_irq_thread_func(void *data)
{
    uint32_t irq_vector;
    tCS *regs;
    unsigned lsid;

    do
    {
        regs = mver_reg_get_coresched_bank_irq4();
        irq_vector = mver_reg_read324(&regs->IRQVE);
        if (irq_vector == 0)
        {
            usleep_range(100, 200);
            continue;
        }

        printk("%s : get irq_vector 0x%x\n", __func__, irq_vector);
        for (lsid = 0; irq_vector != 0; lsid++, irq_vector >>= 1)
        {
            if (irq_vector & 0x1)
            {
                mver_reg_write324(&regs->LSID[lsid].IRQVE, 0);
                mver_scheduler_handle_irq4(lsid);
            }
        }
    }
    while(!kthread_should_stop());

    return 0;
}
#endif

int mver_irq_enable4(void)
{
    if (-1 != mve_rsrc_data4.irq_nr && 0 == mve_rsrc_data4.irq_enable_count++)
    {
        int irq_trigger = mve_rsrc_data4.irq_flags & IRQF_TRIGGER_MASK;
        int ret = request_irq(mve_rsrc_data4.irq_nr, mver_irq_handler_top4,
                              IRQF_SHARED | irq_trigger,
                              MVE_RSRC_DRIVER_NAME, &mve_rsrc_data4);
        if (0 != ret)
        {
            mve_rsrc_data4.irq_enable_count--;
            return -ENXIO;
        }

#ifdef INTERRUPT_POLLING
        {
            char name[] = "mve500-4-irq-thread";
            printk("%s : run kernel polling thread\n", __func__);
            mve_rsrc_data4.irq_thread = kthread_run(mver_irq_thread_func, NULL, name); 
        }
#endif
    }

    return 0;
}

void mver_irq_disable4(void)
{
    if (-1 != mve_rsrc_data4.irq_nr && 1 == mve_rsrc_data4.irq_enable_count--)
    {
        free_irq(mve_rsrc_data4.irq_nr, &mve_rsrc_data4);
        cancel_work_sync(&bottom_half.work);

#ifdef INTERRUPT_POLLING
        kthread_stop(mve_rsrc_data4.irq_thread);
#endif
    }
}

EXPORT_SYMBOL(mver_irq_signal_mve4);
EXPORT_SYMBOL(mver_irq_enable4);
EXPORT_SYMBOL(mver_irq_disable4);
