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

#ifndef MVE_MMU_H
#define MVE_MMU_H

#include "mve_rsrc_mem_frontend.h"
#include "mve_mem_region.h"
#include "mve_buffer.h"

/* The following code assumes 4 kB pages and that the MVE uses a 32-bit
 * virtual address space. */

#define MVE_MMU_PAGE_SHIFT 12
#define MVE_MMU_PAGE_SIZE (1 << MVE_MMU_PAGE_SHIFT)

#define MVE_MMU_L2_CHUNK_SHIFT 22
#define MVE_MMU_L2_CHUNK_SIZE (1 << MVE_MMU_L2_CHUNK_SHIFT)

/* Size of the MVE MMU page table entry in bytes */
#define MVE_MMU_PAGE_TABLE_ENTRY_SIZE 4

enum mve_mmu_attrib
{
    ATTRIB_PRIVATE = 0,
    ATTRIB_REFFRAME = 1,
    ATTRIB_SHARED_RO = 2,
    ATTRIB_SHARED_RW = 3
};

enum mve_mmu_access
{
    ACCESS_NO = 0,
    ACCESS_READ_ONLY = 1,
    ACCESS_EXECUTABLE = 2,
    ACCESS_READ_WRITE = 3
};

enum mve_mmu_alloc
{
    ALLOC_NORMAL = 0,
    ALLOC_EXTERNAL = 1
};

typedef uint32_t mve_mmu_entry_t;

#define MVE_MMU_PADDR_MASK 0x3FFFFFFCLLU
#define MVE_MMU_PADDR_SHIFT 2
#define MVE_MMU_ATTRIBUTE_MASK 0xC0000000LLU
#define MVE_MMU_ATTRIBUTE_SHIFT 30
#define MVE_MMU_ACCESS_MASK 0x3
#define MVE_MMU_ACCESS_SHIFT 0

/**
 * Extracts the physical address from an L1/L2 page table entry.
 * @param entry The L1/L2 page table entry.
 * @return The physical address stored in the page table entry.
 */
static inline phys_addr_t mve_mmu_entry_get_paddr(mve_mmu_entry_t entry)
{
    phys_addr_t pa_entry = (phys_addr_t)entry;
    return (((pa_entry & MVE_MMU_PADDR_MASK) >> MVE_MMU_PADDR_SHIFT) << MVE_MMU_PAGE_SHIFT);
}

/**
 * Extracts the L1/L2 page table entry attribute field.
 * @param entry The L1/L2 page table entry.
 * @return The attribute parameter stored in the page table entry.
 */
static inline enum mve_mmu_attrib mve_mmu_entry_get_attribute(mve_mmu_entry_t entry)
{
    return ((entry & MVE_MMU_ATTRIBUTE_MASK) >> MVE_MMU_ATTRIBUTE_SHIFT);
}

/**
 * Extracts the L1/L2 page table entry access field.
 * @param entry The L1/L2 page table entry.
 * @return The access parameter stored in the page table entry.
 */
static inline enum mve_mmu_access mve_mmu_entry_get_access(mve_mmu_entry_t entry)
{
    return ((entry & MVE_MMU_ACCESS_MASK) >> MVE_MMU_ACCESS_SHIFT);
}

/**
 * Calculates the physical address of the L1 page table entry that corresponds
 * to a given MVE address.
 * @param l1_page Physical address of the L1 page.
 * @param mve_addr MVE virtual address.
 * @return The physical address of the L1 page table entry.
 */
static inline phys_addr_t mve_mmu_l1_entry_addr_from_mve_addr(phys_addr_t l1_page,
                                                              mve_addr_t mve_addr)
{
    return l1_page + MVE_MMU_PAGE_TABLE_ENTRY_SIZE * (mve_addr >> MVE_MMU_L2_CHUNK_SHIFT);
}

/**
 * Calculates the L1 page entry index that corresponds
 * to a given MVE address.
 * @param l1_page Physical address of the L1 page.
 * @param mve_addr MVE virtual address.
 * @return The index of the L1 page table entry.
 */
