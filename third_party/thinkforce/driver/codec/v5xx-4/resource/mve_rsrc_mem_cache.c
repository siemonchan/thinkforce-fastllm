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
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/sched.h>
#endif

#include "mve_rsrc_mem_frontend.h"
#include "mve_rsrc_mem_cache.h"
#include "mve_rsrc_log.h"

#define MAX_MEMORY_CACHE_SMALL      32
#define MAX_MEMORY_CACHE_MEDIUM     128
#define MAX_MEMORY_CACHE_LARGE      1024

static struct kmem_cache *mve_small_cachep;
static struct kmem_cache *mve_medium_cachep;
static struct kmem_cache *mve_large_cachep;

void mve_rsrc_mem_cache_init4(void)
{
    mve_small_cachep = kmem_cache_create_usercopy("mve_small_cache",
                                         MAX_MEMORY_CACHE_SMALL,
                                         0,
                                         SLAB_HWCACHE_ALIGN,
                                         0, 0,
                                         NULL);
    mve_medium_cachep = kmem_cache_create_usercopy("mve_medium_cache",
                                          MAX_MEMORY_CACHE_MEDIUM,
                                          0,
                                          SLAB_HWCACHE_ALIGN,
                                          0, 0,
                                          NULL);
    mve_large_cachep = kmem_cache_create_usercopy("mve_large_cache",
                                         MAX_MEMORY_CACHE_LARGE,
                                         0,
                                         SLAB_HWCACHE_ALIGN,
                                         0, 0,
                                         NULL);
}

void mve_rsrc_mem_cache_deinit4(void)
{
    if (NULL != mve_small_cachep)
    {
        kmem_cache_destroy(mve_small_cachep);
        mve_small_cachep = NULL;
    }
    if (NULL != mve_medium_cachep)
    {
        kmem_cache_destroy(mve_medium_cachep);
        mve_medium_cachep = NULL;
    }
    if (NULL != mve_large_cachep)
    {
        kmem_cache_destroy(mve_large_cachep);
        mve_large_cachep = NULL;
    }
}

static struct kmem_cache *mve_rsrc_get_cachep(uint32_t size)
{
    struct kmem_cache *cachep = NULL;

    if (size <= MAX_MEMORY_CACHE_SMALL)
    {
        cachep = mve_small_cachep;
    }
    else if (size <= MAX_MEMORY_CACHE_MEDIUM)
    {
        cachep = mve_medium_cachep;
    }
    else if (size <= MAX_MEMORY_CACHE_LARGE)
    {
        cachep = mve_large_cachep;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log4, MVE_LOG_ERROR, "No allocation slab found. size=%u.", size);
    }

    return cachep;
}

void *mve_rsrc_mem_cache_alloc4(uint32_t size, uint32_t flags)
{
    void *ptr;
    struct kmem_cache *cachep;

    cachep = mve_rsrc_get_cachep(size);

    if (NULL == cachep)
    {
        ptr = vmalloc(size);
    }
    else
    {
        ptr = kmem_cache_alloc(cachep, flags);
        if (ptr)
        {
            memset(ptr, 0, size);
        }
    }
    return ptr;
}

void mve_rsrc_mem_cache_free4(void *ptr, uint32_t size)
{
    struct kmem_cache *cachep;

    cachep = mve_rsrc_get_cachep(size);

    if (NULL == cachep)
    {
        vfree(ptr);
    }
    else
    {
        kmem_cache_free(cachep, ptr);
    }
}
EXPORT_SYMBOL(mve_rsrc_mem_cache_alloc4);
EXPORT_SYMBOL(mve_rsrc_mem_cache_free4);
