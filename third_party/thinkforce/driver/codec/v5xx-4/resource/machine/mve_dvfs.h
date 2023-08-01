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

#ifndef MVE_DVFS_H
#define MVE_DVFS_H

/**
 * An instance of this struct is stored in the key-value pairs vector using the
 * key MVE_CONFIG_DEVICE_ATTR_DVFS_CALLBACKS.
 */
struct mve_dvfs_callback_conf
{
    /**
     * Called by the driver when the clock signal must be provided to the
     * hardware. This function must not return until the clock is stable.
     */
    void (*enable_clock)(void);

    /**
     * Called by the driver when the clock signal may be shut off to enable
     * power saving. This is just a notification. The implementation may
     * chose to ignore this call.
     */
    void (*disable_clock)(void);

    /**
     * Called by the driver when the clock frequency needs to be adjusted.
     * This may for example happen when the clients attempt to decode
     * several video clips at the same time and the current clock frequency
     * is not sufficient to decode frames at the required rate.
     *
     * @param rate The minimum required clock frequency
     */
    void (*set_rate)(uint32_t clk_rate);

    /**
     * Called by the driver when it needs to know the current clock frequency.
     * This happens when the user reads the sysfs file to get the current
     * clock frequency.
     *
     * @return Current clock frequency
     */
    uint32_t (*get_rate)(void);

    /**
     * Called by the driver when it needs to know maximum supported clock frequency.
     *
     * @return Maximum supported clock frequency
     */
    uint32_t (*get_max_rate)(void);

    /**
     * Called by the driver when driver is removed with rmmod. The implementation
     * should clean up all resources including running threads.
     */
    void (*stop)(void);
};

#endif /* MVE_DVFS_H */
