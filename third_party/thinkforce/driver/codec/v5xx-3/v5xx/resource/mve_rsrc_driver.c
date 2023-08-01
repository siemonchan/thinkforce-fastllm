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

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <asm/io.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_irq.h"
#include "mve_rsrc_log.h"
#include "mve_rsrc_register.h"
#include "mve_rsrc_scheduler.h"
#include "mve_rsrc_pm.h"

#include "machine/mve_port_attributes.h"

#define SET_BITFIELD(value, shift, sz) (((value) & ((1u << (sz)) - 1)) << (shift))

static bool probed = false;
static int mve_dev_major;
struct mve_rsrc_driver_data mve_rsrc_data;

extern struct mve_config_attribute *mve_device_get_config(void);

/* Mali-V500 can handle physical addresses 40 bits wide */
static uint64_t mv500_dma_mask = DMA_BIT_MASK(40);

static int mver_driver_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int mver_driver_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct of_device_id mve_rsrc_of_match[] =
{
    {
        .compatible = "arm,mali-v500",
    },
    {
        .compatible = "arm,mali-v550",
    },
    {
        .compatible = "arm,mali-mve",
    },
    {}
};
MODULE_DEVICE_TABLE(of, mve_rsrc_of_match);

static struct file_operations rsrc_fops =
{
    .owner = THIS_MODULE,
    .open = mver_driver_open,
    .release = mver_driver_release,
};

static void reset_hw(void)
{
    tCS *regs;
    uint32_t ncore;
    uint32_t corelsid_mask;

    regs = mver_reg_get_coresched_bank();
    ncore = mver_reg_read32(&regs->NCORES);
    corelsid_mask = 0;
    for (; ncore > 0; --ncore)
    {
        corelsid_mask = (corelsid_mask << 4) | 0xF;
    }
    mver_reg_write32(&regs->RESET, 1);
    while (corelsid_mask !=  mver_reg_read32(&regs->CORELSID))
        ;

    mver_reg_put_coresched_bank(&regs);
}

static void enableVideo(void)
{
    phys_addr_t start = 0xB1000000;
    size_t      size  = 0x00600000;
    volatile unsigned char * addr;

    void __iomem * regs;

    regs = ioremap(start, size);// __pgprot(PROT_DEVICE_nGnRE));

    addr = (volatile unsigned char *)regs;

    //reset vector
    *(volatile unsigned int *)(addr + 0x482000 + 0x12 * 4) = (0x18030000 >> 2);            // decoder 0
    *(volatile unsigned int *)(addr + 0x486000 + 0x12 * 4) = (0x18060000 >> 2);            // dccoder 1
    *(volatile unsigned int *)(addr + 0x0F2000 + 0x12 * 4) = (0x18090000 >> 2);            // decoder 2
    *(volatile unsigned int *)(addr + 0x0D0018) = (0x18000000 >> 20);                      // encoder

    // top left
    *(volatile unsigned int *)(addr + 0x400000 + 0x24 * 4) = 0;                            // unlock
    *(volatile unsigned int *)(addr + 0x400000 + 0x86 * 4) |= (7 << 9);                    // bit 9: decoder 0 bustop clk, bit 10: decoder 1 bustop clk, bit 11: topbus bustop clk;
    *(volatile unsigned int *)(addr + 0x400000 + 0x82 * 4) |= (7 << 5);                    // bit 5: decoder 0 reset, bit 6: decoder 1 reset, bit 7: tl_topbus reset;  
    *(volatile unsigned int *)(addr + 0x400000 + 0x24 * 4) = 1;                            // lock

    // top right
    *(volatile unsigned int *)(addr + 0x0A0000 + 0x24 * 4) = 0;                            // unlock
    *(volatile unsigned int *)(addr + 0x0A0000 + 0x86 * 4) |= ((unsigned int)0xF << 28);    // bit 28: decoder 2 bustop gclk, bit 29: decoder 2 bustop aclk, bit 30: encoder bustop gclk, bit 31: encoder bustop aclk
    *(volatile unsigned int *)(addr + 0x0A0000 + 0x82 * 4) |= (3 << 10);                    // bit 10: decoder 2 reset, bit 11: encoder reset;
    *(volatile unsigned int *)(addr + 0x0A0000 + 0x24 * 4) = 1;                            // lock


#if 0
    // Reset Vector
    *(volatile unsigned int *)(0xB1482000 + 0x12 * 4) = (0x18030000 >> 2);            // decoder 0
    *(volatile unsigned int *)(0xB1486000 + 0x12 * 4) = (0x18060000 >> 2);            // decoder 1
    *(volatile unsigned int *)(0xB10F2000 + 0x12 * 4) = (0x18090000 >> 2);            // decoder 2
    *(volatile unsigned int *)0xB10D0018 = (0x18000000 >> 20);                        // encoder

    // top left
    *(volatile unsigned int *)(0xB1400000 + 0x24 * 4) = 0;                            // unlock
    *(volatile unsigned int *)(0xB1400000 + 0x86 * 4) |= (7 << 9);                    // bit 9: decoder 0 bustop clk, bit 10: decoder 1 bustop clk, bit 11: topbus bustop clk;
    *(volatile unsigned int *)(0xB1400000 + 0x82 * 4) |= (7 << 5);                    // bit 5: decoder 0 reset, bit 6: decoder 1 reset, bit 7: tl_topbus reset;  
    *(volatile unsigned int *)(0xB1400000 + 0x24 * 4) = 1;                            // lock

    // top right
    *(volatile unsigned int *)(0xB10A0000 + 0x24 * 4) = 0;                            // unlock
    *(volatile unsigned int *)(0xB10A0000 + 0x86 * 4) |= ((unsigned int)0xF << 28);    // bit 28: decoder 2 bustop gclk, bit 29: decoder 2 bustop aclk, bit 30: encoder bustop gclk, bit 31: encoder bustop aclk
    *(volatile unsigned int *)(0xB10A0000 + 0x82 * 4) |= (3 << 10);                    // bit 10: decoder 2 reset, bit 11: encoder reset;
    *(volatile unsigned int *)(0xB10A0000 + 0x24 * 4) = 1;                            // lock
#endif

    msleep(100);
}

