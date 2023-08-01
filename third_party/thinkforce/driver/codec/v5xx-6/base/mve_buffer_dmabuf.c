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

#include "mve_buffer_dmabuf.h"
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#include "mve_driver.h"

#include "mve_rsrc_driver.h"
#include "mve_rsrc_mem_frontend.h"

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x) ((void)(x))
#endif

MODULE_IMPORT_NS(DMA_BUF);
/** @brief Buffer-client for the dmabuf interface.
 */
struct dmabuf_buffer_external
{
    struct mve_buffer_external buffer;     /**< Buffer client common data.
                                            *   This member must be the first entry
                                            *   of the struct */
    struct dma_buf *handle;                /**< The dma_buf handle */
    struct dma_buf_attachment *attachment; /**< Pointer to the attechment */
    struct sg_table *sg;                   /**< Pointer to the scatter-gather */
};

struct mve_buffer_external *mve_buffer_dmabuf_create_buffer_external6(struct mve_buffer_info *info)
{
    struct dmabuf_buffer_external *buffer_external = NULL;
    unsigned long fd;

    if (NULL == info)
    {
        return NULL;
    }

    buffer_external = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct dmabuf_buffer_external) + 100, GFP_KERNEL);
    if (NULL == buffer_external)
    {
        return NULL;
    }

    fd = (unsigned long)info->handle;
    buffer_external->handle = dma_buf_get((int)fd);
    if (IS_ERR_OR_NULL(buffer_external->handle))
    {
        goto error;
    }

    buffer_external->attachment = dma_buf_attach(buffer_external->handle, &mve_rsrc_data6.pdev->dev);
    if (IS_ERR(buffer_external->attachment))
    {
        goto error;
    }

    buffer_external->buffer.info = *info;

    return &buffer_external->buffer;

error:
    if (NULL != buffer_external)
    {
        if (!IS_ERR_OR_NULL(buffer_external->handle))
        {
            dma_buf_put(buffer_external->handle);
        }

        MVE_RSRC_MEM_CACHE_FREE(buffer_external, sizeof(struct dmabuf_buffer_external) + 100);
    }

    return NULL;
}

void mve_buffer_dmabuf_destroy_buffer_external6(struct mve_buffer_external *buffer)
{
    struct dmabuf_buffer_external *buffer_external;

    if (NULL == buffer)
    {
        return;
    }

    buffer_external = (struct dmabuf_buffer_external *)buffer;

    dma_buf_detach(buffer_external->handle, buffer_external->attachment);
    dma_buf_put(buffer_external->handle);
    MVE_RSRC_MEM_CACHE_FREE(buffer_external, sizeof(struct dmabuf_buffer_external) + 100);
}

bool mve_buffer_dmabuf_map_physical_pages6(struct mve_buffer_external *buffer)
{
    struct dmabuf_buffer_external *buffer_external;
    struct scatterlist *sgl;
    uint32_t num_pages;
    phys_addr_t *pages = NULL;
    uint32_t i;
    uint32_t count = 0;

    buffer_external = (struct dmabuf_buffer_external *)buffer;

    buffer_external->sg = dma_buf_map_attachment(buffer_external->attachment, DMA_BIDIRECTIONAL);
    if (IS_ERR(buffer_external->sg))
    {
        return false;
    }

    num_pages = (buffer->info.size + PAGE_SIZE - 1) / PAGE_SIZE;
    pages = MVE_RSRC_MEM_VALLOC(sizeof(phys_addr_t) * num_pages);
    if (NULL == pages)
    {
        goto error;
    }

    /* Extract the address of each page and save the address in the pages array */
    for_each_sg(buffer_external->sg->sgl, sgl, buffer_external->sg->nents, i)
    {
        uint32_t npages = PFN_UP(sg_dma_len(sgl));
        uint32_t j;

        for (j = 0; (j < npages) && (count < num_pages); ++j)
        {
            pages[count++] = sg_dma_address(sgl) + (j << PAGE_SHIFT);
        }
        WARN_ONCE(j < npages, "[MVE] scatterlist is bigger than the expected size (%d pages)\n", num_pages);
    }

    if (WARN_ONCE(count < num_pages, "[MVE] scatterlist doesn't contain enough pages (found: %d expected: %d pages)\n",
                  count, num_pages))
    {
        goto error;
    }

    /* Register buffer as an external mapping */
    buffer->mapping.pages = pages;
    buffer->mapping.num_pages = count;

    return true;

error:
    dma_buf_unmap_attachment(buffer_external->attachment,
                             buffer_external->sg,
                             DMA_BIDIRECTIONAL);

    buffer_external->sg = NULL;
    MVE_RSRC_MEM_VFREE(pages);

    return false;
}

bool mve_buffer_dmabuf_unmap_physical_pages6(struct mve_buffer_external *buffer)
{
    struct dmabuf_buffer_external *buffer_external;

    if (NULL == buffer)
    {
        return false;
    }

    buffer_external = (struct dmabuf_buffer_external *)buffer;

    dma_buf_unmap_attachment(buffer_external->attachment,
                             buffer_external->sg,
                             DMA_BIDIRECTIONAL);
    MVE_RSRC_MEM_VFREE(buffer->mapping.pages);

    buffer_external->sg = NULL;
    buffer->mapping.pages = NULL;
    buffer->mapping.num_pages = 0;

    return true;
}

void mve_buffer_dmabuf_set_owner6(struct mve_buffer_external *buffer,
                                 enum mve_buffer_owner owner,
                                 enum mve_port_index port)
{
    void (*sync_func)(struct device *dev,
                      struct scatterlist *sg,
                      int nelems,
                      enum dma_data_direction direction);
    enum dma_data_direction direction;

    struct dmabuf_buffer_external *buffer_external = (struct dmabuf_buffer_external *)buffer;

    if (NULL == buffer)
    {
        return;
    }

    if (MVE_BUFFER_OWNER_CPU == owner && MVE_PORT_INDEX_INPUT == port)
    {
        return;
    }

    if (MVE_BUFFER_OWNER_CPU == owner)
    {
        sync_func = dma_sync_sg_for_cpu;
    }
    else
    {
        sync_func = dma_sync_sg_for_device;
    }

    if (MVE_PORT_INDEX_OUTPUT == port)
    {
        /* Data going from device to CPU */
        direction = DMA_FROM_DEVICE;
    }
    else
    {
        /* Data going from CPU to device */
        direction = DMA_TO_DEVICE;
    }

    sync_func(&mve_rsrc_data6.pdev->dev, buffer_external->sg->sgl, buffer_external->sg->nents, direction);
}
