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
#include "mveemul.h"
#include "emulator_userspace.h"
#else
#include <linux/slab.h>
#endif

#include "mve_mmu.h"
#include "mve_buffer.h"
#include "mve_rsrc_log.h"

#define NUM_L1_ENTRIES 1024
#define NUM_L2_ENTRIES 1024

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x) ((void)(x))
#endif

#ifdef _BullseyeCoverage
    #define BullseyeCoverageOff \
    # pragma BullseyeCoverage off
    #define BullseyeCoverageOn \
    # pragma BullseyeCoverage on
#else
    #define BullseyeCoverageOff
    #define BullseyeCoverageOn
#endif

enum mapping_type
{
    /** Normal page mapping. Do not free pages on unmap or when the MMU context
     *  is destroyed */
    NORMAL_ALLOCATION = 0,
    /** Used for external memory mapping, e.g. user-space memory. */
    EXTERNAL_MAPPING = 1,
    /** Free memory pages when the region is unmapped. */
    FREE_ON_UNMAP = 2,
};

/** @brief Descripe a mapping into the MVE MMU */
struct mve_mmu_mapping
{
    mve_addr_t mve_addr;     /**< MVE virtual address */
    uint32_t num_pages;      /**< Number of allocated pages */
    uint32_t max_pages;      /**< Size of the region reserved for this allocation */
    uint32_t type;           /**< Type of allocation */
    phys_addr_t *pages;      /**< Member is set if pages are to be deallocated upon unmap */
    uint32_t num_sys_pages;

    struct list_head list;   /**< These entries are stored in a linked list */
};

/**
 * Returns the physical address of the L2 line that corresponds to the
 * supplied address in MVE address space. This function allocates a L2 page
 * if one doesn't exist.
 * @param ctx The MMU context.
 * @param mve_addr Address in MVE address space.
 * @return The physical address of the L2 line that corresponds to the supplied address.
 *         Returns 0 in case of an error.
 */
static phys_addr_t get_l2_line_addr(struct mve_mmu_ctx *ctx, mve_addr_t mve_addr)
{
    phys_addr_t l1_entry_addr = mve_mmu_l1_entry_addr_from_mve_addr(ctx->l1_page, mve_addr);
    mve_mmu_entry_t l1_entry = (mve_mmu_entry_t)mve_rsrc_mem_read324(l1_entry_addr);
    phys_addr_t l2_page_addr;
    phys_addr_t l2_entry_addr;

    if (0 == l1_entry)
    {
        /* No L2 page has been created for this memory chunk */
        uint32_t l1_index;
        phys_addr_t l2_page = MVE_RSRC_MEM_ALLOC_PAGE();
        if (0 == l2_page)
        {
            return 0;
        }
        l1_index = mve_mmu_l1_entry_index_from_mve_addr(ctx->l1_page, mve_addr);

        ctx->l1_l2_alloc[l1_index] = ALLOC_NORMAL;
        l1_entry = mve_mmu_make_l1l2_entry(ATTRIB_PRIVATE, l2_page, ACCESS_READ_ONLY);

        mve_rsrc_mem_write324(l1_entry_addr, l1_entry);
        mve_rsrc_mem_clean_cache_range4(l1_entry_addr, sizeof(mve_mmu_entry_t));
    }

    l2_page_addr = mve_mmu_entry_get_paddr(l1_entry);
    l2_entry_addr = mve_mmu_l2_entry_addr_from_mve_addr(l2_page_addr, mve_addr);

    return l2_entry_addr;
}

struct mve_mmu_ctx *mve_mmu_create_ctx4(void)
{
    struct mve_mmu_ctx *ctx;

    ctx = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_mmu_ctx), GFP_KERNEL);
    if (NULL == ctx)
    {
        return NULL;
    }

    ctx->l1_page = MVE_RSRC_MEM_ALLOC_PAGE();
    if (0 == ctx->l1_page)
    {
        goto error;
    }

    ctx->l1_l2_alloc = MVE_RSRC_MEM_VALLOC(sizeof(enum mve_mmu_alloc) * NUM_L1_ENTRIES);
    if (NULL == ctx->l1_l2_alloc)
    {
        goto error;
    }
    memset(ctx->l1_l2_alloc, ALLOC_NORMAL, sizeof(enum mve_mmu_alloc) * NUM_L1_ENTRIES);

    /* This page will be used to mark reserved entries of the L2 pages. */
    ctx->reservation = MVE_RSRC_MEM_ALLOC_PAGE();
    if (0 == ctx->reservation)
    {
        goto error;
    }

    ctx->mappings = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_mmu_mapping), GFP_KERNEL);
    if (0 == ctx->mappings)
    {
        goto error;
    }

    INIT_LIST_HEAD(&ctx->mappings->list);

#if defined EMULATOR
    {
        int ret;

        ctx->mmu_id = ctx->l1_page;
        ret = mveemul_allocate_memory_map((unsigned int *)&ctx->mmu_id);
        if (MVEEMUL_RET_OK != ret)
        {
            goto error;
        }
    }
#endif

    return ctx;

