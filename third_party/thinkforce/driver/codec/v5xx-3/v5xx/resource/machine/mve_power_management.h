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

#ifndef MVE_POWER_MANAGEMENT_H
#define MVE_POWER_MANAGEMENT_H

/**
 * An instance of this struct is stored in the key-value pairs vector using the
 * key MVE_CONFIG_DEVICE_ATTR_POWER_CALLBACKS.
 */
struct mve_pm_callback_conf
{
    /**
     * Called by the driver when the VPU is idle and the power to it can be
     * switched off. The system integrator can decide whether to either do
     * nothing, just switch off the clocks to the VPU, or to completely power
     * down the VPU.
     */
    void (*power_off_callback)(void);

    /**
     * Called by the driver when the VPU is about to become active and power
     * must be supplied. This function must not return until the VPU is powered
     * and clocked sufficiently for register access to succeed.  The return
     * value specifies whether the VPU was powered down since the call to
     * power_off_callback. If the VPU state has been lost then this function
     * must return 1, otherwise it should return 0.
     *
     * The return value of the first call to this function is ignored.
     *
     * @return 1 if the VPU state may have been lost, 0 otherwise.
     */
    int (*power_on_callback)(void);
};

#endif /* MVE_POWER_MANAGEMENT_H */
