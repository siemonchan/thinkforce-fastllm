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
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#endif

#include "mve_fw.h"
#include "mve_driver.h"
#include "mve_mmu.h"
#include "mve_rsrc_log.h"
#include "mve_rsrc_log_ram.h"
#include "mve_rsrc_mem_frontend.h"

#define MAX_STRINGNAME_SIZE     128

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x) ((void)(x))
#endif

/**
 * Firmware binary header.
 */
struct fw_header
{
    /** The RASC instruction for a jump to the "real" firmware (so we
     *  always can start executing from the first address). */
    uint32_t rasc_jmp;

    /** Host interface protocol version. */
    uint8_t protocol_minor;
    uint8_t protocol_major;

    /** Reserved for future use. Always 0. */
    uint8_t reserved[2];

    /** Human readable codec information. */
    uint8_t info_string[56];

    /** Part number. */
    uint8_t part_number[8];

    /** SVN revision */
    uint8_t svn_revision[8];

    /** Firmware version. */
    uint8_t version_string[16];

    /** Length of the read-only part of the firmware. */
    uint32_t text_length;

    /** Start address for BSS segment.  This is always page-aligned. */
    uint32_t bss_start_address;

    /** How to allocate pages for BSS segment is specified by a bitmap.
     *  The encoding of this is as for the "mem_map" in the protocol,
     *  as if you call it with:
     *    mem_map(bss_start_address, bss_bitmap_size, bss_bitmap_size,
     *            bss_bitmap); */
    uint32_t bss_bitmap_size;
    /** How to allocate pages for BSS segment is specified by a bitmap.
     *  The encoding of this is as for the "mem_map" in the protocol,
     *  as if you call it with:
     *    mem_map(bss_start_address, bss_bitmap_size, bss_bitmap_size,
     *            bss_bitmap); */
    uint32_t bss_bitmap[16];

    /** Defines a region of shared pages */
    uint32_t master_rw_start_address;
    /** Defines a region of shared pages */
    uint32_t master_rw_size;
};

/**
 * Firmware container. It contains the actual firmware binary and other
 * data that make the firmware instantiation and removal easier. These
 * objects are reference counted. All data stored in this structure
 * can be shared between firmware instances.
 */
struct mve_firmware_descriptor
{
    uint32_t num_pages;                        /**< Size of the firmware binary */
    phys_addr_t *data;                         /**< Pointer to firmware binary pages */

    mve_mmu_entry_t *mmu_entries;              /**< Premade MMU entries for the firmware binary */
    uint32_t num_text_pages;                   /**< Number of firmware text pages */
    uint32_t num_shared_pages;                 /**< Number of firmware pages that can be shared */
    uint32_t num_bss_pages;                    /**< Number of firmware pages that cannot be shared */

    struct mve_base_fw_version fw_version;     /**< FW version */
    struct kref refcount;                      /**< Reference counter */
};

/**
 * Structure used to store firmware instance specific data. Much of the data in
 * this structure is specific to a certain instance.
 */
struct mve_fw_instance
{
    char *role;                            /**< role string */
    int ncores;                            /**< Number of cores used by this instance */
    bool secure;                           /**< Secure firmware instance, l2 page tables created in secure OS */
    struct mve_firmware_descriptor *desc;  /**< Pointer to the firmware descriptor */

    phys_addr_t *shared_pages;             /**< Array of shared BSS pages allocated for this instance */
    uint32_t used_shared_pages;            /**< Number of used shared BSS pages */

    phys_addr_t *bss_pages;                /**< Array of non-shared BSS pages allocated for this instance */
    uint32_t used_bss_pages;               /**< Number of used non-shared BSS pages */
    struct mve_base_fw_version fw_version; /**< FW version */
};

/**
 * Instances of this structure are used to represent cached firmware instances.
 */
struct firmware_cache_entry
{
    const char *role;                     /**< role string*/
    const char *filename;                 /**< Firmware filename */
    struct mve_firmware_descriptor *desc; /**< Pointer to the cached firmware descriptor. NULL
                                           *   if no such descriptor exists yet. */
};