error:
    if (0 != ctx->mappings)
    {
        MVE_RSRC_MEM_CACHE_FREE(ctx->mappings, sizeof(struct mve_mmu_mapping));
    }
    if (0 != ctx->reservation)
    {
        MVE_RSRC_MEM_FREE_PAGE(ctx->reservation);
    }
    if (0 != ctx->l1_l2_alloc)
    {
        MVE_RSRC_MEM_VFREE(ctx->l1_l2_alloc);
    }
    if (0 != ctx->l1_page)
    {
        MVE_RSRC_MEM_FREE_PAGE(ctx->l1_page);
    }
    if (NULL != ctx)
    {
        MVE_RSRC_MEM_CACHE_FREE(ctx, sizeof(struct mve_mmu_ctx));
    }

    return NULL;
}

#if defined EMULATOR
BullseyeCoverageOff
/**
 * Map a set of pages into the emulators memory space.
 * @param ctx      The MMU context.
 * @param mve_addr The start address of the mapping in the emulators memory space.
 * @param pages    An array of physical pages.
 * @param num_pages Number of pages to map.
 */
static void emul_map_memory(struct mve_mmu_ctx *ctx,
                            mve_addr_t mve_addr,
                            phys_addr_t *pages,
                            uint32_t num_pages)
{
    /* Go through the MMU table within the region defined by
     * [mve_addr:mve_addr + num_pages * MVE_MMU_PAGE_SIZE] and
     * register each consecutive memory chunk with mveemul. */
    mve_addr_t start, end, curr;

    start = mve_addr;
    curr = start;
    end = mve_addr + num_pages * MVE_MMU_PAGE_SIZE;

    CSTD_UNUSED(pages);

    while (curr < end)
    {
        phys_addr_t l2_line;
        mve_mmu_entry_t entry;

        /* Skip all initially empty pages */
        do
        {
            l2_line = get_l2_line_addr(ctx, curr);
            if (0 == l2_line)
            {
                /* Couldn't allocated a L2 page. This is an error! */
                return;
            }
            entry = mve_rsrc_mem_read324(l2_line);
            curr += MVE_MMU_PAGE_SIZE;
        }
        while (curr - MVE_MMU_PAGE_SIZE < end && 0 == entry);
        start = curr - MVE_MMU_PAGE_SIZE;

        /* Find the last page in the region or the first empty page */
        do
        {
            l2_line = get_l2_line_addr(ctx, curr);
            if (0 == l2_line)
            {
                /* Couldn't allocated a L2 page. This is an error! */
                return;
            }
            entry = mve_rsrc_mem_read324(l2_line);
            curr += MVE_MMU_PAGE_SIZE;
        }
        while (curr - MVE_MMU_PAGE_SIZE < end && 0 != entry);
        curr -= MVE_MMU_PAGE_SIZE;

        /* Add the chunk [start:curr] to mveemul */
        if (0 < curr - start)
        {
            while (start < curr)
            {
                mve_addr_t ptr = start + MVE_MMU_PAGE_SIZE;
                uint32_t i = 1;
                phys_addr_t addr;

                l2_line = get_l2_line_addr(ctx, start);
                if (0 == l2_line)
                {
                    return;
                }
                addr = mve_mmu_entry_get_paddr(mve_rsrc_mem_read324(l2_line));
                void *pstart = mve_rsrc_mem_cpu_map_page4(addr);
                void *pptr;

                do
                {
                    l2_line = get_l2_line_addr(ctx, ptr);
                    if (0 == l2_line)
                    {
                        return;
                    }
                    mve_mmu_entry_t entry = mve_rsrc_mem_read324(l2_line);
                    pptr = mve_rsrc_mem_cpu_map_page4(mve_mmu_entry_get_paddr(entry));
                    /* Unmap entry immediately. We only want to know if the pages are linear
                     * in the virtual address space. */
                    mve_rsrc_mem_cpu_unmap_page4(mve_mmu_entry_get_paddr(entry));

                    i++;
                    ptr += MVE_MMU_PAGE_SIZE;
                }
                while (pstart + (i - 1) * MVE_MMU_PAGE_SIZE == pptr && (ptr - MVE_MMU_PAGE_SIZE) < curr);
                /* Unmap pstart */
                mve_rsrc_mem_cpu_unmap_page4(addr);

                ptr -= MVE_MMU_PAGE_SIZE;

                l2_line = get_l2_line_addr(ctx, start);
                if (0 == l2_line)
                {
                    return;
                }
                addr = mve_mmu_entry_get_paddr(mve_rsrc_mem_read324(l2_line));
                int err = mveemul_add_memory(ctx->mmu_id,
                                             mve_rsrc_mem_cpu_map_page4(addr),
                                             start, ptr - start, ptr - start, false);
                if (err != MVEEMUL_RET_OK)
                {
                    /* Exit with a fatal error if mveemul_add_memory fails, as this
                     * indicates that the emulator is in a bad state. */
                    MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "mveemul_add_memory failed. error=%u.", err);
                }
                mve_rsrc_mem_cpu_unmap_page4(addr);
                start = ptr;
            }
        }
    }
}

