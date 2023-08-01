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

#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_mem_backend.h"
#include "mve_rsrc_driver.h"
#include "mve_rsrc_log.h"

#ifdef EMULATOR
#include "emulator_userspace.h"
#else /* EMULATOR */
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <asm/cacheflush.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/hashtable.h>
#endif /* EMULATOR */

#if (1 == MVE_MEM_DBG_SUPPORT)

#ifndef PAGE_SIZE
#pragma message("define PAGE_SIZE to 4096")
//#define PAGE_SIZE 4096
#else
#pragma message("PAGE_SIZE has defined")
#endif

#if (1 == MVE_MEM_DBG_RESFAIL)
static bool mem_resfail_did_fail = false;
static uint32_t mem_resfail_threshold = 0;
static uint32_t mem_resfail_curr_alloc_nr = 0;
static bool mem_resfail_enabled = false;
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

#if (1 == MVE_MEM_DBG_TRACKMEM)
#ifdef EMULATOR
struct semaphore watchdog_sem6;
#endif /* EMULATOR */

/* Hashtable containing all non freed memory allocations */
static DEFINE_HASHTABLE(memory_entries, 8);
static uint32_t allocation_nr;

static uint32_t allocated_memory = 0;
static uint32_t peak_allocated_memory = 0;

/* Lock to to protect the memory allocations list */
static spinlock_t entries_lock;

enum memory_type {VIRTUAL, PHYSICAL};

struct memory_entry
{
    void *os_ptr;                       /**< Address of the allocated memory */
    uint32_t size;                      /**< Size of the allocation */

    enum memory_type mem_type;          /**< Type of allocation, either virtual
                                         *   or physical memory */
    unsigned int sequence;              /**< Allocation sequence */

    char const *allocated_from_func;    /**< String describing in which function
                                         *   the allocation was made */
    unsigned int allocated_from_line;   /**< At which line was the allocation made */

    struct hlist_node hash;             /**< Memory entry hash table */
};

/**
 * Creates a memory entry and adds it to the list of allocations.
 */
static struct memory_entry *create_memory_entry(const char *func_str, int line_nr)
{
    struct memory_entry *entry;

    entry = kzalloc(sizeof(struct memory_entry), GFP_KERNEL);
    if (NULL == entry)
    {
        return NULL;
    }

    entry->allocated_from_func = func_str;
    entry->allocated_from_line = line_nr;

    return entry;
}

static struct memory_entry *find_memory_entry(void *os_ptr)
{
    struct memory_entry *entry = NULL;
    struct memory_entry *ptr;

    if (NULL == os_ptr)
    {
        return NULL;
    }

    hash_for_each_possible(memory_entries, ptr, hash, (long)os_ptr)
    {
        if (os_ptr == ptr->os_ptr)
        {
            entry = ptr;
            break;
        }
    }

    if (entry == NULL)
    {
        MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Cannot find_memory_entry: %p", os_ptr);
    }

    return entry;
}

static void free_memory_entry(void *os_ptr)
{
    struct memory_entry *entry;

    entry = find_memory_entry(os_ptr);
    if (NULL != entry)
    {
        allocated_memory -= entry->size;

        hash_del(&entry->hash);
        kfree(entry);
    }
}
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

void *mve_rsrc_mem_zalloc_track6(uint32_t size,
                                uint32_t flags,
                                enum mve_rsrc_mem_allocator allocator,
                                const char *func_str,
                                int line_nr)
{
    void *ptr;

#if (1 == MVE_MEM_DBG_RESFAIL)
    if (false != mem_resfail_enabled &&
        mem_resfail_curr_alloc_nr >= mem_resfail_threshold)
    {
        /* Allocation failure simulated */
        mem_resfail_did_fail = true;
        __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
        return NULL;
    }

    __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

    if (ALLOCATOR_VMALLOC == allocator)
    {
        ptr = vmalloc(size);
    }
    else if (ALLOCATOR_CACHE == allocator)
    {
        ptr = mve_rsrc_mem_cache_alloc6(size, flags);
    }
    else
    {
        ptr = kzalloc(size, flags);
    }

#if (1 == MVE_MEM_DBG_TRACKMEM)
    if (NULL != ptr)
    {
        struct memory_entry *entry;

        entry = create_memory_entry(func_str, line_nr);

        spin_lock(&entries_lock);

        if (NULL != entry)
        {
            entry->os_ptr = ptr;
            entry->size = size;
            entry->mem_type = VIRTUAL;
            entry->sequence = allocation_nr++;

            hash_add(memory_entries, &entry->hash, (long)entry->os_ptr);

            allocated_memory += size;
            if (allocated_memory > peak_allocated_memory)
            {
                peak_allocated_memory = allocated_memory;
            }
        }

        spin_unlock(&entries_lock);
    }
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    return ptr;
}

