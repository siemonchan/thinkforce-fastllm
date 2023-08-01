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

#ifndef MVE_DRIVER_H
#define MVE_DRIVER_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/platform_device.h>
#endif

#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))

/**
 * Pointer to platform device structure.
 */
extern struct device *mve_device3;

#endif