/**
 * Unmap a set of pages from the emulators memory space.
 * @param ctx   The MMU context.
 * @param entry Structure containing information about the pages to unmap.
 */
static void emul_unmap_memory(struct mve_mmu_ctx *ctx,
                              struct mve_mmu_mapping *entry)
{
    /* Go through the MMU table within the region defined by
     * [mve_addr:mve_addr + entry->num_pages * MVE_MMU_PAGE_SIZE] and
     * remove each consecutive memory chunk from mveemul. */
    mve_addr_t start, end, curr;

    start = entry->mve_addr;
    curr = start;
    end = entry->mve_addr + entry->num_pages * MVE_MMU_PAGE_SIZE;

    while (curr < end)
    {
        mve_mmu_entry_t l2_entry;

        /* Skip all initial empty pages */
        do
        {
            phys_addr_t l2_line = get_l2_line_addr(ctx, curr);
            if (0 == l2_line)
            {
                return;
            }
            l2_entry = mve_rsrc_mem_read324(l2_line);
            curr += MVE_MMU_PAGE_SIZE;
        }
        while (curr - MVE_MMU_PAGE_SIZE < end && 0 == l2_entry);
        start = curr - MVE_MMU_PAGE_SIZE;

        /* Find the last page in the region or the first empty page */
        do
        {
            phys_addr_t l2_line = get_l2_line_addr(ctx, curr);
            if (0 == l2_line)
            {
                return;
            }
            l2_entry = mve_rsrc_mem_read324(l2_line);
            curr += MVE_MMU_PAGE_SIZE;
        }
        while (curr - MVE_MMU_PAGE_SIZE < end && 0 != l2_entry);
        curr -= MVE_MMU_PAGE_SIZE;

        /* Add the chunk [start:curr] to mveemul */
        if (0 < curr - start)
        {
            int err = mveemul_remove_memory(ctx->mmu_id, start);
            if (err != MVEEMUL_RET_OK)
            {
                /* Exit with a fatal error if mveemul_add_memory fails, as this
                 * indicates that the emulator is in a bad state. */
                MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "mveemul_remove_memory failed. error=%u.", err);
            }
        }
    }
}
BullseyeCoverageOn
#endif /* #if defined EMULATOR */

/**
 * Register a page mapping operation with the MMU context. This function saves
 * all information needed to manage mapped pages. E.g. to later be able to
 * unmap the pages. This function also registers the mapping with the emulator.
 * @param ctx       The MMU context.
 * @param mve_addr  Start address of the location in MVE address space where the pages are mapped.
 * @param pages     Array of pages to map.
 * @param num_pages Number of pages to map.
 * @param max_pages Maximum resize size.
 * @param type      Type of mapping.
 * @return True on success, false otherwise.
 */
static bool add_mapping(struct mve_mmu_ctx *ctx,
                        mve_addr_t mve_addr,
                        phys_addr_t *pages,
                        uint32_t num_pages,
                        uint32_t max_pages,
                        uint32_t num_sys_pages,
                        enum mapping_type type)
{
    struct mve_mmu_mapping *map;

    map = MVE_RSRC_MEM_CACHE_ALLOC(sizeof(struct mve_mmu_mapping), GFP_KERNEL);
    if (NULL == map)
    {
        return false;
    }

    map->mve_addr     = mve_addr;
    map->pages        = pages;
    map->num_pages    = num_pages;
    map->max_pages    = max_pages;
    map->num_sys_pages= num_sys_pages;
    map->type         = type;
    INIT_LIST_HEAD(&map->list);

    list_add(&map->list, &ctx->mappings->list);

#ifdef EMULATOR
    {
        emul_map_memory(ctx, mve_addr, pages, num_pages);
    }
#endif

    return true;
}

/**
 * Remove a mapping from the book keeping structure. Also frees the pages if
 * the mapping is of type FREE_ON_UNMAP.
 * @param ctx   The MMU context.
 * @param entry Structure containing information about the pages to unmap.
 */
static void free_mapping_entry(struct mve_mmu_ctx *ctx, struct mve_mmu_mapping *entry)
{
    list_del(&entry->list);

#ifdef EMULATOR
    {
        emul_unmap_memory(ctx, entry);
    }
#endif

    if (FREE_ON_UNMAP == entry->type)
    {
        MVE_RSRC_MEM_FREE_PAGES(entry->pages, entry->num_sys_pages);
        entry->pages = NULL;
    }
    MVE_RSRC_MEM_CACHE_FREE(entry, sizeof(struct mve_mmu_mapping));
}

/**
 * Remove a mapping from the book keeping structure. This function verifies
 * the existence of a page mapping at the supplied address and calls free_mapping_entry
 * to do the actual work.
 * @param ctx      The MMU context.
 * @param mve_addr Start address of the page mapping to unmap.
 * @return True on success, false otherwise.
 */
