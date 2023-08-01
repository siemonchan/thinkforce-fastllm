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

#ifndef MVE_RSRC_MEM_CACHE_H
#define MVE_RSRC_MEM_CACHE_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

/**
 * Cache memory initialization.
 */
void mve_rsrc_mem_cache_init6(void);

/**
 * Cache memory de-initialization.
 */
void mve_rsrc_mem_cache_deinit6(void);
/**
 * Allocated the memory either from cache or kalloc.
 * @param size The requested allocation size.
 * @param flags Allocation flags.
 */
void *mve_rsrc_mem_cache_alloc6(uint32_t size, uint32_t flags);

/**
 * Free the memory either from cache or kalloc.
 * @param size The requested allocation size.
 * @param ptr the memory pointer to be freed.
 */
void mve_rsrc_mem_cache_free6(void *ptr, uint32_t size);

#endif /* MVE_RSRC_MEM_CACHE_H */
