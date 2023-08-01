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
#include <asm/bug.h>
#endif

#include "mve_rsrc_register.h"

#include "mve_com_host_interface_v2.h"
#include "mve_com_host_interface_v3.h"

#include "mve_mem_region.h"

static uint32_t hw_version(void)
{
    static uint32_t version = 0;
    if (0 == version)
    {
        version = mver_reg_get_version6();
    }
    return version;
}

static const struct mve_mem_virt_region *get_mem_virt_regions(void)
{
    if (hw_version() >= MVE_V52_V76)
    {
        return mve_com_hi_v3_get_mem_virt_region6();
    }
    else
    {
        return mve_com_hi_v2_get_mem_virt_region6();
    }

    return NULL;
}

void mve_mem_virt_region_get6(enum mve_mem_virt_region_type type,
                             struct mve_mem_virt_region *region)
{
    if (NULL != region)
    {
        /* Apparently gcc implemented the enum as an unsigned, and disliked
         * what it considers to be an always-true comparision */
        if (/*VIRT_MEM_REGION_FIRMWARE0 <= type &&*/ VIRT_MEM_REGION_COUNT > type)
        {
            *region = get_mem_virt_regions()[type];
        }
        else
        {
            WARN_ON(true);
            region->start = 0xFFFFFFFF;
            region->end = 0xFFFFFFFF;
        }
    }
}

enum mve_mem_virt_region_type mve_mem_virt_region_type_get6(mve_addr_t mve_addr)
{
    enum mve_mem_virt_region_type i;
    const struct mve_mem_virt_region *regions = get_mem_virt_regions();
    for (i = VIRT_MEM_REGION_FIRST; i < VIRT_MEM_REGION_COUNT; i++)
    {
        const struct mve_mem_virt_region *region = &regions[i];
        if (mve_addr >= region->start &&
            mve_addr < region->end)
        {
            return i;
        }
    }
    return VIRT_MEM_REGION_COUNT;
}
