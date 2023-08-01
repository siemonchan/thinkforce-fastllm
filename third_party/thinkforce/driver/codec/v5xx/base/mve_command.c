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
#endif

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include "mve_driver.h"
#include "mve_command.h"
#include "mve_session.h"

#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_register.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_pm.h"
#include "mve_rsrc_log.h"
#include "mve_rsrc_dvfs.h"

#ifdef UNIT
#include "mve_rsrc_register.h"
#endif

#define CORESCHED_FUSE_DISABLE_AFBC (0)
#define CORESCHED_FUSE_DISABLE_REAL (1)
#define CORESCHED_FUSE_DISABLE_VP8  (2)

#define MVE_ROLE_VIDEO_DECODER_RV   "video_decoder.rv"
#define MVE_ROLE_VIDEO_DECODER_VP8  "video_decoder.vp8"

#define KDRIVER_MAJOR   1
#define KDRIVER_MINOR   0
#define KDRIVER_BUILD   20220414

struct mve_buffer_client;

/************************************************************************************/
struct mve_dma_alloc
{
    uint32_t   size;
};

struct mve_dma_info
{
    uint32_t     fd;
    uint32_t     size;
    uint32_t     num_pages;

    struct file *filep;
    struct page **pages;
};

static struct sg_table *mve_map_dmabuf(struct dma_buf_attachment *at,
                                    enum dma_data_direction direction)
{
    struct mve_dma_info *info = at->dmabuf->priv;
    struct sg_table *sg;
    int ret;

//    printk("%s\n", __func__);
    sg = kzalloc(sizeof(*sg), GFP_KERNEL);
    if (!sg)
        return NULL;

    ret = sg_alloc_table_from_pages(sg, info->pages, info->num_pages,
                                    0, info->num_pages << PAGE_SHIFT, GFP_KERNEL);
    if (ret < 0)
    {
        printk("%s : %d error\n", __func__, __LINE__);
        goto err;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    ret = dma_map_sgtable(at->dev, sg, direction, 0);
    if (ret < 0)
    {
        printk("%s : %d error\n", __func__, __LINE__);
        goto err;
    }
#endif
    return sg;
err:
    printk("%s : error \n", __func__);
    sg = kzalloc(sizeof(*sg), GFP_KERNEL);
    sg_free_table(sg);
    kfree(sg);
    return NULL;
}

static void mve_unmap_dmabuf(struct dma_buf_attachment *at,
                          struct sg_table *sg,
                          enum dma_data_direction direction)
{
    //printk("%s\n", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    dma_unmap_sgtable(at->dev, sg, direction, 0);
#endif
    sg_free_table(sg);
    kfree(sg);
}


static int mve_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
    struct        mve_dma_info *dma_info = buf->priv;
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long page;

    uint32_t i = 0;
    printk("%s : enter fd %d start 0x%lx size 0x%lx\n", __func__, dma_info->fd, start, size);

    while (size > 0)
    {
       // printk("%s : i %d start 0x%lx\n", __func__, i, start);
        page = page_to_pfn((dma_info->pages[i]));
	if (remap_pfn_range(vma, start, page, PAGE_SIZE, vma->vm_page_prot))
        {
            printk("%s : map start 0x%lx failed\n", __func__, start);
            return -EAGAIN;
        }

        start += PAGE_SIZE;
        i++;
        if (size > PAGE_SIZE)
            size -= PAGE_SIZE;
        else
            size = 0;
    }

    return 0;
}


static void mve_release_dmabuf(struct dma_buf *buf)
{
    struct mve_dma_info *info = buf->priv;
    uint32_t num_pages = info->num_pages;
    uint32_t i;

    for (i = 0; i < num_pages; i++)
    {
        __free_page(info->pages[i]);
    }

    kfree(info->pages);
    kfree(info);
}

static const struct dma_buf_ops mve_dmabuf_ops = {
        .cache_sgt_mapping = true,
        .map_dma_buf       = mve_map_dmabuf,
        .unmap_dma_buf     = mve_unmap_dmabuf,
        .release           = mve_release_dmabuf,
        .mmap              = mve_mmap,
//        .begin_cpu_access  = al_begin_cpu_access,
//        .end_cpu_access    = al_end_cpu_access,
};

