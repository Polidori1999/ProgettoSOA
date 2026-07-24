KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)/kernel
MODULE_NAME := syscall_throttle

.PHONY: all module rebuild clean load unload reload logs

all: module

module:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) modules

rebuild: clean module

clean:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) clean

load: module
	sudo insmod $(MODULE_DIR)/$(MODULE_NAME).ko

unload:
	-sudo rmmod $(MODULE_NAME)

reload: unload load

logs:
	sudo dmesg -T | grep 'syscall_throttle' | tail -n 30