static bool remove_mapping(struct mve_mmu_ctx *ctx,
                           mve_addr_t mve_addr)
{
    struct list_head *pos;
    struct mve_mmu_mapping *map;

    list_for_each(pos, &(ctx->mappings->list))
    {
        map = container_of(pos, struct mve_mmu_mapping, list);

        if (mve_addr == map->mve_addr)
        {
            free_mapping_entry(ctx, map);
            return true;
        }
    }

    return false;
}

/**
 * Get the book keeping entry for the page mapping starting at the supplied address.
 * @param ctx      The MMU context.
 * @param mve_addr Start address of the page mapping.
 * @return The book keeping entry for the page mapping if one exists. NULL if
 *         none existing.
 */
static struct mve_mmu_mapping *get_mapping(struct mve_mmu_ctx *ctx,
                                           mve_addr_t mve_addr)
{
    struct list_head *pos;
    struct mve_mmu_mapping *map;

    list_for_each(pos, &(ctx->mappings->list))
    {
        map = container_of(pos, struct mve_mmu_mapping, list);

        if (mve_addr == map->mve_addr)
        {
            return map;
        }
    }

    return NULL;
}

void mve_mmu_destroy_ctx4(struct mve_mmu_ctx *ctx)
{
    int i;
    struct list_head *pos, *next;

    if (NULL == ctx)
    {
        return;
    }

    /* Clean up the book keeping structure */
    list_for_each_safe(pos, next, &(ctx->mappings->list))
    {
        struct mve_mmu_mapping *map = container_of(pos, struct mve_mmu_mapping, list);

#ifdef UNIT
        BullseyeCoverageOff
        if (NULL == map->pages && 0 < map->num_pages)
        {
            /* Only warn for regions that are not supposed to be deallocated
             * on unmap. */
            MVE_LOG_PRINT(&mve_rsrc_log4, MVE_LOG_ERROR, "MMU leaked region. addr=0x%X-0x%X.",
                          map->mve_addr,
                          map->mve_addr + map->num_pages * MVE_MMU_PAGE_SIZE);
        }
        BullseyeCoverageOn
#endif

        free_mapping_entry(ctx, map);
    }

    /* Free all MMU pages and unallocated memory */
    for (i = 0; i < NUM_L1_ENTRIES; ++i)
    {
        mve_mmu_entry_t l1_entry;
        /* Check if l2 page corresponding to l1 entry is Externally allocated */
        if (ctx->l1_l2_alloc[i] == ALLOC_EXTERNAL)
        {
            continue;
        }

        l1_entry = mve_rsrc_mem_read324(ctx->l1_page + i * sizeof(mve_mmu_entry_t));
        if (0 != l1_entry)
        {
            phys_addr_t l2_page = mve_mmu_entry_get_paddr(l1_entry);
            /* Free L2 page */
            MVE_RSRC_MEM_FREE_PAGE(l2_page);
        }
    }

#ifdef EMULATOR
    {
        int ret;

        ret = mveemul_remove_memory_map(ctx->mmu_id);
        if (MVEEMUL_RET_OK != ret)
        {
            MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Unable to remove memory map.");
        }
    }
#endif

    MVE_RSRC_MEM_CACHE_FREE(ctx->mappings, sizeof(struct mve_mmu_mapping));

    MVE_RSRC_MEM_FREE_PAGE(ctx->reservation);
    MVE_RSRC_MEM_FREE_PAGE(ctx->l1_page);
    MVE_RSRC_MEM_VFREE(ctx->l1_l2_alloc);
    MVE_RSRC_MEM_CACHE_FREE(ctx, sizeof(struct mve_mmu_ctx));
}

bool mve_mmu_map_page_replace4(struct mve_mmu_ctx *ctx,
                              phys_addr_t l2_page,
                              mve_addr_t mve_addr)
{
    phys_addr_t l1_entry_addr;
    mve_mmu_entry_t l1_entry;
    uint32_t l1_index;

    /* Sanity check of the arguments */
    if (NULL == ctx || 0 == l2_page)
    {
        return false;
    }

    l1_entry_addr = mve_mmu_l1_entry_addr_from_mve_addr(ctx->l1_page, mve_addr);
    l1_entry = mve_mmu_make_l1l2_entry(ATTRIB_PRIVATE, l2_page, ACCESS_READ_ONLY);
    l1_index = mve_mmu_l1_entry_index_from_mve_addr(ctx->l1_page, mve_addr);

    ctx->l1_l2_alloc[l1_index] = ALLOC_EXTERNAL;
    mve_rsrc_mem_write324(l1_entry_addr, l1_entry);
    mve_rsrc_mem_clean_cache_range4(l1_entry_addr, sizeof(mve_mmu_entry_t));

    (void)add_mapping(ctx, mve_addr, NULL, 1, 1, 1, NORMAL_ALLOCATION);

    return true;
}

