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

#ifndef VEXPRESS_REGS_H
#define VEXPRESS_REGS_H

#define VEXP6 0
#define VEXP7 1
#define JUNO 2

#if (VEXP7 == HW)
#define MVE_CORE_BASE 0xFC030000
#define MVE_CORE_BASE_SIZE 0xFFFF
#define MVE_IRQ 70
#elif (JUNO == HW)
#define MVE_CORE_BASE 0x6F030000
#define MVE_CORE_BASE_SIZE 0xFFFF
#define MVE_IRQ 200
#else
#define MVE_CORE_BASE (0xE0000000 + 0x1C020000)
#define MVE_CORE_BASE_SIZE 0xC000
#define MVE_IRQ 70
#endif

#endif /* VEXPRESS_REGS_H */
