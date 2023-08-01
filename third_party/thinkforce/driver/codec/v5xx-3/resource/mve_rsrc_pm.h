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

#ifndef MVE_RSRC_PM_H
#define MVE_RSRC_PM_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/device.h>
#endif

/**
 * Notify the Linux kernel runtime PM framework that clock and power must be
 * disabled for the hardware. This symbol is exported to the base module.
 *
 * @return 0 on success. Other values indicate failure.
 */
int mver_pm_request_suspend3(void);

/**
 * Notify the Linux kernel runtime PM framework that clock and power must be
 * enabled for the hardware. This symbol is exported to the base module.
 *
 * @return 0 on success. Other values indicate failure.
 */
int mver_pm_request_resume3(void);

/**
 * Initialize the PM module.
 */
void mver_pm_init3(struct device *dev);

/**
 * Deinitialize the PM module. E.g. removes sysfs files etc.
 */
void mver_pm_deinit3(struct device *dev);

/**
 * Enable power to the hardware.
 */
int mver_pm_poweron3(void);

/**
 * Disable power to the hardware.
 */
int mver_pm_poweroff3(void);

/* The following functions must not be called directly from the driver code!
 * These are callback functions called by the Linux kernel */

/**
 * Linux kernel power management callback function. Not to be called directly
 * from the driver!
 *
 * This function stops each running session and saves the session memory for all
 * running sessions. No sessions are allowed to start after this function
 * has been called (until mver_pm_resume has been called).
 */
int mver_pm_suspend3(struct device *dev);

/**
 * Linux kernel power management callback function. Not to be called directly
 * from the driver!
 *
 * Bring back the hardware to a functional state. I.e. undo the operations
 * performed by mver_pm_suspend.
 */
int mver_pm_resume3(struct device *dev);

/**
 * Linux kernel power management callback function. Not to be called directly
 * from the driver!
 */
int mver_pm_runtime_suspend3(struct device *dev);

/**
 * Linux kernel power management callback function. Not to be called directly
 * from the driver!
 */
int mver_pm_runtime_resume3(struct device *dev);

/**
 * Linux kernel power management callback function. Not to be called directly
 * from the driver!
 */
int mver_pm_runtime_idle3(struct device *dev);

/**
 * Request a specific hardware clock frequency.
 *
 * @param freq The requested clock frequency
 */
void mver_pm_request_frequency3(uint32_t freq);

/**
 * Read the current hardware clock freqency.
 *
 * @return The current clock frequency
 */
uint32_t mver_pm_read_frequency3(void);

/**
 * Read the maximum supported hardware clock freqency.
 *
 * @return The maximum supported clock frequency
 */
uint32_t mver_pm_read_max_frequency3(void);

#endif /* MVE_RSRC_PM_H */