/**
 * Preinitialized firmware cache. Maps roles to firmware binary files.
 */
static struct firmware_cache_entry firmware_cache[] =
{
    {
        "video_decoder.avc",   "h264dec.fwb", NULL
    },
    {
        "video_encoder.avc",   "h264enc.fwb", NULL
    },
    {
        "video_decoder.hevc",  "hevcdec.fwb", NULL
    },
    {
        "video_encoder.hevc",  "hevcenc.fwb", NULL
    },
    {
        "video_decoder.h264",  "h264dec.fwb", NULL
    },
    {
        "video_decoder.vp8",   "vp8dec.fwb", NULL
    },
    {
        "video_encoder.vp8",   "vp8enc.fwb", NULL
    },
    {
        "video_decoder.vp9",   "vp9dec.fwb", NULL
    },
    {
        "video_encoder.vp9",   "vp9enc.fwb", NULL
    },
    {
        "video_decoder.rv",    "rvdec.fwb", NULL
    },
    {
        "video_decoder.mpeg2", "mpeg2dec.fwb", NULL
    },
    {
        "video_decoder.mpeg4", "mpeg4dec.fwb", NULL
    },
    {
        "video_decoder.h263",  "mpeg4dec.fwb", NULL
    },
    {
        "video_decoder.vc1",   "vc1dec.fwb", NULL
    },
    {
        "video_encoder.jpeg",  "jpegenc.fwb", NULL
    },
    {
        "video_decoder.jpeg",  "jpegdec.fwb", NULL
    },
    {
        "video_decoder.avs",   "avsdec.fwb", NULL
    }
};

/* Mutex used to protect the firmware cache from concurrent access */
static struct semaphore fw_cache_mutex;

/**
 * Construct a page entry for the firmware L2 lookup table.
 * @param page The physical address of the page.
 * @param type Type of firmware page (text, bss, shared).
 * @return L2 page entry for the firmare lookup table.
 */
static mve_mmu_entry_t construct_fw_l2_entry(phys_addr_t page, uint32_t type)
{
    return (mve_mmu_entry_t)(((page >> (MVE_MMU_PAGE_SHIFT - FW_PHYSADDR_SHIFT)) & FW_PHYSADDR_MASK) |
                             (type & FW_PAGETYPE_MASK));
}

void mve_fw_init5(void)
{
    static int count = 0;

    if (0 == count)
    {
        sema_init(&fw_cache_mutex, 1);
        count = 1;
    }
}

/**
 * Loads a binary firmware and creates a firmware descriptor. The client
 * is responsible for freeing the descriptor memory when no longer needed.
 * @param filename Firmware binary filename.
 * @return A firmware descriptor on success, NULL on failure.
 */
static struct mve_firmware_descriptor *load_firmware(const char *filename)
{
    const struct firmware *fw = NULL;
    struct mve_firmware_descriptor *desc = NULL;
    uint32_t num_pages = 0;
    int ret;
    uint32_t bytes_left;
    int i;

    desc = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_firmware_descriptor), GFP_KERNEL);
    if (NULL == desc)
    {
        return NULL;
    }

    ret = request_firmware(&fw, filename, mve_device5);
    if (0 > ret)
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_ERROR, "Unable to load firmware file. file=%s.", filename);
        goto error;
    }

    /* Allocate memory for the firmware binary */
    num_pages = (fw->size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;
    desc->data = MVE_RSRC_MEM_ALLOC_PAGES(num_pages);
    if (NULL == desc->data)
    {
        goto error;
    }

    /* Copy the firmware to the physical pages */
    bytes_left = fw->size;
    i = 0;
    while (0 < bytes_left)
    {
        uint32_t size = min(bytes_left, (uint32_t)MVE_MMU_PAGE_SIZE);
        void *dst = mve_rsrc_mem_cpu_map_page5(desc->data[i]);

        if (NULL == dst)
        {
            goto error;
        }

        memcpy(dst, fw->data + i * MVE_MMU_PAGE_SIZE, size);
        mve_rsrc_mem_cpu_unmap_page5(desc->data[i]);
        mve_rsrc_mem_clean_cache_range5(desc->data[i], PAGE_SIZE);
        i++;
        bytes_left -= size;
    }

    desc->num_pages = num_pages;
    kref_init(&desc->refcount);

    release_firmware(fw);

    return desc;

error:
    if (NULL != desc)
    {
        if (NULL != desc->data)
        {
            MVE_RSRC_MEM_FREE_PAGES(desc->data, desc->num_pages);
            desc->data = NULL;
        }
        MVE_RSRC_MEM_FREE(desc);
    }
    if (NULL != fw)
    {
        release_firmware(fw);
    }

    return NULL;
}

