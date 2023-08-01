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
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#endif

#include "mve_mmu.h"
#include "mve_buffer_valloc.h"
#include "mve_driver.h"

#include "mve_rsrc_driver.h"

/* This is an implementation of the mve_buffer_client interface where the client
 * buffers have been allocated using vmalloc.
 */

struct mve_buffer_external *mve_buffer_valloc_create_buffer_external5(struct mve_buffer_info *info)
{
    /* The vmalloc implementation doesn't need to store any private data in
     * the mve_buffer_client structure. Just create an instance and return it. */
    struct mve_buffer_external *ret;

    ret = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_buffer_external), GFP_KERNEL);
    if (NULL != ret)
    {
        ret->info = *info;
    }

    return ret;
}

void mve_buffer_valloc_destroy_buffer_external5(struct mve_buffer_external *buffer)
{
    MVE_RSRC_MEM_CACHE_FREE(buffer, sizeof(struct mve_buffer_external));
}

bool mve_buffer_valloc_map_physical_pages5(struct mve_buffer_external *buffer)
{
    uint32_t size;
    void *ptr;
    uint32_t num_pages;
    uint32_t offset;
    phys_addr_t *pages;
    uint32_t num_mve_pages;

    buffer->mapping.pages = NULL;
    buffer->mapping.num_pages = 0;
    buffer->mapping.num_mve_pages = 0;


    size = buffer->info.size;
    ptr = (void *)(size_t)buffer->info.handle;
    /* Calculate the number of needed pages */
    offset = ((size_t)ptr) & (PAGE_SIZE - 1);
    size += offset;
    num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    num_mve_pages = (size + MVE_MMU_PAGE_SIZE - 1) / MVE_MMU_PAGE_SIZE;

    /* Get the physical pages */
    pages = mve_rsrc_mem_map_virt_to_phys5(ptr - offset, size, buffer->mapping.write);
    if (NULL == pages)
    {
        return false;
    }

    /* Register buffer as an external mapping */
    buffer->mapping.pages = pages;
    buffer->mapping.num_pages = num_pages;
    /* If the buffer is not page aligned */
    buffer->mapping.offset_in_page = offset;

    buffer->mapping.num_mve_pages = num_mve_pages;

    return true;
}

bool mve_buffer_valloc_unmap_physical_pages5(struct mve_buffer_external *buffer)
{
    /* mve_mem_unmap_virt_to_phys frees the memory allocated for the array
     * buffer->pages. */
    mve_rsrc_mem_unmap_virt_to_phys5(buffer->mapping.pages, buffer->mapping.num_pages);

    buffer->mapping.pages = NULL;
    buffer->mapping.num_pages = 0;

    return true;
}

void mve_buffer_valloc_set_owner5(struct mve_buffer_external *buffer,
                                 enum mve_buffer_owner owner,
                                 enum mve_port_index port)
{
    int i, len = 0;
    void (*sync_func)(struct device *dev,
                      dma_addr_t dma_handle,
                      size_t size,
                      enum dma_data_direction direction);
    enum dma_data_direction direction;

    if (MVE_BUFFER_OWNER_CPU == owner && MVE_PORT_INDEX_INPUT == port)
    {
        return;
    }

    if (MVE_BUFFER_OWNER_CPU == owner)
    {
        sync_func = dma_sync_single_for_cpu;
    }
    else
    {
        sync_func = dma_sync_single_for_device;
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

    for (i = 0; i < buffer->mapping.num_pages; ++i)
    {
        uint32_t offset;
        uint32_t size;

        if (i == 0)
        {
            offset = buffer->mapping.offset_in_page;
            size = PAGE_SIZE - buffer->mapping.offset_in_page;
        }
        else if (i == buffer->mapping.num_pages - 1)
        {
            offset = 0;
            size = buffer->info.size - len;
        }
        else
        {
            offset = 0;
            size = PAGE_SIZE;
        }

        len += size;
        /* Use the device struct from the resource module since this was used when
         * mapping the physical pages */
        sync_func(&mve_rsrc_data5.pdev->dev, buffer->mapping.pages[i] + offset, size, direction);
    }
}
