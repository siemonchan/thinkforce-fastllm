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

#ifndef MVE_SESSION_BUFFER_H
#define MVE_SESSION_BUFFER_H

#include "mve_base.h"
#include "mve_com.h"

/**
 * Unregister all user allocated buffers with the driver.
 * @param session The session.
 */
void mve_session_buffer_unmap_all6(struct mve_session *session);

/**
 * Internal function called by mve_session_buffer_register. It registers
 * a user allocated buffer with the session. This must be done before
 * the buffer can be used by the MVE.
 * @param session          The session.
 * @param port_index       Firmware Buffer Queue Index (0 = input, 1 = output).
 * @param userspace_buffer Buffer descriptor.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_register_internal6(struct mve_session *session,
                                                    uint32_t port_index,
                                                    struct mve_base_buffer_userspace *userspace_buffer);

/**
 * Internal function called by mve_session_buffer_unregister. It unregisters
 * a user allocated buffer with the session. This must be done to free
 * the resources that were allocated when the buffer was registered.
 * @param session   The session.
 * @param buffer_id ID of the buffer.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_unregister_internal6(struct mve_session *session,
                                                      mve_base_buffer_handle_t buffer_id);

/**
 * Internal function called by mve_session_buffer_enqueue. It enqueues an input
 * or output buffer.
 * @param session           The session.
 * @param param             Description of the input/output buffer.
 * @param empty_this_buffer Signals whether this buffer shall be emptied or filled.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_enqueue_internal6(struct mve_session *session,
                                                   struct mve_base_buffer_details *param,
                                                   bool empty_this_buffer);

/**
 * Internal function called by the interrupt handler when a buffer has been
 * returned by the hardware. This function updates the internal representation
 * of the buffer with the data returned by the hardware.
 * @param session   The session.
 * @param buffer_id ID of the returned buffer.
 * @param buffer    The buffer data returned by the hardware.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_dequeue_internal6(struct mve_session *session,
                                                   mve_base_buffer_handle_t buffer_id,
                                                   mve_com_buffer *buffer);

/**
 * Internal function called by mve_session_buffer_notify_ref_frame_release. It sends
 * a request to the firmware that the userspace driver wants to be notified
 * when the supplied buffer is no longer used as a reference frame.
 * @param session   The session.
 * @param buffer_id ID of the buffer.
 * @return MVE error code.
 */
mve_base_error mve_session_buffer_notify_ref_frame_release_internal6(struct mve_session *session,
                                                                    mve_base_buffer_handle_t buffer_id);

/**
 * Fill in the data for a MVE_EVENT_INPUT / MVE_EVENT_INPUT event.
 * @param dst Destination of the data (event data container).
 * @param src The buffer client where the data can be found.
 */
void mve_session_buffer_convert_to_userspace6(struct mve_base_buffer_userspace *dst,
                                             struct mve_buffer_client *src);

/**
 * Returns the buffer descriptor (if one exists) for a user allocated buffer
 * mapping matching the supplied buffer ID.
 * @param session   The session.
 * @param buffer_id ID of the user allocated buffer.
 * @return The buffer descriptor if the supplied user allocated buffer has been
 *         mapped. NULL if no such descriptor exists.
 */
struct mve_buffer_client *mve_session_buffer_get_external_descriptor6(struct mve_session *session,
                                                                     mve_base_buffer_handle_t buffer_id);

#endif /* MVE_SESSION_BUFFER_H */
