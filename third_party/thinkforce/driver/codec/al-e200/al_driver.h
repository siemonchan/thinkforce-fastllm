/*
 * (C) COPYRIGHT Think-Force Inc. All rights reserved.
 *
 *
 *
*/

#ifndef AL_DRIVER_HEADER_FILE
#define AL_DRIVER_HEADER_FILE

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>

#define AL_DRIVER_NAME "al-encoder"

struct al_e200_data;

struct al_irq_bottom_half
{
    struct work_struct work;
    struct al_e200_data *dev_data;
    unsigned long  irq_vector;

};

struct al_e200_data
{
    struct platform_device *pdev;
    dev_t  dev_id;

    int  irq;
    int  irq_flags;

    struct resource *mem_res;
    void __iomem *regs;

    struct cdev al_cdev;

    uint32_t stand_irq;
    uint32_t pending_irq;
    struct semaphore irq_sem;

    struct list_head dma_list;

    struct al_irq_bottom_half  bottom_half;
    struct workqueue_struct   *al_work_queue;
    bool cancel_irq_waiting;

    spinlock_t irq_lock;

    struct task_struct *irq_thread;
};

struct dma_info
{
    void       *cpu_addr;
    phys_addr_t phy_addr;
    uint32_t    size;
    struct al_e200_data * dev;

    struct list_head list;
};


long al_command_exec(struct file *filp, unsigned int cmd, unsigned long arg);
#endif /* AL_DRIVER_HEADER_FILE */
