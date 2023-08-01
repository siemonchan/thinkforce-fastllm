/*
 * (C) COPYRIGHT Think-Force Inc. All rights reserved.
 *
 *
 *
 *
 *
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>

#include "al_driver.h"

MODULE_IMPORT_NS(DMA_BUF);

#define AL_CMD_UNBLOCK_CHANNEL _IO('q', 1)

#define AL_CMD_IP_WRITE_REG         _IOWR('q', 10, struct al5_reg)
#define AL_CMD_IP_READ_REG          _IOWR('q', 11, struct al5_reg)
#define AL_CMD_IP_WAIT_IRQ          _IOWR('q', 12, __s32)
#define GET_DMA_FD                  _IOWR('q', 13, struct al5_dma_info)
#define GET_DMA_MMAP                _IOWR('q', 26, struct al5_dma_info)
#define GET_DMA_PHY                 _IOWR('q', 18, struct al5_dma_info)

#define MAX_TIMES   (10000)

struct al5_reg {
	__u32 id;
	__u32 value;
};

struct al5_dma_info {
	uint32_t      fd;
	uint32_t      size;
        phys_addr_t   phy_addr;
};

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

//    printk("%s : addr 0x%x value 0x%x\n", __func__, offset, value);
    writel(value, addr);
}

#if 0
static int alloc_dma_buffer(struct al_e200_data *dev, struct al5_dma_info *dma_info)
{
    uint32_t   size = dma_info->size;
    void      *cpu_addr;
    dma_addr_t dma_handle;

    //printk("%s: size 0x%x\n", __func__, size);
    cpu_addr = dma_alloc_coherent(&dev->pdev->dev, size, &dma_handle, GFP_KERNEL);
    if (NULL == cpu_addr)
    {
        printk("AL-ENCODER : Alloc dma buffer size 0x%x failed!\n", size);
        return -1;
    }

    dma_info->fd       = cpu_addr;
    dma_info->phy_addr = dma_handle;
    return 0;
}
#endif



static int al_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
    struct dma_info *info;
    uint32_t size;
    void * vaddr;
    int ret;

    //printk("\t%s vma start 0x%lx end 0x%lx flags 0x%x prot 0x%x\n", __func__, vma->vm_start, vma->vm_end, vma->vm_flags, vma->vm_page_prot);

    info = (struct dma_info*) buf->priv;
    size = info->size;
    vaddr = info->cpu_addr;
    

    ret = remap_pfn_range(vma, vma->vm_start, info->phy_addr >> PAGE_SHIFT, size, vma->vm_page_prot);
//    printk("\t%s vma start 0x%lx end 0x%lx pfn 0x%lx prot 0x%x finshed\n", __func__, vma->vm_start, vma->vm_end, info->phy_addr >> PAGE_SHIFT, vma->vm_page_prot);
    return ret;
}

static struct sg_table *al_map_dmabuf(struct dma_buf_attachment *at,
                                    enum dma_data_direction direction)
{
    printk("%s\n", __func__);
    return NULL;
}


static void al_unmap_dmabuf(struct dma_buf_attachment *at,
                          struct sg_table *sg,
                          enum dma_data_direction direction)
{
    printk("%s\n", __func__);
    return;
}

static void al_release_dmabuf(struct dma_buf *buf)
{
    struct dma_info *info;
    struct al_e200_data *dev;

    info = (struct dma_info*) buf->priv;
    dev  = info->dev;

    dma_free_coherent(&dev->pdev->dev, info->size, info->cpu_addr, info->phy_addr);

#if 0
    struct list_head *dma;
    bool   found = false;
    list_for_each(dma, &dev->dma_list)
    {
        struct dma_info *cur;

        cur = container_of(dma, struct dma_info, list);

        if (cur == info)
        {
            found = true;
            list_del(&info->list);
            break;
        }
    }

    if(!found)
    {
        printk("%s warning : dma_buf not found!\n", __func__);
    }
#endif

    kfree(info);

    return;
}

#if 0
static int al_begin_cpu_access(struct dma_buf *buf,
                             enum dma_data_direction direction)
{
    printk("%s\n", __func__);
    return 0;
}

static int al_end_cpu_access(struct dma_buf *buf,
                           enum dma_data_direction direction)
{
    printk("%s\n", __func__);
    return 0;
}
#endif

static const struct dma_buf_ops al_dmabuf_ops = {
        .cache_sgt_mapping = true,
        .map_dma_buf       = al_map_dmabuf,
        .unmap_dma_buf     = al_unmap_dmabuf,
        .release           = al_release_dmabuf,
        .mmap              = al_mmap,
//        .begin_cpu_access  = al_begin_cpu_access,
//        .end_cpu_access    = al_end_cpu_access,
};

static int get_dma_fd(struct al_e200_data *dev, struct al5_dma_info *dma_info)
{
    DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
    uint32_t   size;
    struct dma_buf *dmabuf;
    uint32_t   flags;
    int        ret;
    void      *cpu_addr;
    dma_addr_t dma_handle;
    struct dma_info *info;

    size = dma_info->size;
    cpu_addr = dma_alloc_coherent(&dev->pdev->dev, size, &dma_handle, GFP_KERNEL);
    if (NULL == cpu_addr)
    {
        printk("AL-ENCODER : Alloc dma buffer size 0x%x failed!\n", size);
        return -1;
    }

//    printk("%s : get cpu_addr 0x%lx phy_addr 0x%lx\n", __func__, cpu_addr, dma_handle);
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    info->cpu_addr = cpu_addr;
    info->phy_addr = dma_handle;
    info->size     = size;
    info->dev      = dev;
#if 0
    INIT_LIST_HEAD(&info->list);
    list_add(&info->list, &dev->dma_list);
#endif

    exp_info.ops = &al_dmabuf_ops;
    exp_info.size = size;
    exp_info.flags = O_RDWR;
    exp_info.priv = info;

    dmabuf = dma_buf_export(&exp_info);
        if (IS_ERR(dmabuf)) {
                printk("%s : dma_buf_export failed\n", __func__);
                ret = PTR_ERR(dmabuf);
                return -1;
        }

    flags = 0;
    dma_info->fd = dma_buf_fd(dmabuf, flags);
    dma_info->phy_addr = dma_handle;

//    printk("%s : get fd %d\n", __func__, dma_info->fd);
    return 0;
}


/* temp test func.*/
#if 0
static void dump(struct al_e200_data *dev, struct al5_reg *reg_info)
{
    struct list_head *dma;
    uint32_t val = reg_info->value;
    if (reg_info->id != 0x89e0)
        return;

    list_for_each(dma, &dev->dma_list)
    {
        struct dma_info *info;
        uint32_t phy;
        info = container_of(dma, struct dma_info, list);

        phy = (uint32_t)(info->phy_addr & 0xffffffff);

        if (phy == val)
        {
            uint32_t *pcmd = (uint32_t*)info->cpu_addr;
            int i;

//            printk("%s : found dma info match reg value 0x%x dma info cpu 0x%lx phy 0x%llx\n",
//                __func__, val, info->cpu_addr, info->phy_addr);

            
            for(i = 0; i < 16; i++)
            {
                printk("\t0x%lx : 0x%08x 0x%08x 0x%08x 0x%08x\n", 
                    pcmd, pcmd[0], pcmd[1], pcmd[2], pcmd[3]);

                pcmd += 4;
            }

            break;
        }
    }
}
#endif

