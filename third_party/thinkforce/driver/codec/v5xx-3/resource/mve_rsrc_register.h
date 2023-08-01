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

#ifndef MVE_RSRC_REGISTER_H
#define MVE_RSRC_REGISTER_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

#define MVE_V500        0x56500000
#define MVE_V550        0x56550000
#define MVE_V61         0x56610000
#define MVE_V52_V76     0x56620000

typedef volatile uint32_t REG32;

#include <host_interface_v3/regs/mve_coresched_reg.h>

/**
 * Initializes this module. Must be called before attempting to write to
 * any registers.
 */
void mver_reg_init3(void);

/**
 * Write a 32-bit value to a MVE hardware register.
 * @param addr Address of the register.
 * @param value Value to write.
 */
void mver_reg_write323(volatile uint32_t *addr, uint32_t value);

/**
 * Read a 32-bit value from a MVE hardware register.
 * @param addr Address of the register.
 * @return Value read from the register.
 */
uint32_t mver_reg_read323(volatile uint32_t *addr);

/**
 * Returns the base address of the core scheduler register bank. Needs
 * to be called before writing to any registers since it will acquire a
 * spinlock that prevents other clients from writing at the same time.
 * @return The register bank address.
 */
tCS *mver_reg_get_coresched_bank3(void);

/**
 * This function must be called when a client is done writing to the registers.
 * Releases the spinlock taken by mve_reg_get_coresched_bank.
 */
void mver_reg_put_coresched_bank3(tCS **regs);

/**
 * Returns the base address of the core scheduler register bank. This function
 * may only be called from IRQ context since it doesn't acquire the spinlock
 * that prevents concurrent writing/reading of registers.
 * @return The register bank address.
 */
tCS *mver_reg_get_coresched_bank_irq3(void);

/**
 * Returns the fuse value that was cached for mve fuse state.
 * @return The cached fuse value.
 */
uint32_t mver_reg_get_fuse3(void);

/**
 * Returns the hardware version.
 * @return The hardware version.
 */
uint32_t mver_reg_get_version3(void);

#ifdef UNIT

/**
 * Enable/disable all communication with the HW.
 * @param enable True if communication should be enabled, false if disabled.
 */
void mver_driver_set_hw_interaction3(bool enable);

#endif

#endif /* MVE_RSRC_REGISTER_H */
