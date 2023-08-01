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

obj-y := mve_base.o

mve_base-y := mve_driver.o \
		mve_command.o \
		mve_session.o \
		mve_session_buffer.o \
		mve_mmu.o \
		mve_mem_region.o \
		mve_fw.o \
		mve_com.o \
		mve_queue.o \
		mve_com.o \
		mve_com_host_interface_v2.o \
		mve_com_host_interface_v3.o \
		mve_buffer_common.o \
		mve_buffer_valloc.o \
		mve_buffer_attachment.o \
		mve_buffer_dmabuf.o \
		mve_buffer_ashmem.o

ccflags-y += -I$(src)/../external/ -I$(src)/../resource

ccflags-$(CONFIG_MALI_VPU3_ENABLE_PRINT_SESSION) += -DDEBUG_PRINT_SESSION
ccflags-$(CONFIG_MALI_VPU3_ENABLE_PRINT_MESSAGES) += -DDEBUG_PRINT_MESSAGES
ccflags-$(CONFIG_MALI_VPU3_ENABLE_PRINT_FILE) += -DMVE_LOG_PRINT_FILE_ENABLE

ccflags-$(CONFIG_MALI_VPU3_DISABLE_WATCHDOG) += -DDISABLE_WATCHDOG
# If debug is enabled, disable function inlining to allow ftrace to give a more detailed picture of the executed functions
ccflags-$(CONFIG_MALI_VPU3_DEBUG) += -D_DEBUG -fno-inline
ccflags-$(CONFIG_MALI_VPU3_UNIT) += -DUNIT

ifeq ($(CONFIG_MALI_VPU3_TRACKMEM), y)
	ccflags-y += -DMVE_MEM_DBG_TRACKMEM=1
	MVE_MEM_DEBUG=y
else
	ccflags-y += -DMVE_MEM_DBG_TRACKMEM=0
endif

ifeq ($(CONFIG_MALI_VPU3_RESFAIL), y)
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