/**
 * Parses the firmware binary header and allocates text pages. This function
 * also calculates data needed for creating BSS pages later in the firmware
 * loading process.
 * @param desc The firmware descriptor.
 * @return True on success, false on failure.
 */
static bool construct_fw_page_table(struct mve_firmware_descriptor *desc)
{
    uint32_t i, j;
    mve_mmu_entry_t *pages = NULL;
    uint32_t no_text_pages, no_shared_pages = 0, no_bss_pages = 0;
    struct fw_header *header;
    bool ret = false;

    /* The first section of the firmware is a struct fw_header entry */
    header = (struct fw_header *)mve_rsrc_mem_cpu_map_page5(desc->data[0]);
    if (NULL == header)
    {
        return false;
    }

    /* Verify that the firmware fits in one L2 MMU page (i.e. 4 Mbyte). This
     * is not a hardware but a software restriction. The firmwares are not
     * anticipated to be larger than 4 Mbyte. */
    if (header->bss_bitmap_size >= (MVE_MMU_PAGE_SIZE / sizeof(uint32_t)) ||
        ((header->bss_start_address >> MVE_MMU_PAGE_SHIFT) + header->bss_bitmap_size >=
         (MVE_MMU_PAGE_SIZE / sizeof(uint32_t))))
    {
        WARN_ON(true);
        goto out;
    }

    /* Allocate one L2 page table. It will be used to store references
     * to all text pages (shared with all instances). BSS pages will not be
     * allocated here but when the firmware is instantiated for a given session.
     * This is because BSS pages are session private pages. */
    pages = MVE_RSRC_MEM_ZALLOC(MVE_MMU_PAGE_SIZE, GFP_KERNEL);
    if (NULL == pages)
    {
        goto out;
    }

    /* Text pages can be shared between all cores and all sessions running the
     * same firmware. */
    no_text_pages = (header->text_length + MVE_MMU_PAGE_SIZE - 1) / MVE_MMU_PAGE_SIZE;

    /* Process text pages */
    for (i = 0; i < no_text_pages; ++i)
    {
        /* Do not use the first entry in the L2 lookup table. If the 1st page
         * was used, then the firmware would have to read address 0x0 (NULL)
         * to fetch the jump instruction to the start of the firmware. */
        pages[i + 1] = construct_fw_l2_entry(desc->data[i], FW_PAGETYPE_TEXT);
    }

    /* Process BSS pages */
    i = header->bss_start_address >> MVE_MMU_PAGE_SHIFT;
    for (j = 0; j < header->bss_bitmap_size; j++)
    {
        uint32_t word_idx = j >> 5;
        uint32_t bit_idx = j & 0x1f;
        uint32_t addr = i << MVE_MMU_PAGE_SHIFT;

        /* Mark this page as either a BSS page or a shared page */
        if (addr >= header->master_rw_start_address &&
            addr < header->master_rw_start_address + header->master_rw_size)
        {
            /* Shared pages can be shared between all cores running the same session. */
            pages[i] = construct_fw_l2_entry(0, FW_PAGETYPE_BSS_SHARED);
            no_shared_pages++;
        }
        else if ((header->bss_bitmap[word_idx] & (1 << bit_idx)) != 0)
        {
            /* Non-shared BSS pages. These pages need to be allocated for each core
             * and session. */
            pages[i] = construct_fw_l2_entry(0, FW_PAGETYPE_BSS);
            no_bss_pages++;
        }
        i++;
    }
    /* Fill in book keeping data that is needed when the firmware is to be instantiated */
    desc->mmu_entries      = pages;
    desc->num_text_pages   = no_text_pages;
    desc->num_shared_pages = no_shared_pages;
    desc->num_bss_pages    = no_bss_pages;

    desc->fw_version.major = header->protocol_major;
    desc->fw_version.minor = header->protocol_minor;

    if (MVE_BASE_FW_MAJOR_PROT_V2 == desc->fw_version.major)
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Detected firmware version v2. major=%u, minor=%u.", desc->fw_version.major, desc->fw_version.minor);
    }
    else if (MVE_BASE_FW_MAJOR_PROT_V3 == desc->fw_version.major)
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_INFO, "Detected firmware version v3. major=%u, minor=%u.", desc->fw_version.major, desc->fw_version.minor);
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_ERROR, "Unknown firmware version. major=%u, minor=%u.", desc->fw_version.major, desc->fw_version.minor);
        desc->fw_version.major = MVE_BASE_FW_MAJOR_PROT_UNKNOWN;
    }
    ret = true;