static inline uint32_t mve_mmu_l1_entry_index_from_mve_addr(phys_addr_t l1_page,
                                                            mve_addr_t mve_addr)
{
    return (mve_addr >> MVE_MMU_L2_CHUNK_SHIFT);
}

/**
 * Calculates the physical address of the L2 page table entry that corresponds
 * to a given MVE address.
 * @param l2_page Physical address of the L2 page.
 * @param mve_addr MVE virtual address.
 * @return The physical address of the L2 page table entry.
 */
static inline phys_addr_t mve_mmu_l2_entry_addr_from_mve_addr(phys_addr_t l2_page,
                                                              mve_addr_t mve_addr)
{
    return l2_page + MVE_MMU_PAGE_TABLE_ENTRY_SIZE *
           ((mve_addr & (MVE_MMU_L2_CHUNK_SIZE - 1)) >> MVE_MMU_PAGE_SHIFT);
}

/**
 * Helper function for creating the MMU page table entry that shall be written
 * to the MMUCTRL register.
 * @param attrib       MMU page table entry attribute.
 * @param l1_page_addr Physical address of the L1 page.
 * @return The L0 page table entry.
 */
static inline mve_mmu_entry_t mve_mmu_make_l0_entry(enum mve_mmu_attrib attrib, phys_addr_t l1_page_addr)
{
    return ((((mve_mmu_entry_t)attrib) << MVE_MMU_ATTRIBUTE_SHIFT) & MVE_MMU_ATTRIBUTE_MASK) |
           ((mve_mmu_entry_t)((l1_page_addr >> (MVE_MMU_PAGE_SHIFT - MVE_MMU_PADDR_SHIFT)) & MVE_MMU_PADDR_MASK)) |
           ACCESS_READ_WRITE;
}

/**
 * Helper function for creating a MMU L1/L2 page table entry.
 * @param attrib       MMU page table entry attribute.
 * @param paddr        The physical address of the L2 page in case of a
 *                     L1 page table entry or the physical address of a allocated
 *                     page in case of a L2 page table entry.
 * @param access       MMU page table entry access attribute.
 * @return The L1 page table entry.
 */
static inline mve_mmu_entry_t mve_mmu_make_l1l2_entry(enum mve_mmu_attrib attrib,
                                                      phys_addr_t paddr,
                                                      enum mve_mmu_access access)
{
    return ((((mve_mmu_entry_t)attrib) << MVE_MMU_ATTRIBUTE_SHIFT) & MVE_MMU_ATTRIBUTE_MASK) |
           ((mve_mmu_entry_t)((paddr >> (MVE_MMU_PAGE_SHIFT - MVE_MMU_PADDR_SHIFT)) & MVE_MMU_PADDR_MASK)) |
           ((((mve_mmu_entry_t)access) << MVE_MMU_ACCESS_SHIFT) & MVE_MMU_ACCESS_MASK);
}

/**
 * Calculates the offset of the physical address from the page start.
 * @param addr The physical address.
 * @return The offset of the address from the page start.
 */
static inline uint32_t mve_mmu_get_offset_in_page(phys_addr_t addr)
{
    return (uint32_t)(addr & (MVE_MMU_PAGE_SIZE - 1));
}

struct mve_mmu_mapping; /* Forward declaration */

/**
 * Structure defining a MMU context.
 */
struct mve_mmu_ctx
{
    phys_addr_t l1_page;                 /**< Start address of the L1 page */
    enum mve_mmu_alloc *l1_l2_alloc;     /**< Allocation type of l2 page for each L1 entry */
    phys_addr_t reservation;             /**< Page used to mark reserved L2 entries */
    struct mve_mmu_mapping *mappings;    /**< Book keeping data on mapped memory */

#ifdef EMULATOR
    phys_addr_t mmu_id;                  /**< The emulator creates a MMU ID based on
                                          *   the l1_page when initialized. The ID is
                                          *   used as a context identifier. Store the
                                          *   ID in this member. */
#endif
};

