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

#include "mve_com.h"
#include "mve_session.h"
#include "mve_rsrc_log.h"

#include <host_interface_v3/mve_protocol_def.h>

#include "mve_com_host_interface_v2.h"
#include "mve_com_host_interface_v3.h"

mve_base_error mve_com_add_message4(struct mve_session *session,
                                   uint16_t code,
                                   uint16_t size,
                                   uint32_t *data)
{
    mve_base_error ret = MVE_BASE_ERROR_NONE;

    if (session->com == NULL || session->com->host_interface.add_message == NULL)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    ret = session->com->host_interface.add_message(session, code, size, data);

    if (MVE_BASE_ERROR_NONE != ret)
    {
        MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p mve_com_add_message() returned error status. ret=%u, code=%u, size=%u.", session, ret, code, size);
    }

    if (MVE_REQUEST_CODE_SWITCH != code &&
        MVE_REQUEST_CODE_IDLE_ACK != code)
    {
        session->state.idle_state = IDLE_STATE_ACTIVE;
    }

    return ret;
}

uint32_t *mve_com_get_message4(struct mve_session *session,
                              struct mve_msg_header *header)
{
    uint32_t *ret = NULL;

    if (session->com == NULL || session->com->host_interface.get_message == NULL)
    {
        return NULL;
    }

    ret = session->com->host_interface.get_message(session, header);

    return ret;
}

mve_base_error mve_com_add_input_buffer4(struct mve_session *session,
                                        mve_com_buffer *buffer,
                                        enum mve_com_buffer_type type)
{
    mve_base_error res;

    if (session->com == NULL || session->com->host_interface.add_input_buffer == NULL)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    if (NULL == buffer)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    res = session->com->host_interface.add_input_buffer(session, buffer, type);

    if (MVE_BASE_ERROR_NONE == res)
    {
        if (type != MVE_COM_BUFFER_TYPE_ROI)
        {
            session->input_buffer_count++;
        }
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p mve_com_add_input_buffer4() returned error status. ret=%u.", session, res);
    }

    return res;
}

mve_base_error mve_com_add_output_buffer4(struct mve_session *session,
                                         mve_com_buffer *buffer,
                                         enum mve_com_buffer_type type)
{
    mve_base_error res;

    if (session->com == NULL || session->com->host_interface.add_output_buffer == NULL)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    if (NULL == buffer)
    {
        return MVE_BASE_ERROR_BAD_PARAMETER;
    }

    res = session->com->host_interface.add_output_buffer(session, buffer, type);

    if (MVE_BASE_ERROR_NONE == res)
    {
        session->output_buffer_count++;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p mve_com_add_output_buffer4() returned error status. ret=%u.", session, res);
    }

    return res;
}

mve_base_error mve_com_get_input_buffer4(struct mve_session *session,
                                        mve_com_buffer *buffer)
{
    mve_base_error ret;

    if (session->com == NULL || session->com->host_interface.get_input_buffer == NULL)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    ret = session->com->host_interface.get_input_buffer(session, buffer);

    if (MVE_BASE_ERROR_NONE == ret)
    {
        session->input_buffer_count--;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p mve_com_get_input_buffer4() returned error status. ret=%u.", session, ret);
    }

    return ret;
}

mve_base_error mve_com_get_output_buffer4(struct mve_session *session,
                                         mve_com_buffer *buffer)
{
    mve_base_error ret;

    if (session->com == NULL || session->com->host_interface.get_output_buffer == NULL)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    ret = session->com->host_interface.get_output_buffer(session, buffer);

    if (MVE_BASE_ERROR_NONE == ret)
    {
        session->output_buffer_count--;
    }
    else
    {
        MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p mve_com_get_output_buffer4() returned error status. ret=%u.", session, ret);
    }

    return ret;
}

mve_base_error mve_com_get_rpc_message4(struct mve_session *session,
                                       mve_com_rpc *rpc)
{
    if (NULL == session->com || NULL == session->com->host_interface.get_rpc_message)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    return session->com->host_interface.get_rpc_message(session, rpc);
}

mve_base_error mve_com_put_rpc_message4(struct mve_session *session,
                                       mve_com_rpc *rpc)
{
    if (NULL == session->com || NULL == session->com->host_interface.put_rpc_message)
    {
        return MVE_BASE_ERROR_VERSION_MISMATCH;
    }

    return session->com->host_interface.put_rpc_message(session, rpc);
}

void mve_com_delete4(struct mve_session *session)
{
    MVE_RSRC_MEM_FREE(session->com);
    session->com = NULL;
}

mve_base_error mve_com_set_interface_version4(struct mve_session *session,
                                             enum mve_base_fw_major_prot_ver version)
{
    mve_com_delete4(session);

    switch (version)
    {
        case MVE_BASE_FW_MAJOR_PROT_V2:
        {
            session->com = mve_com_host_interface_v2_new4();
            break;
        }

        case MVE_BASE_FW_MAJOR_PROT_V3:
        {
            session->com = mve_com_host_interface_v3_new4();
            break;
        }

        default:
        {
            MVE_LOG_PRINT(&mve_rsrc_log_fwif4, MVE_LOG_ERROR, "%p unsupported interface version configured. version=%u.", session, version);
            return MVE_BASE_ERROR_BAD_PARAMETER;
        }
    }

    if (session->com == NULL)
    {
        return MVE_BASE_ERROR_INSUFFICIENT_RESOURCES;
    }

    return MVE_BASE_ERROR_NONE;
}
