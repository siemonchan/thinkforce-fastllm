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

#ifndef MVE_RSRC_DVFS_H
#define MVE_RSRC_DVFS_H

/**
 * Structure which represents session status
 * It is used by DVFS to take a decision about power management
 */
struct mver_dvfs_session_status
{
    /* Amount of restricting buffers enqueued to the FW. Restricting buffers
     * are what usually limits the current work. For a decoder, this is the
     * number of output buffers (limited by the display refresh rate). For an
     * encoder, the restricting buffers are the input buffers (limited by the
     * rate at which the camera generates images). */
    int restricting_buffer_count;
};

/**
 * Function pointer to query session status
 *
 * @param session_id Session id
 * @param status Pointer to a structure where the status should be returned
 * @return True when query was successful,
 *         False otherwise
 */
typedef bool (*mver_dvfs_get_session_status_fptr)(const mver_session_id session_id, struct mver_dvfs_session_status *status);

/**
 * Initialize the DVFS module.
 *
 * Must be called before any other function in this module.
 *
 * @param dev Device
 * @param get_session_status_callback Callback to query session status
 */
void mver_dvfs_init5(struct device *dev,
                    const mver_dvfs_get_session_status_fptr get_session_status_callback);

/**
 * Deinitialize the DVFS module.
 *
 * All remaining sessions will be unregistered.
 *
 * @param dev Device
 */
void mver_dvfs_deinit5(struct device *dev);

/**
 * Register session in the DFVS module.
 *
 * @param session_id Session id
 * @return True when registration was successful,
 *         False, otherwise
 */
bool mver_dvfs_register_session5(const mver_session_id session_id);

/**
 * Unregister session from the DFVS module.
 *
 * Usage of corresponding session is not permitted after this call.
 * @param session_id Session id
 */
void mver_dvfs_unregister_session5(const mver_session_id session_id);

/**
 * Request maximum clock frequency. This function is usually called in situations
 * when the client requests to increase the operating rate.
 */
void mver_dvfs_request_max_frequency5(void);

#endif /* MVE_RSRC_DVFS_H */
