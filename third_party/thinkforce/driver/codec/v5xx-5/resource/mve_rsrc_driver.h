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

#ifndef MVE_RSRC_DRIVER_H
#define MVE_RSRC_DRIVER_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "machine/mve_config.h"
#endif

/** @brief Structure for holding resource driver information */
struct mve_rsrc_driver_data
{
    /** Pointer to the platform device */
    struct platform_device *pdev;
    /** Assigned irq number */
    int irq_nr;
    /** Assigned irq flags */
    int irq_flags;
    /** Pointer to the memory-mapped registers */
    void __iomem *regs;
    /** Pointer to the memory resource */
    struct resource *mem_res;
    /** Number of cores supported by the hardware */
    int ncore;
    /** Number of logical sessions supported by the hardware */
    int nlsid;

    /** Track hardware interaction */
    bool hw_interaction;
    /** Reference count for IRQ enable/disable */
    int irq_enable_count;

    /** Track configuration data */
    struct mve_config_attribute *config;
    /** Track port-attributes */
    uint32_t port_attributes[4];

#ifdef UNIT
    /** Number of interrupts. */
    uint32_t interrupts;
#endif

    /** Hardware fuse(s) state */
    uint32_t fuse;

    /** Hardware version */
    uint32_t hw_version;

#ifdef INTERRUPT_POLLING
    struct task_struct *irq_thread;
#endif
};

#ifndef NELEMS
#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define MVE_RSRC_DRIVER_NAME "mali-v500-rsrc-5"

extern struct mve_rsrc_driver_data mve_rsrc_data5;

#endif /* MVE_RSRC_DRIVER_H */
