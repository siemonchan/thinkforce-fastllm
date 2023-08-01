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

#ifndef MVE_FW_H
#define MVE_FW_H

#include "mve_mmu.h"

struct mve_fw_instance;
struct mve_session;

#define FW_PAGETYPE_INVALID    0  /* no page */
#define FW_PAGETYPE_TEXT       1  /* TEXT page, shared between sessions. Page is allocated */
#define FW_PAGETYPE_BSS        2  /* BSS  page, separate per core in same session */
#define FW_PAGETYPE_BSS_SHARED 3  /* BSS  page, shared across cores in same session */

#define FW_PHYSADDR_MASK 0xfffffffc
#define FW_PHYSADDR_SHIFT 2
#define FW_PAGETYPE_MASK 0x3
#define FW_PAGETYPE_SHIFT 0

/* Get page type from FW MMU page entry */
#define FW_GET_TYPE(line)    ((line) & FW_PAGETYPE_MASK)
/* Get page address from FW MMU page entry */
#define FW_GET_PHADDR(line)  ((((phys_addr_t)line) & FW_PHYSADDR_MASK) << (MVE_MMU_PAGE_SHIFT - FW_PHYSADDR_SHIFT))

/**
 * Initializes the firmware cache subsystem. This function must be called
 * before any firmware binaries can be loaded.
 */
void mve_fw_init3(void);

/**
 * Loads the firmware necessary to support the supplied role. Maps the firmware into
 * the correct regions of the MMU table.
 * @param ctx            The MMU context.
 * @param fw_secure_desc Pre-created l2 pagetables for secure video playback
 * @param role           The role the firmware must support.
 * @param ncores         Number of firmware instances that shall be mapped into the MVE address space.
 * @return Firmware identifier on success, NULL on failure.
 */
struct mve_fw_instance *mve_fw_load3(struct mve_mmu_ctx *ctx,
                                    struct mve_base_fw_secure_descriptor *fw_secure_desc,
                                    const char *role,
                                    int ncores);

/**
 * Unload firmware, unmap the firmware pages from the MMU tables and free
 * all unused resources. The firmware is removed from the cache if no other
 * running session uses this firmware.
 * @param ctx The MMU context.
 * @param inst The firmware instance to unload.
 * @return True on success, false if an error occurred and no firmware was unloaded.
 */
bool mve_fw_unload3(struct mve_mmu_ctx *ctx, struct mve_fw_instance *inst);

/**
 * Returns the host interface protocol version expected by the loaded firmware instance.
 * @param inst The firmware instance.
 * @return Pointer to a mve_fw_version structure data.
 */
struct mve_base_fw_version *mve_fw_get_version3(struct mve_fw_instance *inst);

/**
 * Returns secure attribute of the loaded firmware instance.
 * @param inst The firmware instance.
 * @return true if secure firmware loaded, false otherwise.
 */
bool mve_fw_secure3(struct mve_fw_instance *inst);

/**
 * Log the firmware binary header.
 * @param inst The firmware instance.
 * @param session Pointer to session.
 */
void mve_fw_log_fw_binary3(struct mve_fw_instance *inst, struct mve_session *session);

#ifdef UNIT

/**
 * Returns FW page setup data. Only used by the unit tests to verify FW loading.
 * @param inst The FW instance that data should be queried from.
 * @param mmu_entries [out] List of text pages and markers for shared and non shared BSS pages.
 * @param no_text_pages [out] Number of text pages.
 * @param no_shared_pages [out] Number of shared BSS pages.
 * @param no_bss_pages [out] Number of non shared BSS pages.
 */
void mve_fw_debug_get_info3(struct mve_fw_instance *inst,
                           mve_mmu_entry_t **mmu_entries,
                           uint32_t *no_text_pages,
                           uint32_t *no_shared_pages,
                           uint32_t *no_bss_pages);

#endif

#endif /* MVE_FW_H */