long al_command_exec(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    struct al_e200_data *private;

    private = (struct al_e200_data*)filp->private_data;
#if 0
    switch(cmd)
    {
        case AL_CMD_IP_WRITE_REG:
            printk("%s : WRITE_REG\n", __func__);
            break;
        case AL_CMD_IP_READ_REG:
            printk("%s : READ_REG\n", __func__);
            break;
        case AL_CMD_UNBLOCK_CHANNEL:
            printk("%s : UNBLOCK_CHANNEL\n", __func__);
            break;
        case AL_CMD_IP_WAIT_IRQ:
            printk("%s : WAIT_IRQ\n", __func__);
            break;
        case GET_DMA_FD:
            printk("%s : GET_DMA_FD\n", __func__);
            break;
        case GET_DMA_MMAP:
            printk("%s : GET_DMA_MMAP\n", __func__);
            break;
        case GET_DMA_PHY:
            printk("%s : GET_DMA_PHY\n", __func__);
            break;
        default:
            printk("AL_ENCODER : iotcl unknown cmd %u\n", cmd);
            ret = -1;
            break;
    }
#endif

    switch (cmd)
    {
        case AL_CMD_IP_WRITE_REG:
        case AL_CMD_IP_READ_REG:
            {
                struct al5_reg reg_info;
                if (0 != copy_from_user(&reg_info, (void __user *)arg, sizeof(struct al5_reg)))
                {
                    printk("AL_ENCODER : iotcl command %u copy user data error\n", cmd);
                    ret = -1;
                    break;
                }

                if (cmd == AL_CMD_IP_READ_REG)
                {
                    uint32_t value;
#if 0
                    if (reg_info.id == 0x8004)
                    {
                        value = read_register(private, 0x8050);
                        printk("al_encoder info : 0x8050 0x%x\n", value);
                    }
#endif
                    value = read_register(private, reg_info.id);
                    reg_info.value = value;
                    if (copy_to_user((void __user *)arg, &reg_info, sizeof(struct al5_reg)))
                    {
                        printk("AL_ENCODER : copy_to_user failed.\n");
                    }
                }
                else
                {
#if 0
                    if (reg_info.id == 0x89e0)
                    {
                        dump(private, &reg_info);
                    }
#endif
                    write_register(private, reg_info.id, reg_info.value);
                }
            }
            break;
      
        case AL_CMD_UNBLOCK_CHANNEL:
            {
                printk("AL_ENCODER : iotcl command UNBLOCK_CHANNLE\n");
                down(&private->irq_sem);
                private->cancel_irq_waiting = true;
                up(&private->irq_sem); 
            }
            break;

        case AL_CMD_IP_WAIT_IRQ:
            {
                int32_t irq_num = 0;
                bool found = false;
                int i;
                int times = 0;

//                printk("WAIT_IQR enter: pending_irq 0x%x stand_irq 0x%x\n", private->pending_irq, private->stand_irq);
                while (!found)
                {
                    //down(&private->irq_sem);

                    //printk("WAIT_IQR :while found 0x%x\n", found);
                    /* handle orl irq first.*/
                    if (private->pending_irq)
                    {
//                        printk("WAIT_IQR : pending_irq 0x%x\n", private->pending_irq);
                        for(i = 0; i < 20; i++)
                        {
                            if ((1 << i) & private->pending_irq)
                            {
                                irq_num = i;
                                private->pending_irq &= ~(1 << i);
                                found = true;
//                                printk("AL_ENCODER : found 0x%x private->pending_irq\n", private->pending_irq);
                                break;
                            }
                        }
                    }
                    else if (private->stand_irq)
                    {
//                        printk("WAIT_IQR :stand_irq 0x%x\n", private->stand_irq);
                        down(&private->irq_sem);
                        private->pending_irq = private->stand_irq;
                        private->stand_irq = 0;
                        up(&private->irq_sem); 
                        //up(&private->irq_sem); 
                        continue;
                    }

                    //up(&private->irq_sem); 

                    if (found)
                        break;

                    usleep_range(100, 120);
                    times++;

                    down(&private->irq_sem);
                    if (private->cancel_irq_waiting)
                    {
                        printk("wait-irq : revcied cancel\n");
                        irq_num = -1;
                        found   = false;
                        private->cancel_irq_waiting = false;
                        up(&private->irq_sem); 
                        break;
                    }
                    up(&private->irq_sem); 

                    if (times >= MAX_TIMES)
                    {
                        irq_num = -1;
                        found   = true;
                        break;
                    }
                }

                if (!found)
                {
                    ret = -1;
                    break;
                }

//                printk("AL_ENCODER : Got irq_num %d\n", irq_num);
                if (copy_to_user((void __user *)arg, &irq_num, sizeof(int32_t)))
                {
                    printk("AL_ENCODER : copy_to_user failed.\n");
                }
            }
            break;

        case GET_DMA_FD:
        case GET_DMA_MMAP:
        case GET_DMA_PHY:
            {
                struct al5_dma_info dma_info;

                if (0 != copy_from_user(&dma_info, (void __user *)arg, sizeof(struct al5_dma_info)))
                {
                    printk("AL_ENCODER : iotcl command %u copy user data error\n", cmd);
                    ret = -1;
                    break;
                }

                if (GET_DMA_FD == cmd)
                {
                    ret = get_dma_fd(private, &dma_info);
                    if (0 != ret)
                    {
                        ret = -1;
                        break;
                    }
                }
                else if (GET_DMA_MMAP == cmd)
                {
                    printk("AL_ENCODER : ioctl command GET_DMA_MMAP isn't supported\n");
                }
                else if (GET_DMA_PHY)
                {
                    printk("AL_ENCODER : ioctl command GET_DMA_MMAP isn't supported\n");
                }

                if (copy_to_user((void __user *)arg, &dma_info, sizeof(struct al5_dma_info)))
                {
                    printk("AL_ENCODER : copy_to_user failed.\n");
                }
            }
            break;

        default:
            printk("AL_ENCODER : iotcl unknown cmd %u\n", cmd);
            ret = -1;
            break;
    }

//    printk("%s : finished ret %d\n", __func__, ret);
    return ret;
}
