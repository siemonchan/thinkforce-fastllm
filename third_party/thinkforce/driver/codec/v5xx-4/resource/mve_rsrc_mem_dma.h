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

#ifndef MVE_RSRC_MEM_DMA_H
#define MVE_RSRC_MEM_DMA_H

/**
 * DMA memory type.
 */
enum mve_rsrc_dma_mem_type
{
    DMA_MEM_TYPE_UNCACHED,  /**< Uncached DMA memory */
    DMA_MEM_TYPE_MAX
};

/**
 * DMA memory handle.
 */
struct mve_rsrc_dma_mem_t
{
    enum mve_rsrc_dma_mem_type type; /**< Memory type (cached/uncached) */
    uint32_t size;                   /**< Size of the memory region in bytes */
};

/**
 * Alloc DMA memory. The memory can either be cached or uncached.
 * @param size Number of bytes to allocate
 * @param type The kind of DMA memory (cached/uncached)
 * @param DMA memory handle
 */
struct mve_rsrc_dma_mem_t *mve_rsrc_dma_mem_alloc4(uint32_t size, enum mve_rsrc_dma_mem_type type);

/**
 * Free DMA memory.
 * @param mem Handle representing the memory region to free
 */
void mve_rsrc_dma_mem_free4(struct mve_rsrc_dma_mem_t *mem);

/**
 * Clean the CPU cache. All cache lines holding information about the
 * DMA memory region have been evicted from the cache and written to memory
 * when this function returns.
 * @param mem Handle to the DMA memory to evict from the CPU cache
 */
void mve_rsrc_dma_mem_clean_cache4(struct mve_rsrc_dma_mem_t *mem);

/**
 * Invalidate the CPU cache. All cache lines holding information about the
 * DMA memory region have been invalidated when this function returns. The
 * next CPU read will therefore go all the way down to the memory.
 * @param mem Handle to the DMA memory to invalidate from the CPU cache
 */
void mve_rsrc_dma_mem_invalidate_cache4(struct mve_rsrc_dma_mem_t *mem);

/**
 * CPU map DMA memory. Returns a virtual address to the DMA memory that the CPU
 * can use to access the memory. Note that the client must unmap the memory
 * when access is no longer needed.
 * @param mem Handle to the DMA memory to CPU map
 * @return Virtual address to the DMA memory
 */
void *mve_rsrc_dma_mem_map4(struct mve_rsrc_dma_mem_t *mem);

/**
 * CPU unmap DMA memory. The virtual address returned by mve_rsrc_dma_mem_map
 * is not valid after invoking this function. It's safe to unmap memory that is not
 * currently mapped.
 * @param mem Handle to the DMA memory to unmap
 */
void mve_rsrc_dma_mem_unmap4(struct mve_rsrc_dma_mem_t *mem);

/**
 * Returns an array containing the physical addresses of the pages that make up
 * the DMA memory region identified by the supplied handle.
 * @param mem Handle to the DMA memory
 * @return An array of physical addresses or NULL on failure. Do not free
 *         the returned array since this is handled by the DMA memory submodule.
 */
phys_addr_t *mve_rsrc_dma_mem_get_pages4(struct mve_rsrc_dma_mem_t *mem);

#endif /* MVE_RSRC_MEM_DMA_H */
