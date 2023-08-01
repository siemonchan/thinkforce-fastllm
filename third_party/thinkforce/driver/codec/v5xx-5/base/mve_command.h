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

#ifndef MVE_COMMAND_H
#define MVE_COMMAND_H

#ifdef EMULATOR
#include "emulator_userspace.h"
#else
#include <linux/types.h>
#endif

#include "mve_base.h"
#include "mve_ioctl.h"

/**
 * This struct is used to store the result of an executed command.
 */
struct mve_response
{
    uint32_t error;             /**< MVE error code */
    uint32_t firmware_error;    /**< Firmware error code */
    int size;                   /**< Size of the supplied data */
    void *data;                 /**< Pointer to the data. The client must free this member! */
};

/**
 * Execute the command specified by the header and the associated data.
 * @param header Specifies which command to execute and the amount of data supplied.
 * @param data   Pointer to the data associated with the command.
 * @param filep  struct file * associated with the file descriptor used to communicate
 *               with the driver.
 * @return Command response.
 */
struct mve_response *mve_command_execute5(struct mve_base_command_header *header,
                                         void *data,
                                         struct file *filep);

#endif /* MVE_COMMAND_H */
