#
#
# (C) COPYRIGHT ARM Limited. All rights reserved.
#
# This program is free software and is provided to you under the terms of the
# GNU General Public License version 2 as published by the Free Software
# Foundation, and any use by you of this program is subject to the terms
# of such GNU licence.
#
# A copy of the licence is included with the program, and can also be obtained
# from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
#

obj-y := mve_rsrc.o

mve_rsrc-y := mve_rsrc_driver.o \
	mve_rsrc_mem_frontend.o \
	mve_rsrc_mem_cache.o \
	mve_rsrc_mem_backend.o \
	mve_rsrc_register.o \
	mve_rsrc_irq.o \
	mve_rsrc_scheduler.o \
	mve_rsrc_dvfs.o \
	mve_rsrc_pm.o \
	mve_rsrc_circular_buffer.o \
	mve_rsrc_mem_dma.o \
	mve_rsrc_mem_dma_uncached.o \
	mve_rsrc_log.o \
	machine/mve_config.o \
        machine/board-vexpress.o

# Only build machine/board-vexpress.c when building for one of the FPGA platforms
#mve_rsrc-$(CONFIG_MALI_VPU6_VEX6) += machine/board-vexpress.o
#mve_rsrc-$(CONFIG_MALI_VPU6_VEX7) += machine/board-vexpress.o
#mve_rsrc-$(CONFIG_MALI_VPU6_JUNO) += machine/board-vexpress.o

ccflags-y = -I$(src)/../external/
ccflags-y += -DDISABLE_DVFS -DHW=0
ccflags-$(CONFIG_MALI_VPU6_ENABLE_FTRACE) += -DMVE_LOG_FTRACE_ENABLE
ccflags-$(CONFIG_MALI_VPU6_ENABLE_ALOG) += -DMVE_LOG_ALOG_ENABLE
ccflags-$(CONFIG_MALI_VPU6_ENABLE_PRINT_FILE) += -DMVE_LOG_PRINT_FILE_ENABLE
ccflags-$(CONFIG_MALI_VPU6_ENABLE_DVFS_SIM) += -DENABLE_DVFS_FREQ_SIM

# If debug is enabled, disable function inlining to allow ftrace to give a more detailed picture of the executed functions
ccflags-$(CONFIG_MALI_VPU6_DEBUG) += -D_DEBUG -fno-inline
ccflags-$(CONFIG_MALI_VPU6_UNIT) += -DUNIT
ccflags-$(CONFIG_MALI_VPU6_POWER_SAVING_MODE_CLOCK_GATING) += -DSCHEDULER_MODE_IDLE_SWITCHOUT -DDISABLE_DVFS

ifeq ($(CONFIG_MALI_VPU6_TRACKMEM), y)
	ccflags-y += -DMVE_MEM_DBG_TRACKMEM=1
	MVE_MEM_DEBUG=y
else
	ccflags-y += -DMVE_MEM_DBG_TRACKMEM=0
endif

ifeq ($(CONFIG_MALI_VPU6_RESFAIL), y)
	ccflags-y += -DMVE_MEM_DBG_RESFAIL=1
	MVE_MEM_DEBUG=y
else
	ccflags-y += -DMVE_MEM_DBG_RESFAIL=0
endif

ifeq ($(MVE_MEM_DEBUG), y)
	ccflags-y += -DMVE_MEM_DBG_SUPPORT=1
else
	ccflags-y += -DMVE_MEM_DBG_SUPPORT=0
endif

# Always switch out on idleness
ccflags-y += -DSCHEDULER_MODE_IDLE_SWITCHOUT=1

ccflags-$(CONFIG_MALI_VPU6_VEX6) += -DHW=0
ccflags-$(CONFIG_MALI_VPU6_VEX7) += -DHW=1
ccflags-$(CONFIG_MALI_VPU6_JUNO) += -DHW=2