static int mve_command_alloc_dma(struct file *filep, struct mve_dma_alloc * dma_alloc)
{
    DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
    uint32_t size;
    uint32_t num_pages;
    uint32_t i;

    struct dma_buf *dmabuf;
    uint32_t   flags;
    struct mve_dma_info *info;
    struct page **pages;

    int ret = 0;
    int fd = 0;

    size = dma_alloc->size;
    num_pages = (size + PAGE_SIZE - 1)/ PAGE_SIZE;
    printk("%s : size %d pages %d\n", __func__, size, num_pages);

    pages = kmalloc_array(num_pages, sizeof(*pages), GFP_KERNEL);
    if (!pages)
    {
        kfree(info);
        return 0;
    }

    printk("%s : got pages %p\n", __func__,  pages);
    ret = 0;
    for (i = 0; i < num_pages; i++)
    {
        pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO | __GFP_NORETRY);

        if (!pages[i])
        {
            ret = -1;
            break;
        }
    }

    if (ret != 0)
    {
        printk("alloc page failed i %d\n", i);
        for (; i > 0; i--)
        {
            __free_page(pages[i - 1]);
        }

        return 0;
    }

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info)
    {
        printk("MVE : alloc dma error!\n");
        return 0;
    }
    printk("%s : info %p\n", __func__, info);

    info->size      = size;
    info->num_pages = num_pages;
    info->pages     = pages;
    info->filep     = filep;

    exp_info.ops = &mve_dmabuf_ops;
    exp_info.size = size;
    exp_info.flags = O_RDWR;
    exp_info.priv = info;

    dmabuf = dma_buf_export(&exp_info);
        if (IS_ERR(dmabuf)) {
                printk("%s : dma_buf_export failed\n", __func__);
                ret = PTR_ERR(dmabuf);
                return 0;
        }

    flags = 0;
    fd = dma_buf_fd(dmabuf, flags);
    info->fd = fd;

    return fd;
}

/************************************************************************************/

/**
 * Check the given role against the fuse state.
 * @param role A role string.
 * @return true if the role is enabled, false if it is fused.
 */
static bool is_role_enabled(char *role)
{
    bool ret = true;
    uint32_t fuse = mver_reg_get_fuse();

    if (NULL == role)
    {
        return false;
    }

    if (0 == strcmp(role, MVE_ROLE_VIDEO_DECODER_RV))
    {
        if (fuse & (1 << CORESCHED_FUSE_DISABLE_REAL))
        {
            ret = false;
        }
    }
    else if (0 == strcmp(role, MVE_ROLE_VIDEO_DECODER_VP8))
    {
        if (fuse & (1 << CORESCHED_FUSE_DISABLE_VP8))
        {
            ret = false;
        }
    }

    return ret;
}

/**
 * Check the given role against the fuse state before creating a new session.
 * @param data  Pointer to the data associated
 * @param filep File descriptor used to identify which client this session belongs to
 * @return MVE error code
 */
static mve_base_error mve_command_create_session_helper(void *data,
                                                        struct file *filep)
{
    struct mve_session *session = NULL;
    char *role = data;
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    if (false == is_role_enabled(role))
    {
        ret = MVE_BASE_ERROR_NOT_IMPLEMENTED;
        MVE_LOG_PRINT(&mve_rsrc_log_session,
                      MVE_LOG_ERROR,
                      "Role is not implemented/enabled. role=%s.",
                      role);
    }
    else
    {
        session = mve_session_create(filep);
        if (NULL == session)
        {
            ret = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
        }
        else
        {
            mve_session_set_role(session, role);
        }
    }

    return ret;
}

static const char *mve_command_to_string(uint32_t cmd)
{
    static const char *name[] = {
        "MVE_BASE_CREATE_SESSION",
        "MVE_BASE_DESTROY_SESSION",
        "MVE_BASE_ACTIVATE_SESSION",
        "MVE_BASE_ENQUEUE_FLUSH_BUFFERS",
        "MVE_BASE_ENQUEUE_STATE_CHANGE",
        "MVE_BASE_GET_EVENT",
        "MVE_BASE_SET_OPTION",
        "MVE_BASE_BUFFER_PARAM",
        "MVE_BASE_REGISTER_BUFFER",
        "MVE_BASE_UNREGISTER_BUFFER",
        "MVE_BASE_FILL_THIS_BUFFER",
        "MVE_BASE_EMPTY_THIS_BUFFER",
        "MVE_BASE_NOTIFY_REF_FRAME_RELEASE",
        "MVE_BASE_REQUEST_MAX_FREQUENCY",
        "MVE_BASE_READ_HW_INFO",
        "MVE_BASE_RPC_MEM_ALLOC",
        "MVE_BASE_RPC_MEM_RESIZE",
        "MVE_BASE_DEBUG_READ_REGISTER",
        "MVE_BASE_DEBUG_WRITE_REGISTER",
        "MVE_BASE_DEBUG_INTERRUPT_COUNT",
        "MVE_BASE_DEBUG_SEND_COMMAND",
        "MVE_BASE_DEBUG_FIRMWARE_HUNG_SIMULATION",
    };

    if (MVE_BASE_DEBUG_FIRMWARE_HUNG_SIMULATION < cmd)
    {
        return "Unknown command";
    }

    return name[cmd];
}