/**
 * Create a MMU context.
 * @return The created MMU context struct.
 */
struct mve_mmu_ctx *mve_mmu_create_ctx3(void);

/**
 * Destroy a MMU context and free all referenced resources.
 * @param ctx The MMU context to destroy.
 */
void mve_mmu_destroy_ctx3(struct mve_mmu_ctx *ctx);

/**
 * Merge a preinitialized L2 page into the page table. Note that the client
 * is responsible for freeing the supplied l2_page after this function has
 * returned. The client is responsible for freeing the mapped pages when
 * they are no longer needed (i.e. after the unmap operation).
 * @param ctx       The MMU context.
 * @param l2_page   The preinitialized L2 page to merge into the page table.
 * @param mve_addr  Where in MVE virtual address space to merge the page.
 * @param num_pages Number of page entries to merge.
 * @return True on success, false on failure.
 */
bool mve_mmu_map_pages_merge3(struct mve_mmu_ctx *ctx,
                             phys_addr_t l2_page,
                             mve_addr_t mve_addr,
                             uint32_t num_pages);

/**
 * Create L1 page table using l2_page address provided by secure OS/client.
 * The client is responsible for freeing the mapped pages when
 * they are no longer needed (i.e. after the unmap operation).
 * @param ctx       The MMU context.
 * @param l2_page   The preinitialized L2 page to add into the page table.
 * @param mve_addr  Where in MVE virtual address space to add the page.
 * @return True on success, false on failure.
 */
bool mve_mmu_map_page_replace3(struct mve_mmu_ctx *ctx,
                              phys_addr_t l2_page,
                              mve_addr_t mve_addr);

/**
 * Maps an array of physical pages into MVE address space. No extra space
 * reservations are made which means that a future resize will most likely
 * fail. Use mve_mmu_map_pages_and_reserve in these cases. Note that mve_addr
 * must be page aligned or this function will fail. If free_pages_on_unmap is true,
 * the MMU module takes over as owner of both the array and the allocated pages.
 * The client shall in this case not free any of these resources.
 * @param ctx                 The MMU context.
 * @param pages               List of physical pages to map.
 * @param mve_addr            The start address in MVE virtual space where to map the pages.
 *                            Must be page aligned.
 * @param num_pages           Number of pages to map.
 * @param attrib              MMU attribute settings.
 * @param access              MMU access settings.
 * @param free_pages_on_unmap Whether the pages backing this mapping shall
 *                            be freed on unmap.
 * @return True on success, false on failure.
 */
bool mve_mmu_map_pages3(struct mve_mmu_ctx *ctx,
                       phys_addr_t *pages,
                       mve_addr_t mve_addr,
                       uint32_t num_pages,
                       uint32_t num_sys_pages,
                       enum mve_mmu_attrib attrib,
                       enum mve_mmu_access access,
                       bool free_pages_on_unmap);

/**
 * Remove MVE virtual address space mapping. This function also removes mapping
 * reservations. Note that this function does not free the unmapped pages unless
 * they were mapped with free_pages_on_unmap set to true.
 * @param ctx       The MMU context.
 * @param mve_addr  The start address in MVE virtual space of the mapping.
 */
void mve_mmu_unmap_pages3(struct mve_mmu_ctx *ctx,
                         mve_addr_t mve_addr);

/**
 * Maps a list of physical pages into MVE address space and reserves pages
 * for future resize operations. The physical pages will be mapped somewhere
 * in the region specified by the region parameter and the virtual start
 * address of the mapping is guaranteed to be aligned according to the alignment
 * parameter. Note that the region boundaries must be page aligned or the map
 * will fail. If free_pages_on_unmap is true, the MMU module takes over as owner
 * of both the array and the allocated pages. The client shall in this case not
 * free any of these resources.
 * @param[in] ctx                 The MMU context.
 * @param[in] pages               List of physical pages to map.
 * @param[in,out] region          Defining the region where the mapping must be
 *                                performed. Note that the region must be page
 *                                aligned. This parameter will contain the actual
 *                                mapping region if the call returns successfully.
 * @param[in] num_pages           Number of physical pages to map.
 * @param[in] max_pages           max_pages - num_pages is the number of pages to
 *                                reserve in order to support future resize operations.
 * @param[in] alignment           Aligntment of the mapping. The value must be a power of 2
 *                                or 0 which results in 1 byte alignment.
 * @param[in] attrib              MMU attribute settings.
 * @param[in] access              MMU access settings.
 * @param[in] free_pages_on_unmap Whether the pages backing this mapping shall be
 *                                freed on unmap.
 * @return True on success, false on failure. On success, region will contain
 *         the start and end addresses of the mapping (excluding reserved pages).
 */
