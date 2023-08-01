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

#include "mve_config.h"

uint32_t *mve_config_get_value2(struct mve_config_attribute *attributes,
                               enum mve_config_key key)
{
    if (NULL == attributes)
    {
        return NULL;
    }

    while (MVE_CONFIG_DEVICE_ATTR_END != attributes->key)
    {
        if (attributes->key == key)
        {
            return attributes->value;
        }

        attributes++;
    }

    return NULL;
}
