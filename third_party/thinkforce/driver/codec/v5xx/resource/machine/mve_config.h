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

#ifndef MVE_CONFIG_H
#define MVE_CONFIG_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

/**
 * This enum lists the possible keys that may be inserted into a key-value list.
 */
enum mve_config_key
{
    /**
     * Power management functions. The value associated with this key must be a
     * reference to a struct mve_pm_callback_conf.
     */
    MVE_CONFIG_DEVICE_ATTR_POWER_CALLBACKS,

    /**
     * Callback function to retrieve bus attributes. The value associated
     * with this key must be a function pointer conforming to the prototype
     * uint32_t (*)(int)
     */
    MVE_CONFIG_DEVICE_ATTR_BUS_ATTRIBUTES,

    /**
     * Callback functions to support dynamic voltage and frequency scaling.
     * Currently, this is used to enable/disable the clock and change the
     * clock frequency. The implementation of these functions are assumed
     * to take care of the voltage settings. The value associated with this
     * key must be a reference to a struct mve_dvfs_callback_conf.
     */
    MVE_CONFIG_DEVICE_ATTR_DVFS_CALLBACKS,

    /**
     * End of key-value pairs vector indicator. The configuration loader will stop
     * processing any more elements when it encounters this key. Note that
     * the key-value vector must be terminated with this key! The value
     * associated with this key is ignored.
     */
    MVE_CONFIG_DEVICE_ATTR_END
};

/**
 * Type definition of the value type used in the key-value pairs structure.
 */
typedef uint32_t *mve_config_attribute_value;

/**
 * Each element in the key-value pairs vector is stored in an instance of this
 * structure.
 */
struct mve_config_attribute
{
    enum mve_config_key key;          /**< The key of the element */
    mve_config_attribute_value value; /**< The value corresponding to the key
                                       *   above */
};

/**
 * Returns the value associated with the first occurrence of the supplied key.
 * The vector must be terminated by a MVE_CONFIG_DEVICE_ATTR_END key.
 * @param attributes A vector containing key-value pairs.
 * @param key        The key to find the value for.
 * @return The value if the key exists in the list. NULL if no such key exists.
 */
uint32_t *mve_config_get_value(struct mve_config_attribute *attributes,
                               enum mve_config_key key);

#endif /* MVE_CONFIG_H */
