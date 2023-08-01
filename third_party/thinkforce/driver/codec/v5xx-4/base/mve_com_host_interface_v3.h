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

#ifndef MVE_COM_HOST_INTERFACE_V3_H
#define MVE_COM_HOST_INTERFACE_V3_H

#include "mve_com.h"

struct mve_com *mve_com_host_interface_v3_new4(void);
const struct mve_mem_virt_region *mve_com_hi_v3_get_mem_virt_region4(void);

#endif /* MVE_COM_HOST_INTERFACE_V3_H */
