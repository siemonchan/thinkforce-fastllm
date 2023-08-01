
obj-m := al_module.o

al_module-y := al_driver.o \
               al_command.o

ccflags-y += -I$(src)

KERNEL_VERSION=$(shell uname -r)

ifeq ($(LINUX_VERSION),Ubuntu)
	KDIR=/usr/src/linux-headers-$(KERNEL_VERSION)
else
	KDIR=/usr/src/kernels/$(KERNEL_VERSION)/
endif

all: module

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64  modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: all module 
