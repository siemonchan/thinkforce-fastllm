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


obj-m := mve_driver6.o
mve_driver6-y := \
		base/mve_command.o \
		base/mve_session.o \
		base/mve_session_buffer.o \
		base/mve_mmu.o \
		base/mve_mem_region.o \
		base/mve_fw.o \
		base/mve_com.o \
		base/mve_queue.o \
		base/mve_com.o \
		base/mve_com_host_interface_v2.o \
		base/mve_com_host_interface_v3.o \
		base/mve_buffer_common.o \
		base/mve_buffer_valloc.o \
		base/mve_buffer_attachment.o \
		base/mve_buffer_dmabuf.o \
		base/mve_buffer_ashmem.o \
        resource/mve_rsrc_driver.o \
        resource/mve_rsrc_mem_frontend.o \
	resource/mve_rsrc_mem_cache.o \
	resource/mve_rsrc_mem_backend.o \
	resource/mve_rsrc_register.o \
	resource/mve_rsrc_irq.o \
	resource/mve_rsrc_scheduler.o \
	resource/mve_rsrc_dvfs.o \
	resource/mve_rsrc_pm.o \
	resource/mve_rsrc_circular_buffer.o \
	resource/mve_rsrc_mem_dma.o \
	resource/mve_rsrc_mem_dma_uncached.o \
	resource/mve_rsrc_log.o \
	resource/machine/mve_config.o \
        resource/machine/board-vexpress.o


ccflags-y += -I$(src)/external/ -I$(src)/resource -I$(src)/base
ccflags-y += -DDISABLE_DVFS -DHW=0
ccflags-y += -DMVE_MEM_DBG_TRACKMEM=0
ccflags-y += -DMVE_MEM_DBG_RESFAIL=0
ccflags-y += -DMVE_MEM_DBG_SUPPORT=0
ccflags-y += -DSCHEDULER_MODE_IDLE_SWITCHOUT=1
ccflags-y += -DUNIT

KERNEL_VERSION=$(shell uname -r)

ifeq ($(LINUX_VERSION),Ubuntu)
	KDIR=/usr/src/linux-headers-$(KERNEL_VERSION)
else
	KDIR=/usr/src/kernels/$(KERNEL_VERSION)/
endif

all: android

android:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64  modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) HW=1 clean

.PHONY: all android

