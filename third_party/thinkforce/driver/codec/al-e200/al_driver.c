/*
 * (C) COPYRIGHT Think-Force Inc. All rights reserved.
 *
 *
 *
 *
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
#include <linux/workqueue.h>
#include <linux/of_reserved_mem.h>
#include <linux/acpi.h>

#ifdef INTERRUPT_POLLING
#include <linux/kthread.h>
#endif

//#ifdef CONFIG_ACPI
//#include <linux/dma-map-ops.h>
//#endif

#include "al_driver.h"


#define AL_E200_REG_INTERRUPT (0x8018)
#define AL_E200_REG_AXI_STATUS (0x8108)

static int al_dev_major;
static int al_dev_minor;

static struct class *al_class;

#ifdef CONFIG_ACPI
struct dma_coherent_mem {
    void        *virt_base;
    dma_addr_t    device_base;
    unsigned long    pfn_base;
    int        size;
    unsigned long    *bitmap;
    spinlock_t    spinlock;
    bool        use_dev_dma_pfn_offset;
};

static int dma_init_coherent_memory(phys_addr_t phys_addr,
        dma_addr_t device_addr, size_t size,
        struct dma_coherent_mem **mem)
{
    struct dma_coherent_mem *dma_mem = NULL;
    void *mem_base = NULL;
    int pages = size >> PAGE_SHIFT;
    int bitmap_size = BITS_TO_LONGS(pages) * sizeof(long);
    int ret;

    if (!size) {
        ret = -EINVAL;
        goto out;
    }

    mem_base = memremap(phys_addr, size, MEMREMAP_WC);
    if (!mem_base) {
        ret = -EINVAL;
        goto out;
    }
    dma_mem = kzalloc(sizeof(struct dma_coherent_mem), GFP_KERNEL);
    if (!dma_mem) {
        ret = -ENOMEM;
        goto out;
    }
    dma_mem->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
    if (!dma_mem->bitmap) {
        ret = -ENOMEM;
        goto out;
    }

    dma_mem->virt_base = mem_base;
    dma_mem->device_base = device_addr;
    dma_mem->pfn_base = PFN_DOWN(phys_addr);
    dma_mem->size = pages;
    spin_lock_init(&dma_mem->spinlock);

    *mem = dma_mem;
    return 0;

out:
    kfree(dma_mem);
    if (mem_base)
        memunmap(mem_base);
    return ret;
}

static void dma_release_coherent_memory(struct dma_coherent_mem *mem)
{
    if (!mem)
        return;

    memunmap(mem->virt_base);
    kfree(mem->bitmap);
    kfree(mem);
}

static int dma_assign_coherent_memory(struct device *dev,
                      struct dma_coherent_mem *mem)
{
    if (!dev)
        return -ENODEV;

    if (dev->dma_mem)
        return -EBUSY;

    dev->dma_mem = mem;
    return 0;
}


static int _dma_declare_coherent_memory(struct device *dev, phys_addr_t phys_addr,
                dma_addr_t device_addr, size_t size)
{
    struct dma_coherent_mem *mem;
    int ret;

    ret = dma_init_coherent_memory(phys_addr, device_addr, size, &mem);
    if (ret)
        return ret;

    ret = dma_assign_coherent_memory(dev, mem);
    if (ret)
        dma_release_coherent_memory(mem);
    return ret;
}
#endif


static void enableVideo(uint64_t hi_mask)
{
    phys_addr_t start = 0xB1000000 + hi_mask;
    size_t      size  = 0x00600000;
    volatile unsigned char * addr;
    uint32_t val;

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

    if (hi_mask != 0)
    {
        uint32_t mk;
        unsigned int isDualSocket, isDualDie;
        unsigned int *configBase = ioremap(0xFE170000, 0x100000);

        /* chip info. */
        isDualSocket = (*(volatile unsigned int *)(configBase + 0x30 / 4) & 0x40) >> 6;
        isDualDie    = *(volatile unsigned int *)(configBase + 0x3046C / 4) & 0x1;

        mk = (hi_mask >> 38) & 0x3;
#if 0
        if (mk == 0x2)
        {
            /* 2-cpu board. */
            if (!isDualSocket && isDualDie)
            {
                mk = 0x1;
            }
        }
#endif
        /* address */
        val = *(volatile unsigned int *)(addr + 0x0D00F0);                          // lock
        val = (val & ~0x3) | (mk & 0x3);
        printk("%s : will set 0x%x for enc high address\n", __func__, val);
        *(volatile unsigned int *)(addr + 0x0D00F0) = val;                            // lock
    }

    iounmap(regs);
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