bool mve_mmu_map_pages_and_reserve3(struct mve_mmu_ctx *ctx,
                                   phys_addr_t *pages,
                                   struct mve_mem_virt_region *region,
                                   uint32_t num_mve_pages,
                                   uint32_t max_mve_pages,
                                   uint32_t alignment,
                                   uint32_t num_sys_pages,
                                   enum mve_mmu_attrib attrib,
                                   enum mve_mmu_access access,
                                   bool free_pages_on_unmap);

/**
 * Return info of current mapping if it exist for given mve_addr
 * @param mve_addr            Address in MVE virtual address space to the first page of
 *                            the mapping.
 * @param num_pages           Fill with number of physical pages in the map.
 * @param max_pages           Fill with maximum number of physical pages this map has reserved.
 *
 * @return True on success, false on failure.
 */
bool mve_mmu_map_info3(struct mve_mmu_ctx *ctx,
                      mve_addr_t mve_addr,
                      uint32_t *num_pages,
                      uint32_t *max_pages);

/**
 * Resizes an existing mapping of physical pages in MVE address
 * space. New pages will be allocated as needed if new_pages is NULL. The resize
 * operation will fail if num_pages is larger than the value specified for max_pages
 * when the initial mapping was made. If num_pages is the same as the current mapping
 * then nothing will happen. You can therefore not use this function to switch
 * physical pages. This function does not alter the owner property
 * specified in the initial mapping operation. If a client resizes a mapping
 * with free_pages_on_unmap set to false, the client is responsible for freeing
 * the original pages and array after this function returns.
 * @param ctx                 The MMU context.
 * @param new_pages           The new pages
 * @param mve_addr            Address in MVE virtual address space to the first page of
 *                            the mapping.
 * @param num_pages           Number of physical pages to map.
 * @param attrib              MMU attribute settings.
 * @param access              MMU access settings.
 * @param extend              Extend an old mapping.
 *                            only valid if new_pages != NULL and map->type != FREE_ON_UNMAP
 * @return True on success, false on failure.
 */
bool mve_mmu_map_resize3(struct mve_mmu_ctx *ctx,
                        phys_addr_t *new_pages,
                        mve_addr_t mve_addr,
                        uint32_t num_pages,
                        enum mve_mmu_attrib attrib,
                        enum mve_mmu_access access,
                        bool extend);

/**
 * Returns the ID of the L1 page. This value shall be written to the video HW.
 * @param ctx The MMU context.
 * @return The ID of the L1 page.
 */
phys_addr_t mve_mmu_get_id3(struct mve_mmu_ctx *ctx);

/**
 * Returns the physical address of the L1 page.
 * @param ctx The MMU context.
 * @return The physical address of the L1 page or 0 if no page exists.
 */
phys_addr_t mve_mmu_get_l1_page3(struct mve_mmu_ctx *ctx);

/**
 * Copy data from a memory region pointed out by a MVE virtual address to
 * a CPU buffer.
 * @param ctx  The MMU context.
 * @param src  MVE virtual address of the source buffer.
 * @param dst  CPU pointer of the destination buffer.
 * @param size Number of bytes to copy.
 * @return True on success, false on failure.
 */
bool mve_mmu_read_buffer3(struct mve_mmu_ctx *ctx,
                         mve_addr_t src,
                         uint8_t *dst,
                         uint32_t size);

#endif /* MVE_MMU_H */
