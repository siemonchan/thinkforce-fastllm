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

#ifndef MVE_RSRC_MEM_BACKEND_H
#define MVE_RSRC_MEM_BACKEND_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

/**
 * Allocate one page of physical memory.
 * @return Physical address of the allocated page.
 */
phys_addr_t mve_rsrc_mem_alloc_page4(void);

/**
 * Free a physical page.
 * @param addr Address of the page that is to be freed.
 */
void mve_rsrc_mem_free_page4(phys_addr_t addr);

/**
 * Convenience function for freeing an array of pages. The supplied array will
 * also be freed with a call to kfree. Internally calls mve_rsrc_mem_free_page on
 * each page.
 * @param addrs     Array of pages to free.
 * @param num_pages Number of pages to free.
 */
void mve_rsrc_mem_free_pages4(phys_addr_t *addrs, uint32_t num_pages);

/**
 * Map a page into the virtual address space of the calling process. This function
 * returns a virtual address that can be used to access the contents stored
 * at the supplied physical address. Note that this function handles non page
 * aligned addresses. The client must free the mapping using
 * mve_rsrc_mem_cpu_unmap_page when the mapping is no longer needed.
 * @param addr Physical address to map.
 * @return CPU virtual address.
 */
void *mve_rsrc_mem_cpu_map_page4(phys_addr_t addr);

/**
 * Unmap a page previously mapped with mve_rsrc_mem_cpu_map_page.
 * @param addr Physical address of the page to unmap.
 */
void mve_rsrc_mem_cpu_unmap_page4(phys_addr_t addr);

/**
 * Pins and returns an array of physical pages corresponding to a region in
 * virtual CPU address space. The client must unpin the pages and free the
 * returned array by calling mve_rsrc_mem_unmap_virt_to_phys when the pages are
 * no longer needed.
 * @param ptr  Virtual address of the memory region.
 * @param size Size of the memory region.
 * @param write Map pages writable or read-only
 * @return Array of physical addresses of the pages. Returns NULL if the virtual
 *         address is NULL, not page aligned or size is 0. The client must free
 *         the returned array to prevent memory leaks.
 */
phys_addr_t *mve_rsrc_mem_map_virt_to_phys4(void *ptr, uint32_t size, uint32_t write);

/**
 * Unpins the pages and releases the array containing physical addresses of the
 * pages backing a memory region pointed out by a virtual CPU address.
 * @param pages    Pages corresponding to a region defined by a virtual CPU address.
 * @param nr_pages Number of pages
 */
void mve_rsrc_mem_unmap_virt_to_phys4(phys_addr_t *pages, uint32_t nr_pages);

/**
 * Allocates the specified number of pages and returns an array of physical
 * address. Note that the client must free the returned array using
 * kfree to prevent memory leaks. This is just a convenience function since it
 * calls mve_mem_alloc_page internally.
 * @param nr_pages Number of pages to allocate.
 * @return An array containing physical addresses of the allocated pages. Returns
 *         NULL on failure or if nr_pages equals 0.
 */
phys_addr_t *mve_rsrc_mem_alloc_pages4(uint32_t nr_pages);

/**
 * Read 32-bits of data from the specified physical address.
 * @param addr Physical address of the memory location to read.
 * @return 32-bits of data read.
 */
uint32_t mve_rsrc_mem_read324(phys_addr_t addr);

/**
 * Write 32-bits of data to the specified physical address.
 * @param addr Physical address of the memory location to write to.
 * @param value The value to write to memory.
 */
void mve_rsrc_mem_write324(phys_addr_t addr, uint32_t value);

/**
 * Clean the memory range specified by addr and size from the CPU cache. The cache
 * lines will be written to memory if they are marked as dirty. The lines will then
 * be marked as not dirty. The lines will not be evicted from the cache which means
 * that a subsequent memory access will hit the cache.
 * @param addr Physical address of the first byte to clean from the cache.
 * @param size Size of the block to clean.
 */
void mve_rsrc_mem_clean_cache_range4(phys_addr_t addr, uint32_t size);

/**
 * Invalidate the memory range specified by addr and size in the CPU cache. The
 * cached lines will be marked as non-valid even if they are marked as dirty. No
 * lines will be written to memory. A cache flush can be implemented as a clean
 * followed by an invalidate.
 * @param addr Physical address of the first byte to invalidate in the cache.
 * @param size Size of the block to invalidate.
 */
void mve_rsrc_mem_invalidate_cache_range4(phys_addr_t addr, uint32_t size);

/**
 * Flush the write buffer.
 */
void mve_rsrc_mem_flush_write_buffer4(void);

#endif /* MVE_RSRC_MEM_BACKEND_H */
