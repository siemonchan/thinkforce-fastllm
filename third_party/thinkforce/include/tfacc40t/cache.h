//
// Created by huangyuyang on 10/14/21.
//

#ifndef TFACC_CACHE_H
#define TFACC_CACHE_H

namespace tfacc40t {
#define CACHE_BASE                0x81000000 // for interleave
#define ACTIVE_WAY_EN                0x0000
#define CACHE_GNL_CTRL                0x0004
#define CACHE_GNL_CTRL1                0x023c // set preload credit
#define CACHE_CLK_CTRL                0x0008
#define CACHE_ADDR_MAP0_LOW_0_31        0x0010
#define CACHE_ADDR_MAP0_HI_0_31            0x0014
#define UNCACHE_ADDR_MAP0_LOW_0_31        0x0018
#define UNCACHE_ADDR_MAP0_HI_0_31        0x001C
#define CACHE_ADDR_MAP1_LOW_0_31        0x0020
#define CACHE_ADDR_MAP1_HI_0_31            0x0024
#define UNCACHE_ADDR_MAP1_LOW_0_31        0x0028
#define UNCACHE_ADDR_MAP1_HI_0_31        0x002C
#define CACHE_ADDR_MAP0_LOW_32_63        0x0030
#define CACHE_ADDR_MAP0_HI_32_63        0x0034
#define UNCACHE_ADDR_MAP0_LOW_32_63        0x0038
#define UNCACHE_ADDR_MAP0_HI_32_63        0x003C
#define CACHE_ADDR_MAP1_LOW_32_63        0x0040
#define CACHE_ADDR_MAP1_HI_32_63        0x0044
#define UNCACHE_ADDR_MAP1_LOW_32_63        0x0048
#define UNCACHE_ADDR_MAP1_HI_32_63        0x004C
#define CACHE_INIT_REQ                0x0050
#define CACHE_INIT_ACK                0x0054
#define CACHE_INVALID_REQ            0x0058
#define CACHE_INVALID_ADDR            0x005C
#define CACHE_INVALID_ADDR_HI            0x0060
#define CACHE_INVALID_LEN            0x0064
#define CACHE_INVALID_ACK            0x0068
#define CACHE_LOCK_REQ            0x006C
#define CACHE_LOCK_ADDR            0x0070
#define CACHE_LOCK_ADDR_HI            0x0074
#define CACHE_LOCK_LEN                0x0078
#define CACHE_LOCK_ACK                0x007C
#define CACHE_PL_REQ                0x0080
#define CACHE_PL_ADDR                0x0084
#define CACHE_PL_ADDR_HI            0x0088
#define CACHE_PL_LEN                0x008C
#define CACHE_PL_AES0                0x0090
#define CACHE_PL_STAT                0x0098
#define CACHE_PL_CNT                0x009C
#define CACHE_PL_NUM                0x234
#define CACHE_PL_STRIDE            0x238
#define CACHE_INVALID_NUM            0x240
#define CACHE_INVALID_STRIDE        0x244
#define CACHE_LOCK_NUM            0x248
#define CACHE_LOCK_STRIDE            0x24C
#define CACHE_ERR_CLR                0x00A0
#define CACHE_AXI_STATUS            0x00A4
#define CACHE_CONCAT_ARUSER_L       0x00c0
#define CACHE_CONCAT_AWUSER_L       0x00c8
#define CACHE_PERMUTE_ARUSER_L      0x00d0
#define CACHE_PERMUTE_AWUSER_L      0x00d8
#define CONC_PERM_R_HI              0x00d4
#define CONC_PERM_W_HI              0x00dc
#define DBG_CNT_M0_AXI_B            0x0100
#define DBG_CNT_M1_AXI_B            0x0104
#define DBG_CNT_M0_AXI_W            0x0108
#define DBG_CNT_M1_AXI_W            0x010C
#define DBG_CNT_M0_AXI_AW            0x0110
#define DBG_CNT_M1_AXI_AW            0x0114
#define DBG_CNT_M0_AXI_R            0x0118
#define DBG_CNT_M1_AXI_R            0x011C
#define DBG_CNT_M0_AXI_AR            0x0120
#define DBG_CNT_M1_AXI_AR            0x0124
#define DBG_CNT_P0_AXI_B            0x0128
#define DBG_CNT_P1_AXI_B            0x012C
#define DBG_CNT_P2_AXI_B            0x0130
#define DBG_CNT_P0_AXI_W            0x0134
#define DBG_CNT_P1_AXI_W            0x0138
#define DBG_CNT_P2_AXI_W            0x013C
#define DBG_CNT_P0_AXI_AW            0x0140
#define DBG_CNT_P1_AXI_AW            0x0144
#define DBG_CNT_P2_AXI_AW            0x0148
#define DBG_CNT_P0_AXI_R            0x014C
#define DBG_CNT_P1_AXI_R            0x0150
#define DBG_CNT_P2_AXI_R            0x0154
#define DBG_CNT_P0_AXI_AR            0x0158
#define DBG_CNT_P1_AXI_AR            0x015C
#define DBG_CNT_P2_AXI_AR            0x0160
#define DBG_CNT_P0_PL_AXI_R                0x0164
#define DBG_CNT_P1_PL_AXI_R                0x0168
#define DBG_CNT_P2_PL_AXI_R                0x016C
#define DBG_CNT_P0_PL_AXI_AR                0x0170
#define DBG_CNT_P1_PL_AXI_AR                0x0174
#define DBG_CNT_P2_PL_AXI_AR                0x0178
#define DBG_CNT_BK0_RD_HIT_CMD_WO_PL        0x017C
#define DBG_CNT_BK1_RD_HIT_CMD_WO_PL        0x0180
#define DBG_CNT_BK0_RD_MST_CMD_WO_PL        0x0184
#define DBG_CNT_BK1_RD_MST_CMD_WO_PL        0x0188
#define DBG_CNT_BK0_RD_HIT_CMD            0x018C
#define DBG_CNT_BK1_RD_HIT_CMD            0x0190
#define DBG_CNT_BK0_RD_MST_CMD            0x0194
#define DBG_CNT_BK1_RD_MST_CMD            0x0198
#define DBG_CNT_BK0_WR_HIT_CMD            0x019C
#define DBG_CNT_BK1_WR_HIT_CMD            0x01A0
#define DBG_CNT_BK0_WR_MST_CMD            0x01A4
#define DBG_CNT_BK1_WR_MST_CMD            0x01A8

//#define DDR1_BASE 0xea000000

//cleaninvalid == invalid
//cleanshared == flush
//makeinvalid == invlaid with no write back
//nb == non blocking
    int cache_init();

