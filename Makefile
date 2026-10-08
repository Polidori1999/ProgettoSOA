KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)/kernel
MODULE_NAME := syscall_throttle

CC := gcc
USER_BUILD_DIR := $(CURDIR)/build
CONTROLLER := $(USER_BUILD_DIR)/syscall_throttle_ctl

USER_SOURCES := \
	user/main.c \
	user/controller.c

USER_OBJECTS := \
	$(USER_BUILD_DIR)/main.o \
	$(USER_BUILD_DIR)/controller.o

CPPFLAGS := -I$(CURDIR)/include
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -O2

.PHONY: all module user \
	rebuild clean clean-module clean-user \
	load unload reload logs controller \
	demo1 demo2 demo3 demo4 demo5 demo6 demo-all

all: module user

#
# Modulo kernel
#
module:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) modules

#
# Controller user-space
#
user: $(CONTROLLER)

$(USER_BUILD_DIR):
	mkdir -p $(USER_BUILD_DIR)

$(USER_BUILD_DIR)/main.o: user/main.c user/controller.h \
                         include/syscall_throttle_ioctl.h \
                         | $(USER_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(USER_BUILD_DIR)/controller.o: user/controller.c user/controller.h \
                               include/syscall_throttle_ioctl.h \
                               | $(USER_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(CONTROLLER): $(USER_OBJECTS)
	$(CC) $(USER_OBJECTS) -o $(CONTROLLER)

#
# Pulizia e ricompilazione
#
rebuild: clean all

clean: clean-module clean-user

clean-module:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) clean

clean-user:
	rm -rf $(USER_BUILD_DIR)

#
# Gestione modulo
#
load: module
	sudo insmod $(MODULE_DIR)/$(MODULE_NAME).ko

unload:
	@if lsmod | grep -q '^$(MODULE_NAME)'; then \
		sudo rmmod $(MODULE_NAME); \
	else \
		echo "Modulo $(MODULE_NAME) non caricato"; \
	fi

reload: unload load

logs:
	sudo dmesg -T | grep 'syscall_throttle' | tail -n 30

#
# Controller
#
controller: user
	 $(CONTROLLER) $(ARGS)

#
# Demo
#
demo1:
	./demos/run_demo1.sh

demo2:
	./demos/run_demo2.sh

demo3:
	./demos/run_demo3.sh

demo4:
	./demos/run_demo4.sh

demo5:
	./demos/run_demo5.sh

demo6:
	./demos/run_demo6.sh

demo-all:
	$(MAKE) demo1
	$(MAKE) demo2
	$(MAKE) demo3
	$(MAKE) demo4
	$(MAKE) demo5
	$(MAKE) demo6