out:
    mve_rsrc_mem_cpu_unmap_page5(desc->data[0]);

    return ret;
}

/**
 * Release function for a mve_firmware_descriptor. Called by kref_put
 * when the reference counter reaches zero.
 * @param ref Pointer to the kref member of the mve_firmware_descriptor structure.
 */
static void release_fw_descriptor(struct kref *ref)
{
    unsigned int i;

    struct mve_firmware_descriptor *desc = container_of(ref,
                                                        struct mve_firmware_descriptor,
                                                        refcount);

    /* Remove the firmware descriptor from the cache */
    for (i = 0; i < NELEMS(firmware_cache); ++i)
    {
        if (desc == firmware_cache[i].desc)
        {
            struct firmware_cache_entry *entry = &firmware_cache[i];
            entry->desc = NULL;
            break;
        }
    }

    MVE_RSRC_MEM_FREE_PAGES(desc->data, desc->num_pages);
    desc->data = NULL;

    MVE_RSRC_MEM_FREE(desc->mmu_entries);
    MVE_RSRC_MEM_FREE(desc);
}

/**
 * Get the firmware descriptor for a given role. The corresponding
 * firmware is loaded if it's not present in the firmware cache.
 * @param role A role string.
 * @return The firmware descriptor on success, NULL on failure.
 */
static struct mve_firmware_descriptor *get_fw_descriptor(const char *role)
{
    unsigned int i;
    struct firmware_cache_entry *entry = NULL;
    struct mve_firmware_descriptor *ret = NULL;

    /* Find which firmware to load based on the role */
    for (i = 0; i < NELEMS(firmware_cache); ++i)
    {
        if (0 == strncmp(role, firmware_cache[i].role, strlen(firmware_cache[i].role)))
        {
            entry = &firmware_cache[i];
            break;
        }
    }

    if (NULL == entry)
    {
        /* Unsupported role */
        goto out;
    }

    if (NULL == entry->desc)
    {
        bool ret;

        /* This firmware has not been loaded previously */
        entry->desc = load_firmware(entry->filename);
        if (NULL == entry->desc)
        {
            goto out;
        }

        /* Create all text pages and mark bss pages */
        ret = construct_fw_page_table(entry->desc);
        if (false == ret)
        {
            kref_put(&entry->desc->refcount, release_fw_descriptor);
        }
    }
    else
    {
        /* Firmware has already been loaded. Prevent it from being unloaded
         * by increasing the reference counter. */
        kref_get(&entry->desc->refcount);
    }

    ret = entry->desc;
out:
    return ret;
}

/**
 * Create and map a consecutive chunk of bss pages.
 * @param idx         Index in the reference page list of the starting position
 * @param mmu_entries Pointer to a list that maps out the different FW pages. It
 *                    tells e.g. where the FW expects BSS pages.
 * @param dst_page    The destination L2 MMU page
 * @param first_core  Indicating whether the function builds L2 entries for the first core
 * @param inst        The firmware instance
 * @return Index of the next page that isn't part of the consecutive BSS page chunk.
 */