void mve_rsrc_mem_free_track6(void *ptr,
                             uint32_t size,
                             enum mve_rsrc_mem_allocator allocator,
                             const char *func_str,
                             int line_nr)
{
#if (1 == MVE_MEM_DBG_TRACKMEM)
    spin_lock(&entries_lock);
    free_memory_entry(ptr);
    spin_unlock(&entries_lock);
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    if (ALLOCATOR_VMALLOC == allocator)
    {
        vfree(ptr);
    }
    else if (ALLOCATOR_CACHE == allocator)
    {
        mve_rsrc_mem_cache_free6(ptr, size);
    }
    else
    {
        kfree(ptr);
    }
}

phys_addr_t mve_rsrc_mem_alloc_page_track6(const char *func_str, int line_nr)
{
    phys_addr_t paddr;

#if (1 == MVE_MEM_DBG_RESFAIL)
    if (false != mem_resfail_enabled &&
        mem_resfail_curr_alloc_nr >= mem_resfail_threshold)
    {
        /* Allocation failure simulated */
        mem_resfail_did_fail = true;
        __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
        return 0;
    }

    __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

    paddr = mve_rsrc_mem_alloc_page6();

#if (1 == MVE_MEM_DBG_TRACKMEM)
    if (0 != paddr)
    {
        struct memory_entry *entry;

        entry = create_memory_entry(func_str, line_nr);

        spin_lock(&entries_lock);

        if (NULL != entry)
        {
            entry->os_ptr = (void *)paddr;
            entry->size = PAGE_SIZE;
            entry->mem_type = PHYSICAL;
            entry->sequence = allocation_nr++;

            hash_add(memory_entries, &entry->hash, (long)entry->os_ptr);

            allocated_memory += entry->size;
            if (allocated_memory > peak_allocated_memory)
            {
                peak_allocated_memory = allocated_memory;
            }
        }

        spin_unlock(&entries_lock);
    }
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    return paddr;
}

void mve_rsrc_mem_free_page_track6(phys_addr_t paddr, const char *func_str, int line_nr)
{
#if (1 == MVE_MEM_DBG_TRACKMEM)
    spin_lock(&entries_lock);
    free_memory_entry((void *)paddr);
    spin_unlock(&entries_lock);
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    mve_rsrc_mem_free_page6(paddr);
}

phys_addr_t *mve_rsrc_mem_alloc_pages_track6(uint32_t nr_pages,
                                            const char *func_str,
                                            int line_nr)
{
    phys_addr_t *paddrs;

#if (1 == MVE_MEM_DBG_RESFAIL)
    if (false != mem_resfail_enabled &&
        mem_resfail_curr_alloc_nr >= mem_resfail_threshold)
    {
        /* Allocation failure simulated */
        mem_resfail_did_fail = true;
        __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
        return NULL;
    }

    __sync_fetch_and_add(&mem_resfail_curr_alloc_nr, 1);
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

    paddrs = mve_rsrc_mem_alloc_pages6(nr_pages);

#if (1 == MVE_MEM_DBG_TRACKMEM)
    if (NULL != paddrs)
    {
        int i;
        struct memory_entry **entries;

        entries = kzalloc(sizeof(struct memory_entry *) * nr_pages, GFP_KERNEL);
        if (NULL == entries)
        {
            return paddrs;
        }

        for (i = 0; i < nr_pages; ++i)
        {
            entries[i] = create_memory_entry(func_str, line_nr);
        }

        spin_lock(&entries_lock);
        for (i = 0; i < nr_pages; ++i)
        {
            struct memory_entry *entry = entries[i];

            if (NULL != entry)
            {
                entry->os_ptr = (void *)paddrs[i];
                entry->size = PAGE_SIZE;
                entry->mem_type = PHYSICAL;
                entry->sequence = allocation_nr;

                hash_add(memory_entries, &entry->hash, (long)entry->os_ptr);

                allocated_memory += PAGE_SIZE;
            }
        }

        allocation_nr++;

        if (allocated_memory > peak_allocated_memory)
        {
            peak_allocated_memory = allocated_memory;
        }

        spin_unlock(&entries_lock);

        kfree(entries);
    }
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    return paddrs;
}