bool mve_mmu_map_pages_merge4(struct mve_mmu_ctx *ctx,
                             phys_addr_t l2_page,
                             mve_addr_t mve_addr,
                             uint32_t num_pages)
{
    mve_addr_t start_addr, curr_addr, end_addr;
    unsigned int i;
    uint32_t offset;

    /* Sanity check of the arguments */
    if (NULL == ctx || 0 == l2_page || 0 == num_pages || NUM_L2_ENTRIES < num_pages)
    {
        return false;
    }

    offset = mve_mmu_get_offset_in_page(mve_addr);
    if (0 != offset)
    {
        /* Not page aligned */
        return false;
    }

    if (mve_addr + num_pages * MVE_MMU_PAGE_SIZE < mve_addr)
    {
        /* Trying to map past 0xFFFFFFFF */
        return false;
    }

    start_addr = mve_addr;
    end_addr = start_addr + num_pages * MVE_MMU_PAGE_SIZE;

    /* Make sure there is room at the designated place. */
    for (curr_addr = start_addr; curr_addr < end_addr; curr_addr += MVE_MMU_PAGE_SIZE)
    {
        phys_addr_t addr;
        mve_mmu_entry_t l2_entry;

        addr = get_l2_line_addr(ctx, curr_addr);
        if (0 == addr)
        {
            return false;
        }

        l2_entry = mve_rsrc_mem_read324(addr);
        if (0 != l2_entry)
        {
            /* Memory region is not free! */
            return false;
        }
    }

    /* Setup the MMU tables */
    for (i = 0; i < num_pages; ++i)
    {
        mve_mmu_entry_t entry;
        phys_addr_t addr = get_l2_line_addr(ctx, start_addr + i * MVE_MMU_PAGE_SIZE);
        /* No need to check addr here since it was verified in the for loop above */
        entry = mve_rsrc_mem_read324(l2_page + i * sizeof(mve_mmu_entry_t));
        mve_rsrc_mem_write324(addr, entry);
        mve_rsrc_mem_clean_cache_range4(addr, sizeof(mve_mmu_entry_t));
    }

    (void)add_mapping(ctx, mve_addr, NULL, num_pages, num_pages, num_pages, NORMAL_ALLOCATION);

    return true;
}

bool mve_mmu_map_pages4(struct mve_mmu_ctx *ctx,
                       phys_addr_t *pages,
                       mve_addr_t mve_addr,
                       uint32_t num_pages,
                       uint32_t num_sys_pages,
                       enum mve_mmu_attrib attrib,
                       enum mve_mmu_access access,
                       bool free_pages_on_unmap)
{
    unsigned int i;
    mve_addr_t start_addr, end_addr, curr_addr;
    uint32_t offset;

    if (NULL == ctx || NULL == pages)
    {
        return false;
    }

    offset = mve_mmu_get_offset_in_page(mve_addr);
    if (0 != offset)
    {
        /* Not page aligned */
        return false;
    }

    if ((0xFFFFFFFF - mve_addr) / MVE_MMU_PAGE_SIZE < num_pages)
    {
        /* Trying to map past 0xFFFFFFFF */
        return false;
    }

    start_addr = mve_addr;
    end_addr = mve_addr + num_pages * MVE_MMU_PAGE_SIZE;

    /* Make sure there is room at the designated place. */
    for (curr_addr = start_addr; curr_addr < end_addr; curr_addr += MVE_MMU_PAGE_SIZE)
    {
        phys_addr_t addr;
        mve_mmu_entry_t l2_entry;

        addr = get_l2_line_addr(ctx, curr_addr);
        if (0 == addr)
        {
            /* Failed to allocate L2 page */
            return false;
        }

        l2_entry = mve_rsrc_mem_read324(addr);
        if (0 != l2_entry)
        {
            /* Memory region is not free! */
            return false;
        }
    }

    /* Setup the MMU tables */
    for (i = 0; i < num_pages; ++i)
    {
        mve_mmu_entry_t entry = mve_mmu_make_l1l2_entry(attrib, pages[i], access);
        phys_addr_t addr = get_l2_line_addr(ctx, mve_addr + i * MVE_MMU_PAGE_SIZE);
        /* No need to check addr here since it was verified in the for loop above */
        mve_rsrc_mem_write324(addr, entry);
        mve_rsrc_mem_clean_cache_range4(addr, sizeof(mve_mmu_entry_t));
    }

    if (true == free_pages_on_unmap)
    {
        (void)add_mapping(ctx, mve_addr, pages, num_pages, num_pages, num_sys_pages, FREE_ON_UNMAP);
    }
    else
    {
        (void)add_mapping(ctx, mve_addr, NULL, num_pages, num_pages, num_sys_pages, NORMAL_ALLOCATION);
    }

    return true;
}