static int add_bss_range(int idx,
                         const mve_mmu_entry_t *mmu_entries,
                         phys_addr_t dst_page,
                         bool first_core,
                         struct mve_fw_instance *inst)
{
    unsigned int i, j;
    unsigned int first_bss_idx, last_bss_idx, nof_bss_pages;
    phys_addr_t *bss_pages = NULL;

    mve_mmu_entry_t ref_line = mmu_entries[idx];
    uint32_t page_type = FW_GET_TYPE(ref_line);
    if ((page_type != FW_PAGETYPE_BSS) &&
        (page_type != FW_PAGETYPE_BSS_SHARED))
    {
        /* This is not a BSS page. */
        return idx;
    }

    /* Count number of consecutive pages of same type */
    first_bss_idx = idx;
    for (i = idx + 1; FW_GET_TYPE(mmu_entries[i]) == page_type; i++)
    {
        /* Do nothing. */
    }
    last_bss_idx = i - 1;
    nof_bss_pages = last_bss_idx - first_bss_idx + 1;

    if (true == first_core || FW_PAGETYPE_BSS == page_type)
    {
        /* Find free pages from the preallocated pages list */
        if (FW_PAGETYPE_BSS_SHARED == page_type)
        {
            bss_pages = &inst->shared_pages[inst->used_shared_pages];
            inst->used_shared_pages += nof_bss_pages;
        }
        else
        {
            bss_pages = &inst->bss_pages[inst->used_bss_pages];
            inst->used_bss_pages += nof_bss_pages;
        }

        for (j = first_bss_idx; j <= last_bss_idx; j++)
        {
            phys_addr_t page = bss_pages[j - first_bss_idx];
            mve_mmu_entry_t entry;

            entry = mve_mmu_make_l1l2_entry(ATTRIB_PRIVATE, page, ACCESS_READ_WRITE);
            mve_rsrc_mem_write325(dst_page + MVE_MMU_PAGE_TABLE_ENTRY_SIZE * j, entry);
        }
    }

    return i;
}

/**
 * Loads a firmware binary and sets up the session's MMU table.
 * @param ctx The session's MMU context.
 * @param role Role String. This is used to decide which firmware to load.
 * @param ncores Number of video cores this session will use.
 * @return A firmware instance on success, NULL on failure.
 */