struct mve_response *mve_command_execute(struct mve_base_command_header *header,
                                         void *data,
                                         struct file *filep)
{
#define MVE_LOG_COMMAND(_severity)                                                                                   \
    MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,                                                                     \
                          _severity,                                                                                 \
                          session,                                                                                   \
                          "mve_command_execute(header={cmd=%u \"%s\", size=%u, data[0]=%u}, data=0x%x, file=0x%x).", \
                          header->cmd,                                                                               \
                          mve_command_to_string(header->cmd),                                                        \
                          header->size,                                                                              \
                          header->data[0],                                                                           \
                          data,                                                                                      \
                          filep);

    struct mve_response *ret;
    struct mve_session *session;

    /* Allocate and initialize return struct */
    ret = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_response), GFP_KERNEL);
    if (NULL == ret)
    {
        MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Failed to allocate memory for the ioctl return struct.");
        return NULL;
    }

    ret->error = MVE_BASE_ERROR_NONE;
    ret->firmware_error = 0;
    ret->size = 0;
    ret->data = NULL;

    /* Find session associated with file pointer */
    session = mve_session_get_by_file(filep);

//    printk("%s : cmd %d %s enter\n", __func__, header->cmd, mve_command_to_string(header->cmd));

    switch (header->cmd)
    {
        case MVE_BASE_CREATE_SESSION:
            if (NULL != session)
            {
                /* Multiple sessions for one file descriptor is not allowed! */
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "Failed to create session. Only one session per file descriptor is allowed.");
            }
            else
            {
                ret->error = mve_command_create_session_helper(data, filep);
                MVE_LOG_COMMAND(MVE_LOG_INFO);
            }
            break;
        case MVE_BASE_DESTROY_SESSION:
            MVE_LOG_COMMAND(MVE_LOG_INFO);
            mve_session_destroy(session);
            break;
        case MVE_BASE_ACTIVATE_SESSION:
        {
            bool res;
            uint32_t *version;
            struct mve_base_fw_secure_descriptor *fw_secure_desc;

            MVE_LOG_COMMAND(MVE_LOG_INFO);
            fw_secure_desc = (struct mve_base_fw_secure_descriptor *)data;
            version = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(uint32_t), GFP_KERNEL);
            if (NULL != version)
            {
                res = mve_session_activate(session, version, fw_secure_desc);
                if (false != res)
                {
                    ret->data = version;
                    ret->size = sizeof(uint32_t);
                }
                else
                {
                    ret->error = MVE_BASE_ERROR_UNDEFINED;
                    MVE_RSRC_MEM_CACHE_FREE(version, sizeof(uint32_t));
                }
            }
            else
            {
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "Failed to allocate memory for the ioctl return struct's data member.");
                printk("%s : activate session faile no resource!\n", __func__);
            }
            break;
        }
        case MVE_BASE_ENQUEUE_FLUSH_BUFFERS:
        {
            uint32_t flush = ((uint32_t *)data)[0];

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_enqueue_flush_buffers(session, flush);
            break;
        }
        case MVE_BASE_ENQUEUE_STATE_CHANGE:
        {
            enum mve_base_hw_state state = ((uint32_t *)data)[0];

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_enqueue_state_change(session, state);
            break;
        }
        case MVE_BASE_GET_EVENT:
        {
            struct mve_base_event_header *event;
            uint32_t timeout;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            timeout = ((uint32_t *)data)[0];
            event = mve_session_get_event(session, timeout);
            if (NULL == event)
            {
                ret->error = MVE_BASE_ERROR_TIMEOUT;
            }
            else
            {
                ret->size = sizeof(struct mve_base_event_header) + event->size;
                ret->data = event;
            }
            break;
        }
        case MVE_BASE_SET_OPTION:
        case MVE_BASE_BUFFER_PARAM:
        {
            ret->error = mve_session_set_paramconfig(session,
                                                     header->size,
                                                     data,
                                                     &ret->firmware_error,
                                                     header->cmd);
            if (ret->error == MVE_BASE_ERROR_NONE)
            {
                MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            }
            break;
        }
        case MVE_BASE_REGISTER_BUFFER:
        {
            uint32_t port_index = *((uint32_t *)data);
            struct mve_base_buffer_userspace *descriptor =
                (struct mve_base_buffer_userspace *)(data + sizeof(uint32_t));

            /* Verify that the user and kernel space versions of the mve_buffer_userspace
             * match in at least size */
            WARN_ON(header->size != sizeof(uint32_t) + sizeof(struct mve_base_buffer_userspace));

            MVE_LOG_COMMAND(MVE_LOG_INFO);
            if (1 < port_index)
            {
                ret->error = MVE_BASE_ERROR_BAD_PORT_INDEX;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "%u is greater than 1 which is not a valid port_index.",
                                      port_index);
            }
            else
            {
                ret->error = mve_session_buffer_register(session, port_index, descriptor);
            }
            break;
        }
        case MVE_BASE_UNREGISTER_BUFFER:
        {
            mve_base_buffer_handle_t buffer_id = *((mve_base_buffer_handle_t *)data);
            /* Must check for NULL here to avoid returning an error code when a buffer
             * is unregistered after the session has been removed */
            if (NULL != session)
            {
                MVE_LOG_COMMAND(MVE_LOG_INFO);
                ret->error = mve_session_buffer_unregister(session, buffer_id);
            }
            break;
        }
        case MVE_BASE_FILL_THIS_BUFFER:
        {
            struct mve_base_buffer_details *param = (struct mve_base_buffer_details *)data;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_buffer_enqueue(session, param, false);
            break;
        }
        case MVE_BASE_EMPTY_THIS_BUFFER:
        {
            struct mve_base_buffer_details *param = (struct mve_base_buffer_details *)data;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_buffer_enqueue(session, param, true);
            break;
        }
        case MVE_BASE_NOTIFY_REF_FRAME_RELEASE:
        {
            mve_base_buffer_handle_t buffer_id = *((mve_base_buffer_handle_t *)data);

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_buffer_notify_ref_frame_release(session, buffer_id);
            break;
        }
        case MVE_BASE_REQUEST_MAX_FREQUENCY:
        {
            MVE_LOG_COMMAND(MVE_LOG_DEBUG);

            mver_dvfs_request_max_frequency();
            ret->error = MVE_BASE_ERROR_NONE;
            break;
        }
        case MVE_BASE_READ_HW_INFO:
        {
            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->data = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_base_hw_info), GFP_KERNEL);
            if (NULL == ret->data)
            {
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "Failed to allocate memory for the ioctl return struct's data member.");
            }
            else
            {
                struct mve_base_hw_info *hw_info;
                hw_info = (struct mve_base_hw_info *)ret->data;
                ret->size = sizeof(struct mve_base_hw_info);
                hw_info->fuse = mver_reg_get_fuse();
                hw_info->version = mver_reg_get_version();
                hw_info->ncores = mver_scheduler_get_ncores();
            }
            break;
        }
        case MVE_BASE_RPC_MEM_ALLOC:
        {
#ifndef EMULATOR
            int32_t fd;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);

            fd = *((int32_t *)data);
            ret->error = mve_session_handle_rpc_mem_alloc(session, fd);
