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

#ifndef MVE_RSRC_MEM_FRONTEND_H
#define MVE_RSRC_MEM_FRONTEND_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#endif

#include "mve_rsrc_mem_backend.h"
#include "mve_rsrc_mem_cache.h"

#if (1 == MVE_MEM_DBG_SUPPORT)

enum mve_rsrc_mem_allocator {ALLOCATOR_KMALLOC, ALLOCATOR_VMALLOC, ALLOCATOR_CACHE};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))

#define MVE_RSRC_MEM_CACHE_ALLOC(size, flags) \
    mve_rsrc_mem_zalloc_track3(size, flags, ALLOCATOR_CACHE, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_CACHE_FREE(ptr, size) \
    mve_rsrc_mem_free_track3(ptr, size, ALLOCATOR_CACHE, __FUNCTION__, __LINE__)

#else
#define MVE_RSRC_MEM_CACHE_ALLOC(size, flags) \
    mve_rsrc_mem_zalloc_track3(size, flags, ALLOCATOR_KMALLOC, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_CACHE_FREE(ptr, size) \
    mve_rsrc_mem_free_track3(ptr, size, ALLOCATOR_KMALLOC, __FUNCTION__, __LINE__)

#endif

#define MVE_RSRC_MEM_ZALLOC(size, flags) \
    mve_rsrc_mem_zalloc_track3(size, flags, ALLOCATOR_KMALLOC, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_FREE(ptr) \
    mve_rsrc_mem_free_track3(ptr, 0, ALLOCATOR_KMALLOC, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_VALLOC(size) \
    mve_rsrc_mem_zalloc_track3(size, 0, ALLOCATOR_VMALLOC, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_VFREE(ptr) \
    mve_rsrc_mem_free_track3(ptr, 0, ALLOCATOR_VMALLOC, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_ALLOC_PAGE() \
    mve_rsrc_mem_alloc_page_track3(__FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_FREE_PAGE(paddr) \
    mve_rsrc_mem_free_page_track3(paddr, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_ALLOC_PAGES(nr_pages) \
    mve_rsrc_mem_alloc_pages_track3(nr_pages, __FUNCTION__, __LINE__)

#define MVE_RSRC_MEM_FREE_PAGES(paddrs, nr_pages) \
    mve_rsrc_mem_free_pages_track3(paddrs, nr_pages, __FUNCTION__, __LINE__)

/**
 * Allocate heap memory using the supplied allocator and add the allocation to the memory tracking table.
 * @param size      Size in bytes of the allocation.
 * @param flags     Allocation flags. See the documentation of kalloc.
 * @param allocator Which allocator to use when allocating the memory.
 * @param func_str  Function name of the allocation origin.
 * @param line_nr   Line number of the allocation origin.
 * @return Virtual  address of the allocation on success, NULL on failure.
 */
void *mve_rsrc_mem_zalloc_track3(uint32_t size,
                                uint32_t flags,
                                enum mve_rsrc_mem_allocator allocator,
                                const char *func_str,
                                int line_nr);

/**
 * Free memory allocated with mve_rsrc_mem_zalloc_track and remove the allocation
 * from the memory tracking table.
 * @param size      size of the corresponding allocation only used for cached memory.
 * @param ptr       Pointer to the memory to free.
 * @param allocator The allocator that was used to allocate the memory.
 * @param func_str  Function name of the free origin.
 * @param line_nr   Line number of the free origin.
 */
void mve_rsrc_mem_free_track3(void *ptr,
                             uint32_t size,
                             enum mve_rsrc_mem_allocator allocator,
                             const char *func_str,
                             int line_nr);

/**
 * Allocate one page of physical memory and add it to the memory tracking table
 * @param func_str Function name of the allocation origin.
 * @param line_nr  Line number of the allocation origin.
 * @return Physical address of the allocated page.
 */
phys_addr_t mve_rsrc_mem_alloc_page_track3(const char *func_str, int line_nr);

/**
 * Free one page of physical memory and remove it from the memory tracking table.
 * @param paddr    Physical address of the page to free.
 * @param func_str Function name of the free origin.
 * @param line_nr  Line number of the free origin.
 */
void mve_rsrc_mem_free_page_track3(phys_addr_t paddr, const char *func_str, int line_nr);

/**
 * Allocate a set of pages and add the allocations to the memory tracking table.
 * The client must free the returned array with MVE_RSRC_MEM_FREE or there will
 * be a memory leak.
 * @param nr_pages Number of pages to allocate.
 * @param func_str Function name of the allocation origin.
 * @param line_nr  Line number of the allocation origin.
 * @return An array of pages on success, NULL on failure. Note that the client
 *         must free the returned array to prevent memory leaks.
 */
phys_addr_t *mve_rsrc_mem_alloc_pages_track3(uint32_t nr_pages,
                                            const char *func_str,
                                            int line_nr);

/**
 * Free a set of pages and remove them from the memory tracking table.
 * The supplied array is freed with MVE_RSRC_MEM_FREE so there is no need for
 * the client to free this memory.
 * @param paddrs   Array containing pages to free.
 * @param nr_pages Number of pages in the paddrs array.
 * @param func_str Function name of the free origin.
 * @param line_nr  Line number of the free origin.
 */
void mve_rsrc_mem_free_pages_track3(phys_addr_t *paddrs,
                                   uint32_t nr_pages,
                                   const char *func_str,
                                   int line_nr);

/**
 * Print the contents of the memory allocation table into the supplied string
 * array.
 * @param buf The string to put the data in.
 * @return The number of characters copied to the array.
 */
ssize_t mve_rsrc_mem_print_stack3(char *buf);

/**
 * Clear the contents of the memory allocation table.
 */
void mve_rsrc_mem_clear_stack3(void);

/**
 * Enable/disable memory allocation failure simulation
 * @param enable True to enable, false to disable.
 */
void mve_rsrc_mem_resfail_enable3(bool enable);

/**
 * Set the range for the allocations that shall fail.
 * @param min Start of the range.
 * @param end End of the range. Note that this argument is ignored in the
 *            current implementation.
 */
void mve_rsrc_mem_resfail_set_range3(uint32_t min, uint32_t max);

/**
 * Has a memory allocation failure been simulated since the last reset?
 * @return 1 if a failure has been simulated, 0 if not.
 */
uint32_t mve_rsrc_mem_resfail_did_fail3(void);

#else

/* Allocate physically contiguous memory using kmalloc */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))

#define MVE_RSRC_MEM_CACHE_ALLOC(size, flags) \
    mve_rsrc_mem_cache_alloc3(size, flags)

#define MVE_RSRC_MEM_CACHE_FREE(ptr, size) \
    mve_rsrc_mem_cache_free3(ptr, size)

#else
#define MVE_RSRC_MEM_CACHE_ALLOC(size, flags) \
    kzalloc(size, flags)

#define MVE_RSRC_MEM_CACHE_FREE(ptr, size) \
    kfree(ptr)

#endif

#define MVE_RSRC_MEM_ZALLOC(size, flags) \
    kzalloc(size, flags)

#define MVE_RSRC_MEM_FREE(ptr) \
    kfree(ptr)

/* Allocate non physically contiguous memory using vmalloc */
#define MVE_RSRC_MEM_VALLOC(size) \
    vmalloc(size)

#define MVE_RSRC_MEM_VFREE(ptr) \
    vfree(ptr)

#define MVE_RSRC_MEM_ALLOC_PAGE() \
    mve_rsrc_mem_alloc_page3()

#define MVE_RSRC_MEM_FREE_PAGE(paddr) \
    mve_rsrc_mem_free_page3(paddr)

#define MVE_RSRC_MEM_ALLOC_PAGES(nr_pages) \
    mve_rsrc_mem_alloc_pages3(nr_pages)

#define MVE_RSRC_MEM_FREE_PAGES(paddrs, nr_pages) \
    mve_rsrc_mem_free_pages3(paddrs, nr_pages)

#endif /* MVE_MEM_DBG_SUPPORT */

/**
 * Performs all necessary initialization of the memory frontend.
 */
void mve_rsrc_mem_init3(struct device *dev);

/**
 * Performs all necessary deinitialization of the memory frontend.
 */
void mve_rsrc_mem_deinit3(struct device *dev);

#endif /* MVE_RSRC_MEM_FRONTEND_H */