    int cache_set_addr_range(unsigned int cache_start, unsigned int cache_end, unsigned int uncache_start,
                             unsigned int uncache_end);

    void cache_cleaninvalid_all();

    void cache_cleaninvalid_range(long long start_addr, unsigned int num_bytes);

    void cache_makeinvalid_all();

    void cache_makeinvalid_range(long long start_addr, unsigned int num_bytes);

    void cache_cleanshared_all();

    void cache_cleanshared_range(long long start_addr, unsigned int num_bytes);

    void cache_preload_range(long long start_addr,
                             unsigned int num_bytes);//suggestion: do lock before preload, so that the content won't be swapped out
    void cache_preload_range_nb(long long start_addr, unsigned int num_bytes);

    void cache_lock_range(long long start_addr, unsigned int num_bytes);

    void cache_lock_range_nb(long long start_addr, unsigned int num_bytes);

    void cache_unlock_range(long long start_addr, unsigned int num_bytes);

    void cache_unlock_range_nb(long long start_addr, unsigned int num_bytes);

    void cache_unlock_all();

    void cache_unlock_all_nb();

    void cache_preload_cmds(struct BlasopList *blasopList, long addr, long size, int port);

    void cache_preload_cmds2(struct BlasopList *blasopList, long addr, long size, int port);

    void cache_preload_cmds_stride(struct BlasopList *blasopList, long addr, long size, int port, int num, int stride);

    void cache_clear_cmds_stride(struct BlasopList *blasopList, unsigned long addr, long size, int num, int stride);

    void cache_preload_cmds_stride2(struct BlasopList *blasopList, long addr, long size, int port, int num, int stride);

    void cache_makeinvalid_cmds(struct BlasopList *blasopList, unsigned long addr, long size);

    void cache_cleaninvaild_cmds(struct BlasopList *blasopList, unsigned long addr, long size);

    void cache_lock_cmds(struct BlasopList *blasopList, unsigned long addr, long size);

    void cache_unlock_cmds(struct BlasopList *blasopList, unsigned long addr, long size);

    void cache_invalid_cmds_stride(struct BlasopList *blasopList, unsigned long addr, long size, int num, int stride);

    void cache_invalid_cmds(struct BlasopList *blasopList, unsigned long addr, long size);

    void DoClearCache(uint64_t address, int len);

    void ClearCache(uint64_t address, int len);

}
#endif //TFACC_CACHE_H