static int mver_driver_probe(struct platform_device *pdev)
{
    struct mve_rsrc_driver_data *private = &mve_rsrc_data;
    int ret = 0;
    struct resource *res;
    struct resource *irq_res;
    tCS *regs;
    mve_port_attributes_callback_fptr attr_fptr;
    const struct of_device_id *of_match;

    probed = true;

    of_match = of_match_device(mve_rsrc_of_match, &pdev->dev);
    if (NULL == of_match)
    {
        /* Driver doesn't support this device */
        printk(KERN_ERR "MVE: No matching device to Mali-MVE of_node: %p.\n", pdev->dev.of_node);
        return -EINVAL;
    }

    mve_dev_major = register_chrdev(0, MVE_RSRC_DRIVER_NAME, &rsrc_fops);
    if (0 > mve_dev_major)
    {
        printk(KERN_ERR "MVE: Failed to register the driver \'%s\'.\n", MVE_RSRC_DRIVER_NAME);
        ret = mve_dev_major;
        goto error;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (NULL == res)
    {
        printk(KERN_ERR "MVE: No Mali-MVE I/O registers defined.\n");
        ret = -ENXIO;
        goto error;
    }

    irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    if (NULL == irq_res)
    {
        printk(KERN_ERR "MVE: No IRQ defined for Mali-MVE.\n");
        ret = -ENODEV;
        goto error;
    }
    private->irq_nr = irq_res->start;
    private->irq_flags = irq_res->flags;

    private->mem_res = request_mem_region(res->start, resource_size(res), MVE_RSRC_DRIVER_NAME);
    if (!private->mem_res)
    {
        printk(KERN_ERR "MVE: Failed to request Mali-MVE memory region.\n");
        ret = -EBUSY;
        goto error;
    }

    //private->regs = ioremap_nocache(res->start, resource_size(res));
    //private->regs = __ioremap(res->start, resource_size(res), __pgprot(PROT_DEVICE_nGnRE));
    private->regs = ioremap(res->start, resource_size(res));
    if (NULL == private->regs)
    {
        printk(KERN_ERR "MVE: Failed to map Mali-MVE registers.\n");
        ret = -ENXIO;
        goto error;
    }

    private->pdev = pdev;
    private->irq_enable_count = 0;

    /* There is no way to set the dma_mask using device trees.  */
    pdev->dev.dma_mask = &mv500_dma_mask;
    pdev->dev.coherent_dma_mask = mv500_dma_mask;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    of_dma_configure(&pdev->dev, pdev->dev.of_node, true);
#endif

    private->config = mve_device_get_config();
    if (NULL == private->config)
    {
        printk(KERN_ERR "MVE: Failed to request Mali-MVE driver configuration.\n");
        ret = -ENXIO;
        goto error;
    }

    attr_fptr = (mve_port_attributes_callback_fptr)
                mve_config_get_value(private->config, MVE_CONFIG_DEVICE_ATTR_BUS_ATTRIBUTES);
    if (NULL == attr_fptr)
    {
        printk(KERN_ERR "MVE: Failed to request MVE_CONFIG_DEVICE_ATTR_BUS_ATTRIBUTES.\n");
        ret = -ENXIO;
        goto error;
    }

    /* Fetch the AXI memory access settings */
    private->port_attributes[0] = attr_fptr(0);
    private->port_attributes[1] = attr_fptr(1);
    private->port_attributes[2] = attr_fptr(2);
    private->port_attributes[3] = attr_fptr(3);

    private->hw_interaction = true;

    mver_reg_init();
    mve_rsrc_mem_init(&pdev->dev);
    mve_rsrc_log_init();
    mver_irq_handler_init(&pdev->dev);
    mver_pm_init(&pdev->dev);
    mver_scheduler_init(&pdev->dev);

    pm_runtime_set_suspended(&pdev->dev);
    pm_runtime_enable(&pdev->dev);

    pm_runtime_get_sync(&pdev->dev);

    reset_hw();
    regs = mver_reg_get_coresched_bank();
    private->nlsid = mver_reg_read32(&regs->NLSID);
    private->ncore = mver_reg_read32(&regs->NCORES);
    private->fuse = mver_reg_read32(&regs->FUSE);
    private->hw_version = mver_reg_read32(&regs->VERSION);

    if (private->hw_version >= MVE_V52_V76)
    {
        uint32_t sysbusctrl;
        uint32_t sysbusattr[2];
        sysbusattr[0] = 0; /* default value for CORESCHED_BUSCTRL_SPLIT is CORESCHED_BUSCTRL_SPLIT_128 */
        sysbusattr[1] = 0; /* default value for CORESCHED_BUSCTRL_REF64 is 0 */
        /* of_property_read_u32_array will not modify array until it is successful */
        of_property_read_u32_array(pdev->dev.of_node, "sys-busattr", sysbusattr, 2);
        sysbusctrl = SET_BITFIELD(sysbusattr[0], CORESCHED_BUSCTRL_SPLIT, CORESCHED_BUSCTRL_SPLIT_SZ) |
                     SET_BITFIELD(sysbusattr[1], CORESCHED_BUSCTRL_REF64, CORESCHED_BUSCTRL_REF64_SZ);
        mver_reg_write32(&regs->BUSCTRL, sysbusctrl);
    }

    mver_reg_put_coresched_bank(&regs);
    pm_runtime_put_sync(&pdev->dev);

    platform_set_drvdata(pdev, private);

    printk("MVE resource driver loaded successfully (nlsid=%u, cores=%u, version=0x%X fuse=0x%X).\n",
           private->nlsid, private->ncore, private->hw_version, private->fuse);

    return ret;

error:
    printk(KERN_ERR "Failed to load the driver \'%s\'.\n", MVE_RSRC_DRIVER_NAME);
    if (NULL != private->regs)
    {
        iounmap(private->regs);
    }

    if (0 < mve_dev_major)
    {
        unregister_chrdev(mve_dev_major, MVE_RSRC_DRIVER_NAME);
    }

    return ret;
}

static int mver_driver_remove(struct platform_device *pdev)
{
    mver_scheduler_deinit(&pdev->dev);
    mver_irq_handler_deinit(&pdev->dev);
    mve_rsrc_log_destroy();
    mve_rsrc_mem_deinit(&pdev->dev);
    mver_pm_deinit(&pdev->dev);
    pm_runtime_disable(&pdev->dev);

    iounmap(mve_rsrc_data.regs);
    release_mem_region(mve_rsrc_data.mem_res->start, resource_size(mve_rsrc_data.mem_res));
    unregister_chrdev(mve_dev_major, MVE_RSRC_DRIVER_NAME);

    printk("MVE resource driver unloaded successfully.\n");

    return 0;
}

#if 0
static const struct dev_pm_ops rsrc_pm =
{
    .suspend = mver_pm_suspend,
    .resume = mver_pm_resume,
    .runtime_suspend = mver_pm_runtime_suspend,
    .runtime_resume = mver_pm_runtime_resume,
    .runtime_idle = mver_pm_runtime_idle,
};
#endif

static struct platform_driver mv500_rsrc_driver =
{
    .probe = mver_driver_probe,
    .remove = mver_driver_remove,

    .driver = {
        .name = MVE_RSRC_DRIVER_NAME,
        .owner = THIS_MODULE,
//        .pm = &rsrc_pm,
        .pm = NULL,
        .of_match_table = mve_rsrc_of_match,
    },
};

static int __init mver_driver_init(void)
{
    enableVideo();
    platform_driver_register(&mv500_rsrc_driver);

    if (false != probed)
    {
        return 0;
    }
    else
    {
        printk("MVE Resource driver probe not called, check your device tree.\n");
        return -EINVAL;
    }
}

static void __exit mver_driver_exit(void)
{
    platform_driver_unregister(&mv500_rsrc_driver);
    MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_INFO, "MVE resource driver unregistered.");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mali-V500 resource driver");

module_init(mver_driver_init);
module_exit(mver_driver_exit);

EXPORT_SYMBOL(mve_rsrc_data);
