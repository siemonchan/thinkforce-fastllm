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

#ifndef __EMUL_INCLUDE__MVE_CORESCHED_REG_H__
#define __EMUL_INCLUDE__MVE_CORESCHED_REG_H__


/*
 * Please note: The core scheduler registers are not accessible by the firmware.
 */

/*  */
#define SOC_CORE_INTERVAL           0x200000u

#define SOC_GET_COREID(base, addr)  ((((addr)-(base)) >> 21) & 0xf)

#define SOC_CORESCHED_OFFSET        0x000000u

#if defined(MVE_VERSION) && MVE_VERSION < 0x56620000
#define SESSIONRAM_SIZE_PER_LSID (4096)
#else
#define SESSIONRAM_SIZE_PER_LSID (8192)
#endif

#define MMU_LOG2_PAGE_SIZE (12)
#define MMU_PAGE_SIZE (1 << MMU_LOG2_PAGE_SIZE)

typedef struct _CS
{
  REG32 VERSION;       /* (0x0000) Version register */
  REG32 ENABLE;        /* (0x0004) 1:enable scheduling. Poll for 0 after writing 0. */
  REG32 NCORES;        /* (0x0008) Number of cores (read-only) */
  REG32 NLSID;         /* (0x000c) Number of lsid (read-only) */
  REG32 CORELSID;      /* (0x0010) 4-bit entry per core specifying the current lsid or 15 if none (read-only) */
  REG32 JOBQUEUE;      /* (0x0014) Four 8-bit entries specifying jobs to be scheduled. */
                       /* Entries should be packed at the lower end of JOBQUEUE and consist of
                          a pair (LSID, NCORES-1) specifying which session and how many cores to schedule
                          the job to. Unused entries should hold the value JOBQUEUE_JOB_INVALID. */
    #define CORESCHED_JOBQUEUE_JOB0 (0)
    #define CORESCHED_JOBQUEUE_JOB1 (8)
    #define CORESCHED_JOBQUEUE_JOB2 (16)
    #define CORESCHED_JOBQUEUE_JOB3 (24)
    #define CORESCHED_JOBQUEUE_JOB0_SZ (8)
    #define CORESCHED_JOBQUEUE_JOB1_SZ (8)
    #define CORESCHED_JOBQUEUE_JOB2_SZ (8)
    #define CORESCHED_JOBQUEUE_JOB3_SZ (8)
    #define JOBQUEUE_JOB_LSID      (0) /* Which LSID to run */
    #define JOBQUEUE_JOB_NCORES    (4) /* No. of cores-1 needed to run this job */
    #define JOBQUEUE_JOB_LSID_SZ   (4)
    #define JOBQUEUE_JOB_NCORES_SZ (4)
    #define JOBQUEUE_JOB_INVALID   (0x0f) /* code indicating an unused job queue entry */
  REG32 IRQVE;         /* (0x0018) one bit per LSID, irq from VE to host (read-only bitvector version of LSID[].IRQVE) */
  REG32 CLKPAUSE;      /* (0x001c) one bit per core - set to pause its clock */
  REG32 CLKIDLE;       /* (0x0020) one bit per core - '1' indicates that pause has taken effect (read-only) */
  REG32 CLKFORCE;      /* (0x0024) one bit per core - '1' forces clock to be enabled */
    #define CORESCHED_CLKFORCE_CORE          (0) /* set to force clock to run for core */
    #define CORESCHED_CLKFORCE_CS            (8) /* set to force clock to run for core scheduler (makes TIMER run) */
    #define CORESCHED_CLKFORCE_POWER         (9) /* set to force core power to be enabled */
    #define CORESCHED_CLKFORCE_CORE_SZ       (8) /* actually only NCORES bits */
    #define CORESCHED_CLKFORCE_CS_SZ         (1) /* single-bit field */
    #define CORESCHED_CLKFORCE_POWER_SZ      (1) /* single-bit field */
  REG32 TIMER;         /* (0x0028) 32 high bits of 36 bit timer (use CLKFORCE_CS to make it run) */
  REG32 COREDEBUG;     /* (0x002c) Base address mapped over SESSIONRAM   */
  REG32 SVNREV;        /* (0x0030) SVN revision no (read-only) */
  REG32 FUSE;          /* (0x0034) Fuse bits (read-only) */
    #define CORESCHED_FUSE_DISABLE_AFBC (0)
    #define CORESCHED_FUSE_DISABLE_REAL (1)
    #define CORESCHED_FUSE_DISABLE_VPX  (2)
    #define CORESCHED_FUSE_DISABLE_HEVC (3)
    #define CORESCHED_FUSE_DISABLE_AFBC_SZ (1)
    #define CORESCHED_FUSE_DISABLE_REAL_SZ (1)
    #define CORESCHED_FUSE_DISABLE_VPX_SZ  (1)
    #define CORESCHED_FUSE_DISABLE_HEVC_SZ (1)
  REG32 CONFIG;        /* (0x0038) Configuration (read-only) */
    #define CORESCHED_CONFIG_NCORES         (0)
    #define CORESCHED_CONFIG_NAXIM1         (6) /* No. of AXI interfaces - 1 */
    #define CORESCHED_CONFIG_DATA_WIDTH     (8) /* note: HSIZE notation */
    #define CORESCHED_CONFIG_IDBITS         (12)
    #define CORESCHED_CONFIG_ENCODE         (16)
    #define CORESCHED_CONFIG_AFBC           (17)
    #define CORESCHED_CONFIG_REAL           (18)
    #define CORESCHED_CONFIG_VP8            (19)
    #define CORESCHED_CONFIG_10BIT          (20)
    #define CORESCHED_CONFIG_422            (21)
    #define CORESCHED_CONFIG_UNUSED_0       (22)
    #define CORESCHED_CONFIG_AFBC_IN        (23)
    #define CORESCHED_CONFIG_10BIT_ENC      (24)
    #define CORESCHED_CONFIG_VP9            (25)
    #define CORESCHED_CONFIG_8K             (26)
    #define CORESCHED_CONFIG_HEVC           (27)
    #define CORESCHED_CONFIG_NCORES_SZ      (6)
    #define CORESCHED_CONFIG_NAXIM1_SZ      (2)
    #define CORESCHED_CONFIG_DATA_WIDTH_SZ  (4)
    #define CORESCHED_CONFIG_IDBITS_SZ      (4)
    #define CORESCHED_CONFIG_ENCODE_SZ      (1)
    #define CORESCHED_CONFIG_AFBC_SZ        (1)
    #define CORESCHED_CONFIG_REAL_SZ        (1)
    #define CORESCHED_CONFIG_VP8_SZ         (1)
    #define CORESCHED_CONFIG_10BIT_SZ       (1)
    #define CORESCHED_CONFIG_422_SZ         (1)
    #define CORESCHED_CONFIG_UNUSED_0_SZ    (1)
    #define CORESCHED_CONFIG_AFBC_IN_SZ     (1)
    #define CORESCHED_CONFIG_10BIT_ENC_SZ   (1)
    #define CORESCHED_CONFIG_VP9_SZ         (1)
    #define CORESCHED_CONFIG_8K_SZ          (1)
    #define CORESCHED_CONFIG_HEVC_SZ        (1)
  REG32 TOPRAM_KB;     /* (0x003c) TOPRAM size in kilobytes (read-only) */
  REG32 PROTCTRL;      /* (0x0040) Protection policy */
    #define CORESCHED_PROTCTRL_CPS               (0)
    #define CORESCHED_PROTCTRL_L1R               (1)
    #define CORESCHED_PROTCTRL_L2R               (3)
    #define CORESCHED_PROTCTRL_BARO              (5)
    #define CORESCHED_PROTCTRL_PTMASK            (8)
    #define CORESCHED_PROTCTRL_NSAIDDEFAULT      (12)
    #define CORESCHED_PROTCTRL_NSAIDPUBLIC       (16)
    #define CORESCHED_PROTCTRL_NSAIDPROTECTED    (20)
    #define CORESCHED_PROTCTRL_NSAIDOUTBUF       (24)
    #define CORESCHED_PROTCTRL_NSAIDPRIVATE      (28)
    #define CORESCHED_PROTCTRL_CPS_SZ            (1)
    #define CORESCHED_PROTCTRL_L1R_SZ            (2)
    #define CORESCHED_PROTCTRL_L2R_SZ            (2)
    #define CORESCHED_PROTCTRL_BARO_SZ           (1)
    #define CORESCHED_PROTCTRL_PTMASK_SZ         (4)
    #define CORESCHED_PROTCTRL_NSAIDDEFAULT_SZ   (4)
    #define CORESCHED_PROTCTRL_NSAIDPUBLIC_SZ    (4)
    #define CORESCHED_PROTCTRL_NSAIDPROTECTED_SZ (4)
    #define CORESCHED_PROTCTRL_NSAIDOUTBUF_SZ    (4)
    #define CORESCHED_PROTCTRL_NSAIDPRIVATE_SZ   (4)    
  REG32 BUSCTRL;       /* (0x0044) Bus control */
    #define CORESCHED_BUSCTRL_SPLIT              (0) /* Configuration for burst splitting */
    #define CORESCHED_BUSCTRL_SPLIT_SZ           (2)
      #define CORESCHED_BUSCTRL_SPLIT_128          (0) /* Split bursts at 128-byte boundaries */
      #define CORESCHED_BUSCTRL_SPLIT_256          (1) /* Split bursts at 256-byte boundaries */
      #define CORESCHED_BUSCTRL_SPLIT_512          (2) /* Split bursts at 512-byte boundaries */
      #define CORESCHED_BUSCTRL_SPLIT_1024         (3) /* Split bursts at 1024-byte boundaries */
    #define CORESCHED_BUSCTRL_REF64              (2) /* Reference cache block size 0=128, 1=64 bytes */
    #define CORESCHED_BUSCTRL_REF64_SZ           (1)
  
  REG32 _pad0[2];
  REG32 RESET;         /* (0x0050) Write '1' to reset the VE. Writing other values results in undefined behaviour */
  REG32 _pad1[11];
  REG32 _pad2[6*16];   /* (0x0080) Reserved for CORECONFIG[0..7] */

  struct _LSID_ENTRY   /* (0x0200 + 0x0040*n) */
  {
    REG32 CTRL;         /* (+0x00) [NCORES-1:0]: disallow-coremask, [11:8]: maxcores */
      #define CORESCHED_LSID_CTRL_DISALLOW    (0) /* NCORES */
      #define CORESCHED_LSID_CTRL_MAXCORES    (8) /* max no. of cores that may run this LSID. 0=infinite */
      #define CORESCHED_LSID_CTRL_DISALLOW_SZ (8) /* actually only NCORES bits */
      #define CORESCHED_LSID_CTRL_MAXCORES_SZ (4)

    REG32 MMU_CTRL;     /* (+0x04) Startup pagetable base */
      #define CORESCHED_LSID_MMU_CTRL_TATTR      (30) /* Index 0-3, bus attributes when reading L1 table */
      #define CORESCHED_LSID_MMU_CTRL_TBASE      (2)  /* Upper 28 bits of 40-bit L1 page table address */
      #define CORESCHED_LSID_MMU_CTRL_ENABLE     (1)  /* Should be set to '1' */
      #define CORESCHED_LSID_MMU_CTRL_PROTECT    (0)  /* Should be set to '1' */
      #define CORESCHED_LSID_MMU_CTRL_TATTR_SZ   (2)
      #define CORESCHED_LSID_MMU_CTRL_TBASE_SZ   (28)
      #define CORESCHED_LSID_MMU_CTRL_ENABLE_SZ  (1)
      #define CORESCHED_LSID_MMU_CTRL_PROTECT_SZ (1)

    REG32 NPROT;        /* (+0x08) 0=protected session, 1=non-protected sessions */
    REG32 ALLOC;        /* (+0x0c) Write 1 or 2 to attempt allocation, write 0 to deallocate.
                           Write 1 allocates a non-protected session, write 2 allocates a protected session.
                           If ALLOC=0 and 1 or 2 is written then the session is allocated
                           This sets ALLOC=1, NPROT=PPROT[1], STREAMID=PADDR[19:16] and SCHED=0.
                           If ALLOC=1 or 2 and 0 is written then the session is deallocated
                           This sets ALLOC=0, NPROT=1, STREAMID=0 and SCHED=0. */
    REG32 FLUSH_ALL;    /* (+0x10) initiate mmu flush all for all cores */
    REG32 SCHED;        /* (+0x14) 1 if LSID is available for scheduling */
    REG32 TERMINATE;    /* (+0x18) Write 1 to reset all cores, poll 0 for completion */
    REG32 IRQVE;        /* (+0x1c) Signal IRQ to host (from VE) */
    REG32 IRQHOST;      /* (+0x20) Signal IRQ to VE (from host) */
    REG32 INTSIG;       /* (+0x24) Internal IRQ signals between cores */
    REG32 _pad[1];      /* (+0x28) reserved */
    REG32 STREAMID;     /* (+0x2c) STREAMID value for protected pages */
    REG32 BUSATTR[4];   /* (+0x30) Bus attribute registers, holding four different configurations
                                   corresponding to the four ATTR values in the page tables */
      #define CORESCHED_BUSATTR_ARCACHE       (0)  /* Value used for external AXI ARCACHE, default 0011 */
      #define CORESCHED_BUSATTR_AWCACHE       (4)  /* Value used for external AXI AWCACHE, default 0011 */
      #define CORESCHED_BUSATTR_ARDOMAIN      (8)  /* Value used for external AXI ARDOMAIN, default 00 */
      #define CORESCHED_BUSATTR_AWDOMAIN      (10) /* Value used for external AXI AWDOMAIN, default 00 */
      #define CORESCHED_BUSATTR_ARCACHE_SZ    (4)
      #define CORESCHED_BUSATTR_AWCACHE_SZ    (4)
      #define CORESCHED_BUSATTR_ARDOMAIN_SZ   (2)
      #define CORESCHED_BUSATTR_AWDOMAIN_SZ   (2)
                        
  } LSID[8];

  REG32 _pad3[8192-256];   /* (0x0400) pad to 32 kB */

  REG32 SESSIONRAM[4*SESSIONRAM_SIZE_PER_LSID/4];  /* (0x8000) NLSID * 8kB of session RAM */

} tCS;

#endif /* __EMUL_INCLUDE__MVE_CORESCHED_REG_H__ */
