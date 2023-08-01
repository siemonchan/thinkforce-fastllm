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
#include <linux/dma-mapping.h>
#endif

#include "mve_rsrc_mem_dma_uncached.h"
#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_driver.h"

/**
 * Private structure to the uncached DMA memory allocator
 */
struct dma_mem_uncached_t
{
    struct mve_rsrc_dma_mem_t header; /**< DON'T MOVE! This member must be first in the struct */

    void *cpu_addr;                   /**< Virtual CPU address to the first byte of the allocation */
    dma_addr_t dma_handle;            /**< DMA address (same as the physical address of the first
                                       *   page of the allocation) */
    phys_addr_t *pages;               /**< Array containing the allocated physical pages */
};

struct mve_rsrc_dma_mem_t *mve_rsrc_dma_mem_alloc_uncached3(uint32_t size)
{
    struct dma_mem_uncached_t *mem;

    mem = MVE_RSRC_MEM_ZALLOC(sizeof(struct dma_mem_uncached_t), GFP_KERNEL);
    if (NULL != mem)
    {
        mem->header.type = DMA_MEM_TYPE_UNCACHED;
        mem->header.size = size;

        mem->cpu_addr = dma_alloc_coherent(&mve_rsrc_data3.pdev->dev, size, &mem->dma_handle, GFP_KERNEL);
        if (NULL == mem->cpu_addr)
        {
            MVE_RSRC_MEM_FREE(mem);
            mem = NULL;
        }
        else
        {
            /* dma_zalloc_coherent doesn't clear the memory on arm64 */
            memset(mem->cpu_addr, 0, size);
        }
    }

    return (struct mve_rsrc_dma_mem_t *)mem;
}

void mve_rsrc_dma_mem_free_uncached3(struct mve_rsrc_dma_mem_t *mem)
{
    struct dma_mem_uncached_t *ptr = (struct dma_mem_uncached_t *)mem;

    dma_free_coherent(&mve_rsrc_data3.pdev->dev, ptr->header.size, ptr->cpu_addr, ptr->dma_handle);
    if (NULL != ptr->pages)
    {
        MVE_RSRC_MEM_VFREE(ptr->pages);
    }
    MVE_RSRC_MEM_FREE(mem);
}

void mve_rsrc_dma_mem_clean_cache_uncached3(struct mve_rsrc_dma_mem_t *mem)
{
    /* Nop */
    mve_rsrc_mem_flush_write_buffer3();
}

void mve_rsrc_dma_mem_invalidate_cache_uncached3(struct mve_rsrc_dma_mem_t *mem)
{
    /* Nop */
    mve_rsrc_mem_flush_write_buffer3();
}

void *mve_rsrc_dma_mem_map_uncached3(struct mve_rsrc_dma_mem_t *mem)
{
    struct dma_mem_uncached_t *ptr = (struct dma_mem_uncached_t *)mem;

    return ptr->cpu_addr;
}

void mve_rsrc_dma_mem_unmap_unchached3(struct mve_rsrc_dma_mem_t *mem)
{
    /* Nop */
}

phys_addr_t *mve_rsrc_dma_mem_get_pages_uncached3(struct mve_rsrc_dma_mem_t *mem)
{
    struct dma_mem_uncached_t *ptr = (struct dma_mem_uncached_t *)mem;

    if (NULL == ptr->pages)
    {
        /* Construct a list containing the pages of the allocation */
        uint32_t i;
        uint32_t num_pages = (ptr->header.size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        ptr->pages = MVE_RSRC_MEM_VALLOC(sizeof(phys_addr_t) * num_pages);
        if (NULL == ptr->pages)
        {
            return NULL;
        }

        for (i = 0; i < num_pages; ++i)
        {
            ptr->pages[i] = ptr->dma_handle + i * PAGE_SIZE;
        }
    }

    return ptr->pages;
}