void mve_rsrc_mem_free_pages_track6(phys_addr_t *paddrs,
                                   uint32_t nr_pages,
                                   const char *func_str,
                                   int line_nr)
{
#if (1 == MVE_MEM_DBG_TRACKMEM)
    spin_lock(&entries_lock);
    if (NULL != paddrs)
    {
        int i;
        for (i = 0; i < nr_pages; ++i)
        {
            free_memory_entry((void *)paddrs[i]);
        }
    }
    spin_unlock(&entries_lock);
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

    mve_rsrc_mem_free_pages6(paddrs, nr_pages);
}

#if (1 == MVE_MEM_DBG_TRACKMEM)
struct print_entry
{
    uint32_t size;         /**< Size of the allocation */
    uint32_t nr;
    unsigned int sequence; /**< Allocation sequence */
    char const *func;      /**< String describing in which function
                            *   the allocation was made */
    unsigned int line;     /**< At which line was the allocation made */
    struct list_head list; /**< print entry list */
};

static struct print_entry *find_print_entry(struct list_head *entries, unsigned int sequence, bool *match)
{
    struct list_head *pos;

    list_for_each(pos, entries)
    {
        struct print_entry *pe = container_of(pos, struct print_entry, list);
        if (pe->sequence >= sequence)
        {
            if (pe->sequence == sequence)
            {
                *match = true;
            }
            return pe;
        }
    }
    return NULL;
}

static void register_print(struct list_head *entries, struct memory_entry *entry)
{
    struct print_entry *ptr;
    bool match = false;

    ptr = find_print_entry(entries, entry->sequence, &match);
    if (NULL != ptr && false != match)
    {
        ptr->nr++;
        return;
    }
    else if (ptr)
    {
        entries = &ptr->list;
    }

    ptr = kzalloc(sizeof(struct print_entry), GFP_ATOMIC);
    if (NULL != ptr)
    {
        INIT_LIST_HEAD(&ptr->list);
        ptr->size = entry->size;
        ptr->nr = 1;
        ptr->sequence = entry->sequence;
        ptr->func = entry->allocated_from_func;
        ptr->line = entry->allocated_from_line;
        list_add_tail(&ptr->list, entries);
    }
}

ssize_t mve_rsrc_mem_print_stack6(char *buf)
{
    struct memory_entry *entry;
    unsigned long bkt;
    ssize_t num = 0;
    struct list_head *pos;
    LIST_HEAD(print_entries);

    spin_lock(&entries_lock);

#ifdef EMULATOR
    down_interruptible(&watchdog_sem);
#endif /* EMULATOR */

    num += snprintf(buf + num, PAGE_SIZE - num, "Currently allocated memory (%d kB, peak: %d kB):\n",
                    allocated_memory / 1024, peak_allocated_memory / 1024);

    hash_for_each(memory_entries, bkt, entry, hash)
    {
        register_print(&print_entries, entry);
    }

    list_for_each_prev(pos, &print_entries)
    {
        struct print_entry *ptr = container_of(pos, struct print_entry, list);
        num += snprintf(buf + num, PAGE_SIZE - num - 1, "[%d] %s:%d (%d bytes)\n", ptr->sequence,
                        ptr->func,
                        ptr->line,
                        ptr->size * ptr->nr);
        if (num >= PAGE_SIZE)
        {
            buf[PAGE_SIZE - 2] = '\0';
            num = PAGE_SIZE - 1;
            break;
        }
    }

    while (!list_empty(&print_entries))
    {
        struct print_entry *ptr = list_first_entry(&print_entries, struct print_entry, list);
        list_del(&ptr->list);
        kfree(ptr);
    }

#ifdef EMULATOR
    up(&watchdog_sem);
#endif /* EMULATOR */
    spin_unlock(&entries_lock);

    return num;
}

void mve_rsrc_mem_clear_stack6(void)
{
    struct memory_entry *ptr;
    struct hlist_node *tmp;
    unsigned long bkt;

    spin_lock(&entries_lock);
    /* Clean up the book keeping structure */
    hash_for_each_safe(memory_entries, bkt, tmp, ptr, hash)
    {
        allocated_memory -= ptr->size;

        hash_del(&ptr->hash);
        kfree(ptr);
    }
    spin_unlock(&entries_lock);
}
#endif /* (1 == MVE_MEM_DBG_TRACKMEM)  */

EXPORT_SYMBOL(mve_rsrc_mem_zalloc_track6);
EXPORT_SYMBOL(mve_rsrc_mem_free_track6);
EXPORT_SYMBOL(mve_rsrc_mem_alloc_page_track6);
EXPORT_SYMBOL(mve_rsrc_mem_free_page_track6);
EXPORT_SYMBOL(mve_rsrc_mem_alloc_pages_track6);
EXPORT_SYMBOL(mve_rsrc_mem_free_pages_track6);