static uint32_t read_register(struct al_e200_data *dev, uint32_t offset)
{
    volatile unsigned char * base = (unsigned char *)dev->regs;

    volatile uint32_t *addr = (uint32_t*)(base + offset);

    //printk("%s : base %lx offset 0x%x addr 0x%llx\n", __func__, base, offset, addr);
    return readl(addr);
}

static void write_register(struct al_e200_data *dev, uint32_t offset, uint32_t value)
{
    volatile unsigned char * base = (unsigned char *)dev->regs;
    volatile uint32_t *addr = (uint32_t*)(base + offset);

    writel(value, addr);
}


static void al_irq_handler_bottom(struct work_struct *work)
{
    struct al_irq_bottom_half *bottom;
    struct al_e200_data       *dev;
    uint32_t            irq_vector = 0;
    int                 nr;
    int                 nr_set;

    bottom     = (struct al_irq_bottom_half *)work;
    dev        = bottom->dev_data;;

    for (nr = 0; nr < 32; nr++)
    {
        nr_set = test_and_clear_bit(nr, &bottom->irq_vector);
        if (nr_set)
        {
            irq_vector |= 1 << nr;
        }
    }
   
    down(&dev->irq_sem);
    dev->stand_irq |= irq_vector;
    up(&dev->irq_sem);
}

irqreturn_t al_irq_handler_top(int irq, void *dev_id)
{
    uint32_t  irq_vector;
    int       nr;
    struct al_e200_data * dev_data;
    irqreturn_t ret = IRQ_NONE;

    dev_data = (struct al_e200_data *)dev_id;

    irq_vector = read_register(dev_data, AL_E200_REG_INTERRUPT);

    if (irq_vector)
    {
        write_register(dev_data, AL_E200_REG_INTERRUPT, irq_vector);
    }

    for (nr = 0; irq_vector != 0; nr++, irq_vector>>=1)
    {
        if (irq_vector & 0x1)
        {
            set_bit(nr, &dev_data->bottom_half.irq_vector);
        }
    }

    queue_work(dev_data->al_work_queue, &dev_data->bottom_half.work);

    ret = IRQ_HANDLED;
    return ret;
}

#ifdef INTERRUPT_POLLING
static int al_irq_thread_func(void *data)
{
    struct al_e200_data * dev_data;
    uint32_t  irq_vector;
    int       nr;
    unsigned long  count = 0;

    dev_data = (struct al_e200_data *)data;
    do
    {
        irq_vector = read_register(dev_data, AL_E200_REG_INTERRUPT);
        if (irq_vector)
        {
            printk("%s : get irq_vector 0x%x\n", __func__, irq_vector);
            write_register(dev_data, AL_E200_REG_INTERRUPT, irq_vector);
        }
        else
        {
            count++;
            if (0) //(count % 10000)
            {
                uint32_t axi_status;
                axi_status = read_register(dev_data, AL_E200_REG_AXI_STATUS);
            }
            usleep_range(100, 200);
            continue;
        }

        for (nr = 0; irq_vector != 0; nr++, irq_vector>>=1)
        {
            if (irq_vector & 0x1)
            {
                set_bit(nr, &dev_data->bottom_half.irq_vector);
            }
        }

        queue_work(dev_data->al_work_queue, &dev_data->bottom_half.work);
    }
    while(!kthread_should_stop());

    return 0;
}
#endif

static int al_driver_open(struct inode *inode, struct file *filp)
{
    struct al_e200_data *data;    
    int ret;

    //printk("%s:\n", __func__);
    data = container_of(inode->i_cdev, struct al_e200_data, al_cdev);
    data->cancel_irq_waiting = false;

    filp->private_data = data; 

    data->stand_irq = 0;
    ret = request_irq(data->irq, al_irq_handler_top,
                              (IRQF_SHARED | data->irq_flags) & IRQF_TRIGGER_MASK,
                              AL_DRIVER_NAME, data);

#ifdef INTERRUPT_POLLING
        {
            char name[] = "al-irq-thread";
            printk("%s : run kernel polling thread\n", __func__);
            data->irq_thread = kthread_run(al_irq_thread_func, data, name); 
        }
#endif

    return ret;
}