void mve_mmu_unmap_pages4(struct mve_mmu_ctx *ctx,
                         mve_addr_t mve_addr)
{
    uint32_t i;
    phys_addr_t addr, start;
    struct mve_mmu_mapping *map;
    uint32_t npages_remaining;
    uint32_t indx;
    uint32_t max_indx;
    uint32_t to_clear_current_iteration;

    if (NULL == ctx)
    {
        return;
    }

    /* Find the mapping in the book keeping structure. Unaligned unmaps and
     * other errors will be handled here. */
    map = get_mapping(ctx, mve_addr);
    if (NULL == map)
    {
        return;
    }

    npages_remaining = map->max_pages;

    /* Remove the mapping from the book keeping structure */
    (void)remove_mapping(ctx, mve_addr);

    /* Find address of the beginning of the map */
    do
    {
        /* Find the l2-entry for the mve virtual address */
        addr = get_l2_line_addr(ctx, mve_addr);
        /* No need to check addr here since get_mapping has verified that this
         * is a valid mapping. */

        /* This address must exist inside a mve l2page, which is page-aligned.
         * Each entry is of a known size, so we can get the index.
         */
        indx = (uint32_t)((addr & (MVE_MMU_PAGE_SIZE - 1)) / sizeof(mve_mmu_entry_t));
        /* And this is the max index available in an l2 page */
        max_indx = MVE_MMU_PAGE_SIZE / sizeof(mve_mmu_entry_t);

        /* Figure out how many we should clear from this l2 */
        to_clear_current_iteration = (max_indx - indx) < npages_remaining ? max_indx - indx : npages_remaining;

        /* Remember where we start so we can flush the cache later */
        start = addr;

        /* Clear the entries. */
        for (i = 0; i < to_clear_current_iteration; ++i)
        {
            mve_rsrc_mem_write324(addr, 0);
            addr += sizeof(mve_mmu_entry_t);
        }
        /* Flush the cache */
        mve_rsrc_mem_clean_cache_range4(start, to_clear_current_iteration * sizeof(mve_mmu_entry_t));

        /* Update how many pages that remain to unmap */
        npages_remaining -= to_clear_current_iteration;
        /* The next mve_addr to work with */
        mve_addr += to_clear_current_iteration * MVE_MMU_PAGE_SIZE;

        /* Loop around if not done */
    }
    while (npages_remaining > 0);
}

bool mve_mmu_map_pages_and_reserve4(struct mve_mmu_ctx *ctx,
                                   phys_addr_t *pages,
                                   struct mve_mem_virt_region *region,
                                   uint32_t num_pages,
                                   uint32_t max_pages,
                                   uint32_t alignment,
                                   uint32_t sys_pages,
                                   enum mve_mmu_attrib attrib,
                                   enum mve_mmu_access access,
                                   bool free_pages_on_unmap)
{
    mve_addr_t start_addr, end_addr, curr_addr;
    bool space_found;
    uint32_t i;
    uint32_t start_offset, end_offset;
    bool mapping_registered = false;
    bool ret = true;
    uint32_t mve_pages_in_sys = 1;

    if (NULL == ctx || NULL == region)
    {
        return false;
    }

    if (NULL == pages && 0 != num_pages)
    {
        return false;
    }

    /* Verify that the allocation is sane */
    if (0 == max_pages || max_pages < num_pages)
    {
        return false;
    }

    /* Verify that the region is valid */
    if (region->start > region->end || region->end - region->start == 0)
    {
        return false;
    }

    /* Verify that the region is big enough for the allocation */
    if (region->end - region->start < max_pages * MVE_MMU_PAGE_SIZE)
    {
        return false;
    }

    if (PAGE_SIZE != MVE_MMU_PAGE_SIZE)
    {
        if (PAGE_SIZE < MVE_MMU_PAGE_SIZE)
        {
            return false;
        }

        mve_pages_in_sys = PAGE_SIZE / MVE_MMU_PAGE_SIZE;

        if (PAGE_SIZE != mve_pages_in_sys * MVE_MMU_PAGE_SIZE)
        {
            return false;
        }
    }

    /* Verify that the region is page aligned */
    start_offset = mve_mmu_get_offset_in_page(region->start);
    end_offset = mve_mmu_get_offset_in_page(region->end);
    if (0 != start_offset ||
        0 != end_offset)
    {
        return false;
    }

    if (0 == alignment)
    {
        /* No alignment is the same as one byte alignment */
        alignment = 1;
    }

    start_addr = region->start;
    end_addr = region->end;

    curr_addr = (start_addr + alignment - 1) & ~(alignment - 1);
    /* Attempt to find a free chunk in MVE address space within the supplied region */
    do
    {
        space_found = true;
        start_addr = curr_addr;
        for (i = 0; i < max_pages && true == space_found && curr_addr < end_addr; ++i)
        {
            phys_addr_t addr = get_l2_line_addr(ctx, curr_addr);
            if (0 == addr)
            {
                /* No L2 page found/allocated => Out of memory! */
                return false;
            }
            else
            {
                space_found = mve_rsrc_mem_read324(addr) == 0;
            }

            curr_addr += MVE_MMU_PAGE_SIZE;
        }

        if (false == space_found)
        {
            /* We didn't find a region of consecutive free MMU entries.
             * Find the next gap! */
            curr_addr = (curr_addr + alignment - 1) & ~(alignment - 1);

            while (curr_addr < end_addr)
            {
                phys_addr_t addr = get_l2_line_addr(ctx, curr_addr);
                if (0 != addr)
                {
                    mve_mmu_entry_t entry = mve_rsrc_mem_read324(addr);
                    if (0 == entry)
                    {
                        break;
                    }
                }

                curr_addr = (curr_addr + MVE_MMU_PAGE_SIZE + alignment - 1) & ~(alignment - 1);
            }
        }
    }
    while (false == space_found && curr_addr < end_addr);

    if (false == space_found || i < max_pages)
    {
        /* No space found within the memory region. Allocation failed! */
        return false;
    }

    /* Region found */
    for (i = 0; i < max_pages; ++i)
    {
        mve_mmu_entry_t entry;
        phys_addr_t addr = get_l2_line_addr(ctx, start_addr + i * MVE_MMU_PAGE_SIZE);
        /* No need to check addr since the loop above has verified that there
         * are L2 pages for every L2 entry needed for this mapping */

        if (i < num_pages)
        {
            phys_addr_t paddr;
            paddr = *(pages + (i / mve_pages_in_sys)) + (i % mve_pages_in_sys) * MVE_MMU_PAGE_SIZE;

            /* Map this entry to a real page */
            entry = mve_mmu_make_l1l2_entry(attrib, paddr, access);
        }
        else
        {
            /* Map this entry to a reservation page */
            entry = mve_mmu_make_l1l2_entry(attrib, ctx->reservation, access);
        }

        mve_rsrc_mem_write324(addr, entry);
        mve_rsrc_mem_clean_cache_range4(addr, sizeof(mve_mmu_entry_t));
    }

    /* Return the mapped region */
    region->start = start_addr;
    region->end = region->start + num_pages * MVE_MMU_PAGE_SIZE;
    /* Add mapping to book keeping structure */
    if (true == free_pages_on_unmap)
    {
        mapping_registered = add_mapping(ctx, region->start, pages, num_pages,
                                         max_pages, sys_pages, FREE_ON_UNMAP);
    }
    else
    {
        mapping_registered = add_mapping(ctx, region->start, NULL, num_pages,
                                         max_pages, sys_pages, NORMAL_ALLOCATION);
    }

    if (false == mapping_registered)
    {
        /* Failed to register the memory mapping in the book keeping data
         * structure. Unmap the memory and return false. */
        for (i = 0; i < max_pages; ++i)
        {
            phys_addr_t addr = get_l2_line_addr(ctx, region->start + i * MVE_MMU_PAGE_SIZE);
            /* No need to check addr because the mapping has already been done
             * successfully and get_l2_line_addr will therefore not fail now */
            mve_rsrc_mem_write324(addr, 0);
            mve_rsrc_mem_clean_cache_range4(addr, sizeof(mve_mmu_entry_t));
        }

        ret = false;
    }

    return ret;
}