#if (1 == MVE_MEM_DBG_RESFAIL)
void mve_rsrc_mem_resfail_enable6(bool enable)
{
    mem_resfail_enabled = enable;

    mem_resfail_curr_alloc_nr = 0;
    mem_resfail_did_fail = false;
}

void mve_rsrc_mem_resfail_set_range6(uint32_t min, uint32_t max)
{
    mem_resfail_threshold = min;
    (void)max;

    mem_resfail_curr_alloc_nr = 0;
    mem_resfail_did_fail = false;
}

uint32_t mve_rsrc_mem_resfail_did_fail6(void)
{
    return mem_resfail_did_fail == false ? 0 : 1;
}
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

#ifdef CONFIG_SYSFS

#if (1 == MVE_MEM_DBG_TRACKMEM)
static ssize_t sysfs_print_memory_table(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
    return mve_rsrc_mem_print_stack6(buf);
}
#endif /* (1 == MVE_MEM_DBG_TRACKMEM) */

#if (1 == MVE_MEM_DBG_RESFAIL)
static ssize_t sysfs_read_mem_resfail(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", mem_resfail_did_fail == false ? 0 : 1);
}

ssize_t sysfs_write_mem_resfail6(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
    if (0 == strcmp("enable", buf))
    {
        mve_rsrc_mem_resfail_enable6(true);
    }
    else if (0 == strcmp("disable", buf))
    {
        mve_rsrc_mem_resfail_enable6(false);
    }
    else
    {
        int min, max;
        sscanf(buf, "%d %d", &min, &max);

        mve_rsrc_mem_resfail_set_range6(min, max);
    }

    return count;
}
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */

static struct device_attribute sysfs_files[] =
{
#if (1 == MVE_MEM_DBG_TRACKMEM)
    __ATTR(memory_table, S_IRUGO, sysfs_print_memory_table, NULL),
#endif /* (1 == MVE_MEM_DBG_TRACKMEM) */

#if (1 == MVE_MEM_DBG_RESFAIL)
    __ATTR(mem_resfail,  S_IRUGO, sysfs_read_mem_resfail, sysfs_write_mem_resfail)
#endif /* (1 == MVE_MEM_DBG_RESFAIL) */
};

#endif /* CONFIG_SYSFS */

#endif /* (1 == MVE_MEM_DBG_SUPPORT) */

void mve_rsrc_mem_init6(struct device *dev)
{
#if (1 == MVE_MEM_DBG_SUPPORT)

#if (1 == MVE_MEM_DBG_TRACKMEM)
    spin_lock_init(&entries_lock);
    hash_init(memory_entries);

#ifdef EMULATOR
    sema_init(&watchdog_sem, 1);
#endif /* EMULATOR */
#endif /* (1 == MVE_MEM_DBG_TRACKMEM) */

#ifdef CONFIG_SYSFS
    {
        int i;

        for (i = 0; i < NELEMS(sysfs_files); ++i)
        {
            int err = device_create_file(dev, &sysfs_files[i]);
            if (err < 0)
            {
                MVE_LOG_PRINT(&mve_rsrc_log, MVE_LOG_ERROR, "Unable to create sysfs file.");
            }
        }
    }
#endif /* CONFIG_SYSFS */

#endif /* (1 == MVE_MEM_DBG_SUPPORT) */

    mve_rsrc_mem_cache_init6();
}

void mve_rsrc_mem_deinit6(struct device *dev)
{
#if (1 == MVE_MEM_DBG_SUPPORT)
#ifndef EMULATOR
    int i;

    for (i = 0; i < NELEMS(sysfs_files); ++i)
    {
        device_remove_file(dev, &sysfs_files[i]);
    }
#else /* #ifndef EMULATOR */

#if (1 == MVE_MEM_DBG_TRACKMEM)
    if (0 < allocated_memory)
    {
        char buf[4096] = "";
        mve_rsrc_mem_print_stack6(buf);
        printk(KERN_ERR "%s\n", buf);
        mve_rsrc_mem_clear_stack6();
    }
    hash_deinit(memory_entries);
#endif /* (1 == MVE_MEM_DBG_TRACKMEM) */

#endif /* #ifndef EMULATOR */
#endif /* (1 == MVE_MEM_DBG_SUPPORT) */

    mve_rsrc_mem_cache_deinit6();
}
