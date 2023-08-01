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

#ifndef MVE_PORT_ATTRIBUTES_H
#define MVE_PORT_ATTRIBUTES_H

/**
 * This function must return the AXI memory access settings for accessing
 * pages that are marked with the supplied attribute in the MVE page table.
 * The driver invokes this function for each attribute when the driver is
 * initialized and caches the values internally.
 *
 * This function pointer is associated with the key
 * MVE_CONFIG_DEVICE_ATTR_BUS_ATTRIBUTES.
 *
 * @param attribute The attribute as stated in the MVE page table
 * @return AXI memory access settings according to the AXI protocol specification
 */
typedef uint32_t (*mve_port_attributes_callback_fptr)(int attribute);

#endif /* MVE_PORT_ATTRIBUTES_H */