#endif      /* EMULATOR */
            break;
        }
        case MVE_BASE_RPC_MEM_RESIZE:
        {
#ifndef EMULATOR
            int32_t fd;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);

            fd = *((int32_t *)data);
            ret->error = mve_session_handle_rpc_mem_resize(session, fd);
#endif      /* EMULATOR */
            break;
        }
#ifdef UNIT
        case MVE_BASE_DEBUG_READ_REGISTER:
            WARN_ON(header->size != sizeof(uint32_t));

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->data = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(uint32_t), GFP_KERNEL);
            if (NULL == ret->data)
            {
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "Failed to allocate memory for the ioctl return struct's data member.");
            }
            else
            {
                tCS *regs;
                uint32_t *base;
                uint32_t offset;
                uint32_t value;

                regs = mver_reg_get_coresched_bank();
                base = (uint32_t *)regs;
                offset = ((uint32_t *)data)[0] / sizeof(uint32_t);
                value = mver_reg_read32(base + offset);
                mver_reg_put_coresched_bank(&regs);

                ret->size = sizeof(uint32_t);
                *((uint32_t *)ret->data) = value;
            }
            break;
        case MVE_BASE_DEBUG_WRITE_REGISTER:
            MVE_LOG_COMMAND(MVE_LOG_DEBUG);

            WARN_ON(header->size != sizeof(uint32_t) * 2);
            {
                tCS *regs;
                uint32_t *base;
                uint32_t offset;
                uint32_t value;

                regs = mver_reg_get_coresched_bank();
                base = (uint32_t *)regs;
                offset = ((uint32_t *)data)[0] / sizeof(uint32_t);
                value = ((uint32_t *)data)[1];
                mver_reg_write32(base + offset, value);
                mver_reg_put_coresched_bank(&regs);
            }
            break;
        case MVE_BASE_DEBUG_INTERRUPT_COUNT:
            MVE_LOG_COMMAND(MVE_LOG_DEBUG);

            ret->data = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(uint32_t), GFP_KERNEL);
            if (NULL == ret->data)
            {
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
                MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                      MVE_LOG_ERROR,
                                      session,
                                      "Failed to allocate memory for the ioctl return struct's data member.");
            }
            else
            {
                *((uint32_t *)ret->data) = mve_rsrc_data.interrupts;
                ret->size = sizeof(uint32_t);
            }
            break;
        case MVE_BASE_DEBUG_SEND_COMMAND:
        {
            uint32_t *ptr = (uint32_t *)data;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_enqueue_message(session, ptr[0], ptr[1], &ptr[2]);
            break;
        }
        case MVE_BASE_DEBUG_FIRMWARE_HUNG_SIMULATION:
        {
            /* unload the firmware to simulate firmware hung and no longer
             * response for watchdog ping
             */
#ifndef DISABLE_WATCHDOG
            uint32_t on = *(uint32_t *)data;
            mve_session_firmware_hung_simulation(session, on);
#endif

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            break;
        }
