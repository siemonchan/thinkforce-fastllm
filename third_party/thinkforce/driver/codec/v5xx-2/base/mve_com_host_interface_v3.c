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

/****************************************************************************
 * Include v3 interface
 ****************************************************************************/

#include "host_interface_v3/mve_protocol_def.h"

/****************************************************************************
 * Includes
 ****************************************************************************/

#include "mve_com_host_interface_v3.h"
#include "mve_com_host_interface_v2.h"
#include "mve_com.h"

/* Virtual memory region map for V52/V76 and future hardware */
static const struct mve_mem_virt_region mem_regions[] =
{
    {   /* VIRT_MEM_REGION_FIRMWARE0 */
        MVE_MEM_REGION_FW_INSTANCE0_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE0_ADDR_END
    },
    #define MVE_COMM_QUEUE_SIZE 0x1000
    {   /* VIRT_MEM_REGION_MSG_IN_QUEUE */
        MVE_COMM_MSG_INQ_ADDR,
        MVE_COMM_MSG_INQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_MSG_OUT_QUEUE */
        MVE_COMM_MSG_OUTQ_ADDR,
        MVE_COMM_MSG_OUTQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_INPUT_BUFFER_IN */
        MVE_COMM_BUF_INQ_ADDR,
        MVE_COMM_BUF_INQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_INPUT_BUFFER_OUT */
        MVE_COMM_BUF_INRQ_ADDR,
        MVE_COMM_BUF_INRQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_OUTPUT_BUFFER_IN */
        MVE_COMM_BUF_OUTQ_ADDR,
        MVE_COMM_BUF_OUTQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_OUTPUT_BUFFER_OUT */
        MVE_COMM_BUF_OUTRQ_ADDR,
        MVE_COMM_BUF_OUTRQ_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_RPC_QUEUE */
        MVE_COMM_RPC_ADDR,
        MVE_COMM_RPC_ADDR + MVE_COMM_QUEUE_SIZE
    },
    {   /* VIRT_MEM_REGION_PROTECTED */
        MVE_MEM_REGION_PROTECTED_ADDR_BEGIN,
        MVE_MEM_REGION_PROTECTED_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_OUT_BUF */
        MVE_MEM_REGION_FRAMEBUF_ADDR_BEGIN,
        MVE_MEM_REGION_FRAMEBUF_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE1 */
        MVE_MEM_REGION_FW_INSTANCE1_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE1_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE2 */
        MVE_MEM_REGION_FW_INSTANCE2_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE2_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE3 */
        MVE_MEM_REGION_FW_INSTANCE3_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE3_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE4 */
        MVE_MEM_REGION_FW_INSTANCE4_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE4_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE5 */
        MVE_MEM_REGION_FW_INSTANCE5_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE5_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE6 */
        MVE_MEM_REGION_FW_INSTANCE6_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE6_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_FIRMWARE7 */
        MVE_MEM_REGION_FW_INSTANCE7_ADDR_BEGIN,
        MVE_MEM_REGION_FW_INSTANCE7_ADDR_END,
    },
    {   /* VIRT_MEM_REGION_REGS */
        0xF0000000,
        0xFFFFFFFF,
    }
};

struct mve_com *mve_com_host_interface_v3_new2(void)
{
    struct mve_com *com = mve_com_host_interface_v2_new2();
    if (NULL != com)
    {
        com->host_interface.prot_ver = MVE_BASE_FW_MAJOR_PROT_V3;
    }
    return com;
}

const struct mve_mem_virt_region *mve_com_hi_v3_get_mem_virt_region2(void)
{
    return mem_regions;
}
