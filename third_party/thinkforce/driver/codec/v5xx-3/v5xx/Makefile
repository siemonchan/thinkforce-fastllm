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


obj-y := base/ resource/


#CROSS_COMPILE ?= aarch64-none-elf-
#DEBUG ?= 0

#KDIR = /home/ubuntu/NewNPU40T/linux-5.12

#all: android

#android:
#	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) HW=1 clean

#.PHONY: all android