#endif
#if 0
        case MVE_BASE_DEBUG_SEND_COMMAND:
        {
            uint32_t *ptr = (uint32_t *)data;

            MVE_LOG_COMMAND(MVE_LOG_DEBUG);
            ret->error = mve_session_enqueue_message(session, ptr[0], ptr[1], &ptr[2]);
            break;
        }
#endif

        case MVE_BASE_DMA_ALLOC:
        {
            struct mve_dma_alloc *dma_alloc = (struct mve_dma_alloc *)data;
            int fd;
            printk("IOCTL : MVE_BASE_DMA_ALLOC\n");
            fd = mve_command_alloc_dma(filep, dma_alloc);
            printk("IOCTL : MVE_BASE_DMA_ALLOC fd %d\n", fd);

            if (!fd)
            {
                ret->error = MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
            }
            else
            {
                ret->error = MVE_BASE_ERROR_NONE;
            }

            ret->size = sizeof(fd);

            ret->data = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(int), GFP_KERNEL);
            *((int *)ret->data) = fd;

            break;
        }

        case MVE_BASE_VERSION:
        {
            int32_t *version;
            ret->data = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(int32_t) * 3, GFP_KERNEL);

            version = ret->data;
            version[0] = KDRIVER_MAJOR;
            version[1] = KDRIVER_MINOR;
            version[2] = KDRIVER_BUILD;

            ret->size = sizeof(int32_t) * 3;
            break;
        }

        default:
            MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                                  MVE_LOG_ERROR,
                                  session,
                                  "Invalid command. command=%u.",
                                  header->cmd);
            break;
    }

    WARN_ON(ret->data != NULL && ret->size == 0);

    if (ret->error != MVE_BASE_ERROR_NONE)
    {
        enum mve_rsrc_log_severity severity = MVE_LOG_WARNING;

        if ((ret->error == MVE_BASE_ERROR_TIMEOUT) || (ret->error == MVE_BASE_ERROR_NOT_READY))
        {
            severity = MVE_LOG_VERBOSE;
        }

        MVE_LOG_PRINT_SESSION(&mve_rsrc_log_session,
                              severity,
                              session,
                              "Ioctl failed. command=\"%s\", error=%u.",
                              mve_command_to_string(header->cmd),
                              ret->error);
    }

    return ret;
}