bool mve_mmu_map_info4(struct mve_mmu_ctx *ctx,
                      mve_addr_t mve_addr,
                      uint32_t *num_pages,
                      uint32_t *max_pages)
{
    struct mve_mmu_mapping *map;

    map = get_mapping(ctx, mve_addr);
    if (NULL == map)
    {
        return false;
    }

    if (NULL != num_pages)
    {
        *num_pages = map->num_pages;
    }
    if (NULL != max_pages)
    {
        *max_pages = map->max_pages;
    }
    return true;
}

#ifdef EMULATOR
BullseyeCoverageOff
bool mve_mmu_map_resize4(struct mve_mmu_ctx *ctx,
                        phys_addr_t *new_pages,
                        mve_addr_t mve_addr,
                        uint32_t num_pages,
                        enum mve_mmu_attrib attrib,
                        enum mve_mmu_access access,
                        bool extend)
{
    struct mve_mmu_mapping *map;

    CSTD_UNUSED(new_pages);
    CSTD_UNUSED(attrib);
    CSTD_UNUSED(access);
    CSTD_UNUSED(extend);

    if (NULL == ctx)
    {
        return false;
    }

    map = get_mapping(ctx, mve_addr);
    if (NULL == map)
    {
        return false;
    }

    if (num_pages > map->max_pages)
    {
        /* Trying to increase mapping past the maximum size */
        return false;
    }

    return true;
}
BullseyeCoverageOn
#else
bool mve_mmu_map_resize4(struct mve_mmu_ctx *ctx,
                        phys_addr_t *new_pages,
                        mve_addr_t mve_addr,
                        uint32_t num_pages,
                        enum mve_mmu_attrib attrib,
                        enum mve_mmu_access access,
                        bool extend)
{
    struct mve_mmu_mapping *map;
    mve_addr_t curr_addr;
    uint32_t i;
    uint32_t num_sys_pages;

    if (NULL == ctx ||
        0 == mve_addr)
    {
        return false;
    }

    if (NULL == new_pages &&
        false != extend)
    {
        return false;
    }

    map = get_mapping(ctx, mve_addr);
    if (NULL == map)
    {
        return false;
    }

    if (num_pages > map->max_pages)
    {
        /* Trying to increase mapping past the maximum size */
        return false;
    }

    WARN_ON(map->num_pages != map->num_sys_pages);
    if (map->num_pages != num_pages)
    {
        bool delete_old_page_array = false;
        int new_pages_start = 0;

        if (NULL == new_pages)
        {
            /* Reuse the old pages. Allocate/free pages as needed */
            if (map->num_pages > num_pages)
            {
                /* Area is shrinking */
                uint32_t i;

                /* We'll free unused pages so make sure this mapping is marked
                 * as FREE_ON_UNMAP */
                WARN_ON(map->type != FREE_ON_UNMAP);

                /* The difference is map->num_pages - num_pages. Delete the last
                 * ones in the pagelist */
                for (i = num_pages; i < map->num_pages; ++i)
                {
                    MVE_RSRC_MEM_FREE_PAGE(map->pages[i]);
                    map->pages[i] = 0;
                }

                new_pages = map->pages;
            }
            else if (map->num_pages < num_pages)
            {
                /* Area is increasing */
                uint32_t i;

                /* Warn if the array and pages are not marked for deallocation
                 * upon unmap since this will result in a memory leak. */
                WARN_ON(map->type != FREE_ON_UNMAP);

                /* Area is increasing, by num_pages - map->num_pages */
                new_pages = vmalloc(sizeof(phys_addr_t) * num_pages);

                /* Copy the old pages */
                memcpy(new_pages, map->pages, sizeof(phys_addr_t) * map->num_pages);

                for (i = map->num_pages; i < num_pages; ++i)
                {
                    new_pages[i] = MVE_RSRC_MEM_ALLOC_PAGE();
                }

                delete_old_page_array = true;
            }
        }
        else
        {
            if (FREE_ON_UNMAP == map->type)
            {
                /* Resize with a new page array and FREE_ON_UNMAP is not supported.
                 * The reason is that the code currently doesn't support detecting
                 * whether individual pages are reused or not. Currently there are
                 * no use cases for this so this feature is unimplemented for now. */
                WARN_ON(true);
                return false;
            }
        }

        curr_addr = mve_addr;
        if (false != extend)
        {
            new_pages_start = map->num_pages;
        }
        for (i = new_pages_start; i < map->max_pages; ++i)
        {
            if (i < min(map->num_pages, num_pages) &&
                map->pages[i] == new_pages[i])
            {
                /* This page is being reused */
                continue;
            }
            else if (i > max(map->num_pages, num_pages))
            {
                /* The rest of the entries will just be reservation pages. The
                 * MMU table has already been setup for these. */
                break;
            }
            else
            {
                phys_addr_t l2_line_addr = get_l2_line_addr(ctx, mve_addr + i * MVE_MMU_PAGE_SIZE);
                /* No need to check l2_line_addr because this address already
                 * contains an old entry referencing either a page or a reservation
                 * page */

                mve_mmu_entry_t entry = mve_rsrc_mem_read324(l2_line_addr);

                if (i < num_pages)
                {
                    /* Map this entry to a real page */
                    entry = mve_mmu_make_l1l2_entry(attrib, new_pages[i - new_pages_start], access);
                }
                else
                {
                    /* Map this entry to a reservation page */
                    entry = mve_mmu_make_l1l2_entry(attrib, ctx->reservation, access);
                }

                mve_rsrc_mem_write324(l2_line_addr, entry);
                mve_rsrc_mem_clean_cache_range4(l2_line_addr, sizeof(mve_mmu_entry_t));
            }
        }

        if (FREE_ON_UNMAP == map->type)
        {
            if (false != delete_old_page_array)
            {
                vfree(map->pages);
            }

            map->pages = new_pages;
        }
        map->num_pages = num_pages;
        map->num_sys_pages = num_pages;
    }

    return true;
}
#endif

