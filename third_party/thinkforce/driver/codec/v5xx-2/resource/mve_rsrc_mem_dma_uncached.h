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

#ifndef MVE_RSRC_MEM_DMA_UNCACHED_H
#define MVE_RSRC_MEM_DMA_UNCACHED_H

#include "mve_rsrc_mem_dma.h"

/**
 * Allocate uncached memory.
 * @param size Number of bytes to allocate
 * @return Handle to the allocated memory
 */
struct mve_rsrc_dma_mem_t *mve_rsrc_dma_mem_alloc_uncached2(uint32_t size);

/**
 * Free uncached memory.
 * @param mem Handle to the memory to free
 */
void mve_rsrc_dma_mem_free_uncached2(struct mve_rsrc_dma_mem_t *mem);

/**
 * Clean the CPU cache for the memory region pointed out by the supplied handle.
 * @param mem Handle to the memory
 */
void mve_rsrc_dma_mem_clean_cache_uncached2(struct mve_rsrc_dma_mem_t *mem);

/**
 * Invalidate the CPU cache for the memory region pointed out by the supplied handle.
 * @param mem Handle to the memory
 */
void mve_rsrc_dma_mem_invalidate_cache_uncached2(struct mve_rsrc_dma_mem_t *mem);

/**
 * Map a uncached memory region into CPU address space.
 * @param mem Handle to the memory to map
 * @return Virtual CPU address
 */
void *mve_rsrc_dma_mem_map_uncached2(struct mve_rsrc_dma_mem_t *mem);

/**
 * Unmap a uncached memory region from the CPU address space.
 * @param mem Handle to the memory
 */
void mve_rsrc_dma_mem_unmap_unchached2(struct mve_rsrc_dma_mem_t *mem);

/**
 * Get the address of the physical pages comprising the memory region pointed out by the supplied
 * handle.
 * @param mem Handle to the memory
 * @return Array of physical addresses. Do not free this array!
 */
phys_addr_t *mve_rsrc_dma_mem_get_pages_uncached2(struct mve_rsrc_dma_mem_t *mem);

#endif /* MVE_RSRC_MEM_DMA_UNCACHED_H */
