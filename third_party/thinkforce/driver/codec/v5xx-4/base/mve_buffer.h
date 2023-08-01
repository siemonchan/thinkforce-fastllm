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

#ifndef MVE_BUFFER_H
#define MVE_BUFFER_H

#ifndef __KERNEL__
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

#include "mve_base.h"
#include "mve_mem_region.h"

/* Invalid MVE handle */
#define MVE_HANDLE_INVALID -1

/**
 * This module defines the interface for registering user allocated memory
 * buffers with the MVE.
 */

/**
 * This enum lists the port indices
 */
enum mve_port_index
{
    MVE_PORT_INDEX_INPUT = 0,
    MVE_PORT_INDEX_OUTPUT = 1
};

enum mve_buffer_owner
{
    MVE_BUFFER_OWNER_CPU,
    MVE_BUFFER_OWNER_DEVICE
};

#define ROUND_UP(v, a) (((v) + (a) - 1) & ~((a) - 1))

/**
 * This structure contains all information that is needed to describe a userspace
 * allocated buffer.
 */
struct mve_buffer_info
{
    mve_base_buffer_handle_t buffer_id;       /**< Unique buffer ID */
    mve_base_buffer_handle_t handle;          /**< Handle to the external buffer */
    enum mve_base_buffer_allocator allocator; /**< Specifies which allocator was used to allocate
                                               *   the buffer */
    uint32_t size;                            /**< Size of the external buffer in bytes */
    uint32_t width;                           /**< Width of the buffer (only for pixel formats). */
    uint32_t height;                          /**< Height of the buffer (only for pixel formats). */
    uint32_t stride;                          /**< Stride of the buffer (only for pixel formats). */
    uint32_t max_height;                      /**< Maximum Height of the buffer (only for pixel formats). */
    uint32_t stride_alignment;                /**< Stride alignment in bytes (only for pixel formats). */
    enum mve_base_buffer_format format;       /**< Format of the buffer. */
    uint32_t offset;                          /**< If allocator is attachment, this member marks
                                               *   the offset of the first byte in the buffer */
    bool do_cache_maintenance;                /**< True if cache maintenance is needed for this buffer */

    uint32_t afbc_width_in_superblocks;       /**< Width of the AFBC buffer in superblocks (only for AFBC formats) */
    uint32_t afbc_alloc_bytes;                /**< AFBC frame size */
};

/**
 * This structure contains data necessary to describe a buffer mapping into MVE address space.
 */
struct mve_buffer_mapping
{
    struct mve_mem_virt_region region; /**< Defines the mapping in MVE virtual address space */

    phys_addr_t *pages;                /**< Array of physical pages */
    uint32_t num_pages;                /**< Number of pages in the array above */
    uint32_t num_mve_pages;            /**< Number of pages for MVE_MMU mapping */
    uint32_t write;                    /**< Pages to be mapped writable or read only */

    uint32_t offset_in_page;           /**< Offset in bytes to the start of the buffer. 0 if
                                        *   the allocation is page aligned. */
};

/**
 * This struct encapsulates information about a userspace allocated buffer
 */
struct mve_buffer_external
{
    struct mve_buffer_info info;       /**< External buffer info */
    struct mve_buffer_mapping mapping; /**< Mapping data */
};

/**
 * This structure stores information regarding external buffer mappings for one
 * input/output buffer. A client may allocate a buffer in userspace and
 * instruct the component to use the memory backing that buffer. To do that,
 * the driver must map the physical pages backing the memory buffer into the MVE
 * virtual address space. The client may at a later point chose to unregister the
 * buffer to prevent further MVE write operations to the buffer. This structure
 * stores all data to support these operations.
 */
struct mve_buffer_client
{
    struct mve_buffer_external *buffer;          /**< Buffer descriptor */
    struct mve_buffer_external *crc;             /**< CRC buffer descriptor */

    uint32_t filled_len;                         /**< Number of bytes worth of data in the buffer */
    uint32_t offset;                             /**< Offset from start of buffer to first byte */
    uint32_t flags;                              /**< Flags for omx use */
    uint32_t mve_flags;                          /**< MVE sideband information */
    uint32_t pic_index;                          /**< Picture index in decode order. Output from FW. */
    uint64_t timestamp;                          /**< Buffer timestamp. */

    uint32_t decoded_width;                      /**< Width of the decoded frame */
    uint32_t decoded_height;                     /**< Height of the decoded frame */

    uint16_t cropx;                              /**< Luma x crop */
    uint16_t cropy;                              /**< Luma y crop */
    uint8_t y_offset;                            /**< Deblocking y offset of picture */

    int mve_handle;                              /**< Firmware buffer identifier */
    int in_use;                                  /**< Tells whether this buffer is in
                                                  *   use by the MVE or not */
    enum mve_port_index port_index;              /**< Port index of this buffer */

    struct list_head register_list;              /**< Linked list register entry*/
    struct list_head quick_flush_list;           /**< Linked list quick flush entry */
};

struct mve_mmu_ctx;

/**
 * Validate the supplied buffer information.
 * @param info Buffer to validate.
 * @return True if the supplied buffer information is valid, false otherwise.
 */
bool mve_buffer_is_valid4(const struct mve_buffer_info *info);

/**
 * Allocate memory for private data and a mve_buffer_external instance. This is
 * also the place to e.g. import the external buffer etc.
 * @param info Buffer information.
 * @param port Port on which buffer is registered
 * @return Pointer to the mve_buffer_external part of the allocated structure. NULL
 *         if no such structure could be allocated.
 */
struct mve_buffer_external *mve_buffer_create_buffer_external4(struct mve_buffer_info *info, uint32_t port);

/**
 * Free the memory allocated in mve_buffer_create_buffer_external.
 * @param buffer Pointer to the mve_buffer_external instance to free.
 */
void mve_buffer_destroy_buffer_external4(struct mve_buffer_external *buffer);

/**
 * Constructs an array of physical pages backing the user allocated buffer and
 * stores the array in the supplied mve_buffer_external. Note that the pages
 * must be mapped in the MVE MMU table before MVE can access the buffer. This
 * is the responsibility of the client.
 * @param buffer The buffer to map.
 * @return True on success, false on failure.
 */
bool mve_buffer_map_physical_pages4(struct mve_buffer_external *buffer);

/**
 * Hand back the pages to the user allocated buffer allocator. Note that the
 * pages must also be unmapped from the MVE MMU table. This is the responsibility
 * of the client.
 * @param buffer The buffer to unmap.
 * @return True on success, false on failure. This function fails if the
 *         supplied buffer is currently not registered.
 */
bool mve_buffer_unmap_physical_pages4(struct mve_buffer_external *buffer);

/**
 * Set the owner of the buffer. This function takes care of cache flushing.
 * @param buffer The buffer to change ownership of.
 * @param owner  The new owner of the buffer.
 * @param port   The port this buffer is connected to (input/output).
 */
void mve_buffer_set_owner4(struct mve_buffer_external *buffer,
                          enum mve_buffer_owner owner,
                          enum mve_port_index port);

#endif /* MVE_BUFFER_H */