static int al_driver_release(struct inode *inode, struct file *filp)
{
    struct al_e200_data *data;    
//    struct list_head *dma;

    //printk("%s:\n", __func__);

    data = container_of(inode->i_cdev, struct al_e200_data, al_cdev);

#if 0
    list_for_each (dma, &data->dma_list)
    {
        struct dma_info *info;
        info = container_of(dma, struct dma_info, list);

        printk("%s warning : dma_info 0x%llx phy 0x%llx not release before close.\n",
            __func__, (uint64_t)info->cpu_addr, info->phy_addr);
    }
#endif

    down(&data->irq_sem);
    data->cancel_irq_waiting = true;
    up(&data->irq_sem); 

    free_irq(data->irq, data);
    data->stand_irq = 0;

    #ifdef INTERRUPT_POLLING
        kthread_stop(data->irq_thread);
    #endif

    return 0;
}

static long al_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return al_command_exec(file, cmd, arg);
}

static struct of_device_id al_e200_of_match[] =
{
    {
        .compatible = "tf,al-e200",
    },
    {}
};

MODULE_DEVICE_TABLE(of, al_e200_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id al_e200_apci_match[] = {
    { "TFV1000" },
    {  }
};
MODULE_DEVICE_TABLE(acpi, al_e200_apci_match);
#endif

static struct file_operations al_e200_fops =
{
    .owner = THIS_MODULE,
    .open  = al_driver_open,
    .release = al_driver_release,
    .unlocked_ioctl = al_driver_ioctl,
#ifdef CONFIG_64BIT
    .compat_ioctl = al_driver_ioctl,
#endif
};

static int al_driver_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct al_e200_data *private;
    struct device *dev = &pdev->dev;
    struct resource *res;
    struct resource *irq_res;
    dev_t dev_id;
#ifdef CONFIG_ACPI
    struct resource *reserved_dma;
#endif
    uint32_t addr_msb = 0;

    if (!has_acpi_companion(dev)) 
    {
        const struct of_device_id *of_match;
        of_match = of_match_device(al_e200_of_match, &pdev->dev);
        if (NULL == of_match)
        {
            printk("AL-E200: No matching device found.\n");
            return -EINVAL;
        }
    }

    /* Alloc device no. for the device. */
    if (al_dev_major < 0)
    {
        ret = alloc_chrdev_region(&dev_id, al_dev_minor, 1, AL_DRIVER_NAME);
        al_dev_major = MAJOR(dev_id);
        al_dev_minor = MINOR(dev_id);
    }
    else
    {
        al_dev_minor++; 
        dev_id = MKDEV(al_dev_major, al_dev_minor);
        ret = register_chrdev_region(dev_id, 1, AL_DRIVER_NAME);
    }

    printk("AL_ENCODER : register major %d minor %d\n", al_dev_major, al_dev_minor);

    private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
    private->pdev   = pdev;
    private->dev_id = dev_id;
    sema_init(&private->irq_sem, 1);

    /* Register the cdev of device into the kernel. */
    cdev_init(&private->al_cdev, &al_e200_fops);
    private->al_cdev.owner = THIS_MODULE;
    ret = cdev_add(&private->al_cdev, dev_id, 1);
    if (ret)
    {
        printk("AL-ENCODER : %s add cdev to system failed 0x%x\n", __func__, ret);
    }

    /* Create device node for the encoder. */
    if (al_dev_minor < 1)
    {
        printk("%s :create with minor %d\n", __func__, al_dev_minor);
        device_create(al_class, NULL, dev_id, NULL, "al_enc");
    }
    else
    {
        printk("%s :create with minor %d\n", __func__, al_dev_minor);
        device_create(al_class, NULL, dev_id, NULL, "al_enc%d", al_dev_minor+1);
    }
   
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (NULL == res)
    {
        printk("%s : get res is NULL\n", __func__);
    }

    irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    if (NULL == irq_res)
    {
        printk("%s : get irq is NULL\n", __func__);
    } 


    private->irq = irq_res->start;
    private->irq_flags = irq_res->flags;

    private->mem_res = request_mem_region(res->start, resource_size(res), AL_DRIVER_NAME);
    if (!private->mem_res)
    {
        printk("%s : request memory region failed\n", __func__);
    }


    private->regs = ioremap(res->start, resource_size(res));
    if (NULL == private->regs)
    {
        printk("%s : ioremap failed\n", __func__);
    }
    printk("%s : ioremaped start 0x%llx size 0x%llx\n", __func__, res->start, resource_size(res));

#ifdef CONFIG_ACPI
    reserved_dma = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (reserved_dma == NULL)
    {
        printk("%s : get reserved dma buffer is NULL\n", __func__);
        goto error;
    }

    printk("%s :reserved_dma start 0x%llx size 0x%llx\n", __func__, reserved_dma->start, resource_size(reserved_dma));
    ret = _dma_declare_coherent_memory(&pdev->dev,
                                      reserved_dma->start, reserved_dma->start, resource_size(reserved_dma));
    if (ret)
    {
        printk("%s : declare reserved dma buffer failed\n", __func__);
    }

    addr_msb = reserved_dma->start >> 32 & 0xF;
#else
{
    struct device_node *node;
    struct property *prop;
    int    size;
    uint32_t regs[4];

    node = of_find_node_by_path("/reserved-memory/encbuffer@0");
    if (node == NULL)
    {
        printk("%s : can't found encoder dma buffer node on the tree.\n", __func__);
        goto error;
    }

    prop = of_find_property(node, "reg", &size);
    if (prop == NULL)
    {
        printk("%s : can'g get reg prop of node.\n", __func__);
    }

    ret = of_property_read_u32_array(node, "reg", regs, 4);
    printk("%s : regs 0x%x 0x%x 0x%x 0x%x\n", __func__, regs[0], regs[1], regs[2], regs[3]);

    addr_msb = regs[0];

    printk("%s : addr_msb 0x%x\n", __func__, addr_msb);

    /* Init reserved memory if has. */
    of_reserved_mem_device_init(&pdev->dev);
}
#endif

    enableVideo(res->start & 0xFF00000000);

    spin_lock_init(&private->irq_lock);
    private->bottom_half.dev_data   = private;
    private->bottom_half.irq_vector = 0;
    INIT_WORK(&private->bottom_half.work, al_irq_handler_bottom);
    private->al_work_queue = (struct workqueue_struct *)create_workqueue("al_enc-wq");

    INIT_LIST_HEAD(&private->dma_list);
    platform_set_drvdata(pdev, private);

    printk("%s : probed memory start 0x%llx irq %d\n", __func__, private->mem_res->start, private->irq);
    
    {
        uint32_t version, uid, date;
        uint32_t msb_mask;
        uid     = read_register(private, 0x8000);
        version = read_register(private, 0x800c);
        date    = read_register(private, 0x8008);

        printk("%s : uid 0x%x version 0x%x date %x\n", __func__, uid, version, date);

        msb_mask = read_register(private, 0x8050);
        printk("%s : default msb_mask 0x%x\n", __func__, msb_mask);

        write_register(private, 0x8050, addr_msb);
        msb_mask = read_register(private, 0x8050);
        printk("%s : changed msb_mask 0x%x\n", __func__, msb_mask);
    }

    return 0;

error:
    printk(KERN_ERR "Failed to load the driver \'%s\'.\n", AL_DRIVER_NAME);
    return -1;
}

