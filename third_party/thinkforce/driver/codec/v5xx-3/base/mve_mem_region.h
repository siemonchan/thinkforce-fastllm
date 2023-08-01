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

#ifndef MVE_MEM_REGION_H
#define MVE_MEM_REGION_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

typedef uint32_t mve_addr_t;

/**
 * Struct defining a MVE virtual memory region.
 */
struct mve_mem_virt_region
{
    mve_addr_t start;       /**< Virtual start address of the region */
    mve_addr_t end;         /**< Virtual end address of the region */
};

/**
 * This enum defines the different MVE virtual memory regions.
 */
enum mve_mem_virt_region_type
{
    VIRT_MEM_REGION_FIRST, /**< Region start, Not an actual memory region */
    VIRT_MEM_REGION_FIRMWARE0 = VIRT_MEM_REGION_FIRST,

    VIRT_MEM_REGION_MSG_IN_QUEUE,
    VIRT_MEM_REGION_MSG_OUT_QUEUE,
    VIRT_MEM_REGION_INPUT_BUFFER_IN,
    VIRT_MEM_REGION_INPUT_BUFFER_OUT,
    VIRT_MEM_REGION_OUTPUT_BUFFER_IN,
    VIRT_MEM_REGION_OUTPUT_BUFFER_OUT,
    VIRT_MEM_REGION_RPC_QUEUE,

    VIRT_MEM_REGION_PROTECTED,
    VIRT_MEM_REGION_OUT_BUF,
    VIRT_MEM_REGION_FIRMWARE1,
    VIRT_MEM_REGION_FIRMWARE2,
    VIRT_MEM_REGION_FIRMWARE3,
    VIRT_MEM_REGION_FIRMWARE4,
    VIRT_MEM_REGION_FIRMWARE5,
    VIRT_MEM_REGION_FIRMWARE6,
    VIRT_MEM_REGION_FIRMWARE7,
    VIRT_MEM_REGION_REGS,
    VIRT_MEM_REGION_COUNT /**< Region count, Not an actual memory region */
};

/**
 * This function is used to query the memory region for a certain region type.
 * @param type        The memory region type.
 * @param[out] region Pointer to the structure that will contain the memory
 *                    region information after the call has returned. If the
 *                    memory region doesn't exist, the start and end of the
 *                    region is set to 0xFFFFFFFF.
 */
void mve_mem_virt_region_get3(enum mve_mem_virt_region_type type,
                             struct mve_mem_virt_region *region);

/**
 * This function is used to query the region type for a certain mve address.
 * @param mve_addr    The mve address to check.
 * @return The memory region type. If mve_addr is not a valid address
 *         region count (VIRT_MEM_REGION_COUNT) is returned.
 */
enum mve_mem_virt_region_type mve_mem_virt_region_type_get3(mve_addr_t mve_addr);
#endif /* MVE_MEM_REGION_H */
