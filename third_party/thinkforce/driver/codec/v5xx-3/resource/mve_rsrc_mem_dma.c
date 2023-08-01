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
#include <linux/types.h>
#include <linux/export.h>
#include <linux/kernel.h>
#endif

#include "mve_rsrc_mem_dma.h"
#include "mve_rsrc_mem_dma_uncached.h"

/**
 * Function pointers to a DMA memory implementation
 */
struct dma_mem_fptr
{
    struct mve_rsrc_dma_mem_t *(*alloc)(uint32_t size);
    void (*free)(struct mve_rsrc_dma_mem_t *mem);
    void (*clean_cache)(struct mve_rsrc_dma_mem_t *mem);
    void (*invalidate_cache)(struct mve_rsrc_dma_mem_t *mem);
    void *(*map)(struct mve_rsrc_dma_mem_t *mem);
    void (*unmap)(struct mve_rsrc_dma_mem_t *mem);
    phys_addr_t *(*get_pages)(struct mve_rsrc_dma_mem_t *mem);
};

static struct dma_mem_fptr fptrs[] =
{
    {
        mve_rsrc_dma_mem_alloc_uncached3,
        mve_rsrc_dma_mem_free_uncached3,
        mve_rsrc_dma_mem_clean_cache_uncached3,
        mve_rsrc_dma_mem_invalidate_cache_uncached3,
        mve_rsrc_dma_mem_map_uncached3,
        mve_rsrc_dma_mem_unmap_unchached3,
        mve_rsrc_dma_mem_get_pages_uncached3,
    }
};

struct mve_rsrc_dma_mem_t *mve_rsrc_dma_mem_alloc3(uint32_t size, enum mve_rsrc_dma_mem_type type)
{
    if (0 == size)
    {
        return NULL;
    }

    if (type >= DMA_MEM_TYPE_MAX)
    {
        return NULL;
    }

    return fptrs[type].alloc(size);
}

void mve_rsrc_dma_mem_free3(struct mve_rsrc_dma_mem_t *mem)
{
    if (NULL != mem)
    {
        fptrs[mem->type].free(mem);
    }
}

void mve_rsrc_dma_mem_clean_cache3(struct mve_rsrc_dma_mem_t *mem)
{
    if (NULL != mem)
    {
        fptrs[mem->type].clean_cache(mem);
    }
}

void mve_rsrc_dma_mem_invalidate_cache3(struct mve_rsrc_dma_mem_t *mem)
{
    if (NULL != mem)
    {
        fptrs[mem->type].invalidate_cache(mem);
    }
}

void *mve_rsrc_dma_mem_map3(struct mve_rsrc_dma_mem_t *mem)
{
    void *ret = NULL;

    if (NULL != mem)
    {
        ret = fptrs[mem->type].map(mem);
    }

    return ret;
}

void mve_rsrc_dma_mem_unmap3(struct mve_rsrc_dma_mem_t *mem)
{
    if (NULL != mem)
    {
        fptrs[mem->type].unmap(mem);
    }
}

phys_addr_t *mve_rsrc_dma_mem_get_pages3(struct mve_rsrc_dma_mem_t *mem)
{
    phys_addr_t *ret = NULL;

    if (NULL != mem)
    {
        ret = fptrs[mem->type].get_pages(mem);
    }

    return ret;
}

EXPORT_SYMBOL(mve_rsrc_dma_mem_alloc3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_free3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_clean_cache3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_invalidate_cache3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_map3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_unmap3);
EXPORT_SYMBOL(mve_rsrc_dma_mem_get_pages3);