static struct mve_fw_instance *mve_fw_load_insecure(struct mve_mmu_ctx *ctx,
                                                    const char *role,
                                                    int ncores)
{
    unsigned int i;
    struct mve_firmware_descriptor *desc = NULL;
    struct mve_fw_instance *inst = NULL;
    bool first_core = true;
    phys_addr_t l2page;
    unsigned int ncores_setup = 0;
    int sem_taken = -1;


    enum mve_mem_virt_region_type fw_regions[] =
    {
        VIRT_MEM_REGION_FIRMWARE0,
        VIRT_MEM_REGION_FIRMWARE1,
        VIRT_MEM_REGION_FIRMWARE2,
        VIRT_MEM_REGION_FIRMWARE3,
        VIRT_MEM_REGION_FIRMWARE4,
        VIRT_MEM_REGION_FIRMWARE5,
        VIRT_MEM_REGION_FIRMWARE6,
        VIRT_MEM_REGION_FIRMWARE7,
    };

    if (NULL == ctx || NULL == role)
    {
        return NULL;
    }

    /* Allocate L2 MMU page */
    l2page = MVE_RSRC_MEM_ALLOC_PAGE();
    if (0 == l2page)
    {
        return NULL;
    }

    sem_taken = down_interruptible(&fw_cache_mutex);
    if (0 != sem_taken)
    {
        goto error;
    }

    desc = get_fw_descriptor(role);
    if (NULL == desc)
    {
        /* Unable to load firmware */
        goto error;
    }

    inst = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_fw_instance), GFP_KERNEL);
    if (NULL == inst)
    {
        goto error;
    }

    inst->fw_version = desc->fw_version;

    inst->role = MVE_RSRC_MEM_ZALLOC(sizeof(unsigned char) * MAX_STRINGNAME_SIZE, GFP_KERNEL);
    if (NULL == inst->role)
    {
        goto error;
    }
    strncpy(inst->role, role, MAX_STRINGNAME_SIZE);

    /* Allocate BSS pages. Since shared BSS pages can be shared between all cores
     * used in a session, only desc->num_shared_pages are needed. However,
     * non-shared BSS pages are core private which means that desc->num_bss_pages * ncores
     * are needed. */
    inst->shared_pages = MVE_RSRC_MEM_ALLOC_PAGES(desc->num_shared_pages);
    inst->bss_pages = MVE_RSRC_MEM_ALLOC_PAGES(desc->num_bss_pages * ncores);

    if (NULL == inst->shared_pages || NULL == inst->bss_pages)
    {
        goto error;
    }

    /* Setup the MMU table for each core */
    for (i = 0; i < (unsigned int)ncores; ++i)
    {
        struct mve_mem_virt_region region;
        bool ret;
        unsigned int j;

        mve_mem_virt_region_get5(fw_regions[i], &region);

        /* No need to process text pages again since we reuse the previous L2 page */
        if (true == first_core)
        {
            /* Add text pages */
            for (j = 0; j < (MVE_MMU_PAGE_SIZE / sizeof(mve_mmu_entry_t)); j++)
            {
                mve_mmu_entry_t fw_mmu_page_line = desc->mmu_entries[j];
                if (FW_GET_TYPE(fw_mmu_page_line) == FW_PAGETYPE_TEXT)
                {
                    phys_addr_t paddr = FW_GET_PHADDR(fw_mmu_page_line);
                    mve_mmu_entry_t entry = mve_mmu_make_l1l2_entry(ATTRIB_PRIVATE, paddr, ACCESS_EXECUTABLE);
                    mve_rsrc_mem_write325(l2page + 4 * j, entry);
                }
            }
        }

        /* Add BSS pages */
        for (j = 0; j < (MVE_MMU_PAGE_SIZE / sizeof(mve_mmu_entry_t)); j++)
        {
            j = add_bss_range(j, desc->mmu_entries, l2page, first_core, inst);
            if ((unsigned int)-1 == j)
            {
                goto error;
            }
        }

        /* Merge L2 page to MMU table */
        ret = mve_mmu_map_pages_merge5(ctx,
                                      l2page,
                                      region.start,
                                      MVE_MMU_PAGE_SIZE / sizeof(uint32_t));
        if (false == ret)
        {
            goto error;
        }

        /* When the first core has been setup, do not process text pages
         * for the other cores since the text pages can be reused. */
        first_core = false;
        ncores_setup++;
    }

    MVE_RSRC_MEM_FREE_PAGE(l2page);

    inst->ncores = ncores;
    inst->desc = desc;

    up(&fw_cache_mutex);

    return inst;

error:
    if (0 != l2page)
    {
        MVE_RSRC_MEM_FREE_PAGE(l2page);
    }
    if (NULL != desc)
    {
        if (NULL != inst)
        {
            MVE_RSRC_MEM_FREE_PAGES(inst->shared_pages, desc->num_shared_pages);
            MVE_RSRC_MEM_FREE_PAGES(inst->bss_pages, desc->num_bss_pages * ncores);

            inst->shared_pages = NULL;
            inst->bss_pages = NULL;

            if (NULL != inst->role)
            {
                MVE_RSRC_MEM_FREE(inst->role);
            }
            MVE_RSRC_MEM_FREE(inst);
        }

        /* Unmapp the firmware from all cores that has already been setup */
        for (i = 0; i < ncores_setup; ++i)
        {
            struct mve_mem_virt_region region;

            mve_mem_virt_region_get5(fw_regions[i], &region);
            mve_mmu_unmap_pages5(ctx, region.start);
        }

        kref_put(&desc->refcount, release_fw_descriptor);
    }

    up(&fw_cache_mutex);

    return NULL;
}