static int al_driver_remove(struct platform_device *pdev)
{
    struct al_e200_data *private;

    private = (struct al_e200_data*)platform_get_drvdata(pdev);

    flush_workqueue(private->al_work_queue);
    destroy_workqueue(private->al_work_queue);

    device_destroy(al_class, private->dev_id);
    cdev_del(&private->al_cdev);
    iounmap(private->regs);
    release_mem_region(private->mem_res->start, resource_size(private->mem_res));
    
    unregister_chrdev_region(private->dev_id, 1);
    devm_kfree(&pdev->dev, private);
    return 0;
}


static struct platform_driver al_e200_driver =
{
    .probe  = al_driver_probe,
    .remove = al_driver_remove,

    .driver = {
        .name = AL_DRIVER_NAME,
        .owner = THIS_MODULE,
        .pm = NULL,
        .of_match_table = al_e200_of_match,
        .acpi_match_table = ACPI_PTR(al_e200_apci_match),

    },
};

static int __init al_driver_init(void)
{
    printk("%s : al_driver module init\n", __func__);

    al_class = class_create(THIS_MODULE, AL_DRIVER_NAME);
    al_dev_major = -1;
    al_dev_minor = 0;
    platform_driver_register(&al_e200_driver);

    return 0;
}

static void __exit al_driver_exit(void)
{
    platform_driver_unregister(&al_e200_driver);
    class_destroy(al_class);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AL-E200 video encoder driver");

module_init(al_driver_init);
module_exit(al_driver_exit);