phys_addr_t mve_mmu_get_id4(struct mve_mmu_ctx *ctx)
{
    if (NULL == ctx)
    {
        return 0;
    }

#ifdef EMULATOR
    return ctx->mmu_id;
#else
    return ctx->l1_page;
#endif
}

phys_addr_t mve_mmu_get_l1_page4(struct mve_mmu_ctx *ctx)
{
    if (NULL == ctx)
    {
        return 0;
    }

    return ctx->l1_page;
}

bool mve_mmu_read_buffer4(struct mve_mmu_ctx *ctx,
                         mve_addr_t src,
                         uint8_t *dst,
                         uint32_t size)
{
    while (size > 0)
    {
        void *ptr;
        uint32_t copy_size;
        mve_mmu_entry_t l2_entry;
        phys_addr_t paddr;
        phys_addr_t l2addr = get_l2_line_addr(ctx, src);
        if (0 == l2addr)
        {
            return false;
        }

        l2_entry = (mve_mmu_entry_t)mve_rsrc_mem_read324(l2addr);
        paddr = mve_mmu_entry_get_paddr(l2_entry);
        if (0 == paddr)
        {
            return false;
        }

        copy_size = min(size, MVE_MMU_PAGE_SIZE - (src & (MVE_MMU_PAGE_SIZE - 1)));

        mve_rsrc_mem_invalidate_cache_range4(paddr, copy_size);
        ptr = mve_rsrc_mem_cpu_map_page4(paddr);
        if (NULL != ptr)
        {
            memcpy(dst, ptr, copy_size);
            mve_rsrc_mem_cpu_unmap_page4(paddr);
        }

        src += copy_size;
        dst += copy_size;
        size -= copy_size;
    }

    return true;
}