/**
 * Sets up the session's MMU table from precreated l2pages from Secure OS.
 * @param ctx The session's MMU context.
 * @param fw_secure_desc Contains fw version and l2 page tables address for secure sessions.
 * @param ncores Number of video cores this session will use.
 * @return A firmware instance on success, NULL on failure.
 */
static struct mve_fw_instance *mve_fw_load_secure(struct mve_mmu_ctx *ctx,
                                                  struct mve_base_fw_secure_descriptor *fw_secure_desc,
                                                  int ncores)
{
    struct mve_fw_instance *inst = NULL;
    uint32_t i;
    phys_addr_t l2page;
    phys_addr_t l2pages = fw_secure_desc->l2pages;

    enum mve_mem_virt_region_type fw_regions[] =
    {
        VIRT_MEM_REGION_FIRMWARE0,
        VIRT_MEM_REGION_FIRMWARE1,
        VIRT_MEM_REGION_FIRMWARE2,
        VIRT_MEM_REGION_FIRMWARE3,
        VIRT_MEM_REGION_FIRMWARE4,
        VIRT_MEM_REGION_FIRMWARE5,
        VIRT_MEM_REGION_FIRMWARE6,
        VIRT_MEM_REGION_FIRMWARE7,
    };

    if (NULL == ctx)
    {
        return NULL;
    }

    inst = MVE_RSRC_MEM_ZALLOC(sizeof(struct mve_fw_instance), GFP_KERNEL);
    if (NULL == inst)
    {
        goto error;
    }
    inst->secure = true;
    inst->fw_version = fw_secure_desc->fw_version;

    for (i = 0; i < (unsigned int)ncores; ++i)
    {
        bool ret;
        struct mve_mem_virt_region region;
        mve_mem_virt_region_get5(fw_regions[i], &region);

        l2page = l2pages + (i * MVE_MMU_PAGE_SIZE);

        ret = mve_mmu_map_page_replace5(ctx,
                                       l2page,
                                       region.start);
        if (false == ret)
        {
            goto error;
        }
    }
    inst->ncores = ncores;
    return inst;

error:
    if (NULL != inst)
    {
        MVE_RSRC_MEM_FREE(inst);
    }
    return NULL;
}

/**
 * Entry point for loading a firmware binary and setting up the session's MMU table.
 * @param ctx The session's MMU context.
 * @param fw_secure_desc Contains fw version and l2 page tables address for secure sessions.
 * @param role Role String. This is used to decide which firmware to load.
 * @param ncores Number of video cores this session will use.
 * @return A firmware instance on success, NULL on failure.
 */
struct mve_fw_instance *mve_fw_load5(struct mve_mmu_ctx *ctx,
                                    struct mve_base_fw_secure_descriptor *fw_secure_desc,
                                    const char *role,
                                    int ncores)
{
    if (NULL == fw_secure_desc)
    {
        return mve_fw_load_insecure(ctx, role, ncores);
    }
    else
    {
        return mve_fw_load_secure(ctx, fw_secure_desc, ncores);
    }
}

bool mve_fw_unload5(struct mve_mmu_ctx *ctx, struct mve_fw_instance *inst)
{
    int i;
    int sem_taken = -1;

    enum mve_mem_virt_region_type fw_regions[] =
    {
        VIRT_MEM_REGION_FIRMWARE0,
        VIRT_MEM_REGION_FIRMWARE1,
        VIRT_MEM_REGION_FIRMWARE2,
        VIRT_MEM_REGION_FIRMWARE3,
        VIRT_MEM_REGION_FIRMWARE4,
        VIRT_MEM_REGION_FIRMWARE5,
        VIRT_MEM_REGION_FIRMWARE6,
        VIRT_MEM_REGION_FIRMWARE7,
    };

    if (NULL == ctx || NULL == inst)
    {
        return false;
    }

    if (false == inst->secure)
    {
        sem_taken = down_interruptible(&fw_cache_mutex);
        /* Continue even in the case the semaphore was not successfully taken */

        /* Unmap all firmware instances */
        for (i = 0; i < inst->ncores; ++i)
        {
            struct mve_mem_virt_region region;

            mve_mem_virt_region_get5(fw_regions[i], &region);
            mve_mmu_unmap_pages5(ctx, region.start);
        }
        /* The text pages will be removed when the firmware descriptor is
         * freed in release_fw_descriptor. */
        MVE_RSRC_MEM_FREE_PAGES(inst->shared_pages, inst->desc->num_shared_pages);
        MVE_RSRC_MEM_FREE_PAGES(inst->bss_pages, inst->desc->num_bss_pages * inst->ncores);
        MVE_RSRC_MEM_FREE(inst->role);

        inst->shared_pages = NULL;
        inst->bss_pages = NULL;
        inst->role = NULL;

        kref_put(&inst->desc->refcount, release_fw_descriptor);
    }

    MVE_RSRC_MEM_FREE(inst);

    if (0 == sem_taken)
    {
        up(&fw_cache_mutex);
    }

    return true;
}

