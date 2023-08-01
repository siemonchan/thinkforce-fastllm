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

#include "mve_rsrc_mem_backend.h"

#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <asm/cacheflush.h>
#include <linux/export.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "mve_rsrc_driver.h"
#include "mve_rsrc_log.h"

#ifdef CONFIG_64BIT
#define PAGE_MASK_INT 0xFFFFFFFFFFFFF000ULL
#else
#define PAGE_MASK_INT 0xFFFFF000
#endif

phys_addr_t mve_rsrc_mem_alloc_page5(void)
{
    struct page *new_page;
    dma_addr_t dma_handle;

    /* Allocate a page that has a kernel logical address (low memory). These
     * pages always have a virtual address and doesn't need to be mapped */
    new_page = alloc_page(GFP_KERNEL | __GFP_ZERO | __GFP_NORETRY /*| __GFP_COLD*/);
    if (NULL == new_page)
    {
        goto error;
    }

    /* dma_map_page ensures that any data held in the cache is discarded or written back */
    dma_handle = dma_map_page(&mve_rsrc_data5.pdev->dev, new_page, 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(&mve_rsrc_data5.pdev->dev, dma_handle))
    {
        MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_ERROR, "mve_rsrc_mem_alloc_page5: dma_map_page failed.");
        goto error;
    }
    return (phys_addr_t)dma_handle;

error:
    if (NULL != new_page)
    {
        __free_page(new_page);
    }

    return 0;
}

void mve_rsrc_mem_free_page5(phys_addr_t addr)
{
    struct page *unmap_page;
    unmap_page = pfn_to_page(addr >> PAGE_SHIFT);
    dma_unmap_page(&mve_rsrc_data5.pdev->dev, addr, PAGE_SIZE, DMA_BIDIRECTIONAL);
    __free_page(unmap_page);
}

void mve_rsrc_mem_free_pages5(phys_addr_t *addrs, uint32_t num_pages)
{
    int i;

    if (NULL == addrs || 0 == num_pages)
    {
        return;
    }

    for (i = num_pages - 1; i >= 0; i--)
    {
        mve_rsrc_mem_free_page5(addrs[i]);
    }

    vfree(addrs);
}

void *mve_rsrc_mem_cpu_map_page5(phys_addr_t addr)
{
    phys_addr_t page_off = addr & ~PAGE_MASK_INT;
    struct page *p = pfn_to_page(addr >> PAGE_SHIFT);
    void *page_ptr = kmap(p);

    return (((uint8_t *)page_ptr) + page_off);
}

void mve_rsrc_mem_cpu_unmap_page5(phys_addr_t addr)
{
    struct page *p = pfn_to_page(addr >> PAGE_SHIFT);
    kunmap(p);
}

phys_addr_t *mve_rsrc_mem_alloc_pages5(uint32_t nr_pages)
{
    int i;
    phys_addr_t *pages;

    if (0 == nr_pages)
    {
        return NULL;
    }

    pages = vmalloc(sizeof(phys_addr_t) * nr_pages);
    if (NULL == pages)
    {
        return NULL;
    }

    for (i = 0; i < nr_pages; ++i)
    {
        pages[i] = mve_rsrc_mem_alloc_page5();

        if (0 == pages[i])
        {
            /* Page allocation failed. Free all allocated pages and return NULL */
            mve_rsrc_mem_free_pages5(pages, i);
            return NULL;
        }
    }

    return pages;
}