struct mve_base_fw_version *mve_fw_get_version5(struct mve_fw_instance *inst)
{
    if (NULL == inst)
    {
        return NULL;
    }

    return &inst->fw_version;
}

bool mve_fw_secure5(struct mve_fw_instance *inst)
{
    if (NULL == inst)
    {
        return false;
    }

    return inst->secure;
}

void mve_fw_log_fw_binary5(struct mve_fw_instance *inst, struct mve_session *session)
{
    struct fw_header *header;
    struct
    {
        struct mve_log_header header;
        struct mve_log_fw_binary fw_binary;
    }
    message;
    struct iovec vec[2];
    struct timespec64 timespec;

    if (false != inst->secure)
    {
        return;
    }

    /* Map first page containing the firmware binary header. */
    header = (struct fw_header *)mve_rsrc_mem_cpu_map_page5(inst->desc->data[0]);
    if (NULL == header)
    {
        return;
    }

    //getnstimeofday(&timespec);
    ktime_get_ts64(&timespec);

    message.header.magic = MVE_LOG_MAGIC;
    message.header.length = 0;
    message.header.type = MVE_LOG_TYPE_FW_BINARY;
    message.header.severity = MVE_LOG_INFO;
    message.header.timestamp.sec = timespec.tv_sec;
    message.header.timestamp.nsec = timespec.tv_nsec;

    message.fw_binary.length = sizeof(*header);
    message.fw_binary.session = (uintptr_t)session;

    vec[0].iov_base = &message;
    vec[0].iov_len = sizeof(message);

    vec[1].iov_base = header;
    vec[1].iov_len = sizeof(*header);

    message.header.length = sizeof(message.fw_binary) + sizeof(*header);

    MVE_LOG_DATA(&mve_rsrc_log_fwif5, MVE_LOG_WARNING, vec, 2);

    /* Unmap first page. */
    mve_rsrc_mem_cpu_unmap_page5(inst->desc->data[0]);
}

#ifdef UNIT

void mve_fw_debug_get_info5(struct mve_fw_instance *inst,
                           mve_mmu_entry_t **mmu_entries,
                           uint32_t *no_text_pages,
                           uint32_t *no_shared_pages,
                           uint32_t *no_bss_pages)
{
    struct mve_firmware_descriptor *desc;
    int sem_taken;

    if (NULL == inst)
    {
        return;
    }

    sem_taken = down_interruptible(&fw_cache_mutex);
    if (0 != sem_taken)
    {
        return;
    }

    desc = get_fw_descriptor(inst->role);
    if (NULL == desc)
    {
        *mmu_entries = NULL;
        *no_text_pages = 0;
        *no_shared_pages = 0;
        *no_bss_pages = 0;

        up(&fw_cache_mutex);
        return;
    }

    *mmu_entries = desc->mmu_entries;
    *no_text_pages = desc->num_text_pages;
    *no_shared_pages = desc->num_shared_pages;
    *no_bss_pages = desc->num_bss_pages;

    kref_put(&desc->refcount, release_fw_descriptor);
    up(&fw_cache_mutex);
}

#endif