phys_addr_t *mve_rsrc_mem_map_virt_to_phys5(void *ptr, uint32_t size, uint32_t write)
{
    struct page **phys_pages = NULL;
    phys_addr_t *pages = NULL;
    int i;
    int get_npages = 0;
    uint32_t nr_pages;
    uintptr_t virt_addr;

    if (NULL == ptr || 0 == size)
    {
        return NULL;
    }

    nr_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    pages = vmalloc(sizeof(phys_addr_t) * nr_pages);
    if (NULL == pages)
    {
        goto error;
    }

    virt_addr = (uintptr_t)ptr;
    if (0 != virt_addr - (virt_addr & PAGE_MASK_INT))
    {
        goto error;
    }

    phys_pages = vmalloc(nr_pages * sizeof(struct page *));
    if (NULL == phys_pages)
    {
        goto error;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    down_read(&current->mm->mmap_lock);
#else
    down_read(&current->mm->mmap_sem);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    get_npages = get_user_pages(virt_addr, nr_pages, write != 0 ? FOLL_WRITE : FOLL_GET, phys_pages, NULL);
#else
    get_npages = get_user_pages(current, current->mm, virt_addr, nr_pages, write, 0, phys_pages, NULL);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    up_read(&current->mm->mmap_lock);
#else
    up_read(&current->mm->mmap_sem);
#endif

    if (get_npages != nr_pages)
    {
        goto error;
    }

    for (i = 0; i < nr_pages; ++i)
    {
        /* Flush cache and retreive physical address */
        dma_addr_t dma_handle;
        dma_handle = dma_map_page(&mve_rsrc_data5.pdev->dev, phys_pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
        if (dma_mapping_error(&mve_rsrc_data5.pdev->dev, dma_handle))
        {
            printk("\tmve_rsrc_mem_map_virt_to_phys i %d: dma_map_page failed.\n", i);
            MVE_LOG_PRINT(&mve_rsrc_log5, MVE_LOG_ERROR, "mve_rsrc_mem_map_virt_to_phys: dma_map_page failed.");
            dma_handle = (dma_addr_t)NULL;
        }
        pages[i] = (phys_addr_t)dma_handle;
    }

    vfree(phys_pages);
    return pages;

error:
    for (i = get_npages - 1; i >= 0; --i)
    {
        put_page(phys_pages[i]);
    }
    if (NULL != phys_pages)
    {
        vfree(phys_pages);
    }
    if (NULL != pages)
    {
        vfree(pages);
    }

    return NULL;
}

void mve_rsrc_mem_unmap_virt_to_phys5(phys_addr_t *pages, uint32_t nr_pages)
{
    int i;

    if (NULL == pages || 0 == nr_pages)
    {
        return;
    }

    for (i = nr_pages - 1; i >= 0; --i)
    {
        struct page *p = pfn_to_page(pages[i] >> PAGE_SHIFT);

        dma_unmap_page(&mve_rsrc_data5.pdev->dev, pages[i], PAGE_SIZE, DMA_BIDIRECTIONAL);
        set_page_dirty_lock(p);
        put_page(p);
    }

    vfree(pages);
}

uint32_t mve_rsrc_mem_read325(phys_addr_t addr)
{
    phys_addr_t offset = addr & ~PAGE_MASK_INT;
    struct page *p = pfn_to_page(addr >> PAGE_SHIFT);
    void *ptr = kmap(p);
    uint32_t ret;

    ret = *((uint32_t *)(((uint8_t *)ptr) + offset));
    kunmap(p);

    return ret;
}

void mve_rsrc_mem_write325(phys_addr_t addr, uint32_t value)
{
    phys_addr_t offset = addr & ~PAGE_MASK_INT;
    struct page *p = pfn_to_page(addr >> PAGE_SHIFT);
    void *ptr = kmap(p);

    *((uint32_t *)(((uint8_t *)ptr) + offset)) = value;
    kunmap(p);
}

void mve_rsrc_mem_clean_cache_range5(phys_addr_t addr, uint32_t size)
{
    dma_sync_single_for_device(&mve_rsrc_data5.pdev->dev, addr, size, DMA_TO_DEVICE);
}

void mve_rsrc_mem_invalidate_cache_range5(phys_addr_t addr, uint32_t size)
{
    dma_sync_single_for_device(&mve_rsrc_data5.pdev->dev, addr, size, DMA_FROM_DEVICE);
}

void mve_rsrc_mem_flush_write_buffer5(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
    dsb(sy);
#else
    dsb();
#endif
}

EXPORT_SYMBOL(mve_rsrc_mem_alloc_page5);
EXPORT_SYMBOL(mve_rsrc_mem_free_page5);
EXPORT_SYMBOL(mve_rsrc_mem_free_pages5);
EXPORT_SYMBOL(mve_rsrc_mem_cpu_map_page5);
EXPORT_SYMBOL(mve_rsrc_mem_cpu_unmap_page5);
EXPORT_SYMBOL(mve_rsrc_mem_alloc_pages5);
EXPORT_SYMBOL(mve_rsrc_mem_map_virt_to_phys5);
EXPORT_SYMBOL(mve_rsrc_mem_unmap_virt_to_phys5);
EXPORT_SYMBOL(mve_rsrc_mem_read325);
EXPORT_SYMBOL(mve_rsrc_mem_write325);
EXPORT_SYMBOL(mve_rsrc_mem_clean_cache_range5);
EXPORT_SYMBOL(mve_rsrc_mem_invalidate_cache_range5);
EXPORT_SYMBOL(mve_rsrc_mem_flush_write_buffer5);
