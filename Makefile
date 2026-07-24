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

.PHONY: all module user rebuild clean clean-module clean-user \
	load unload reload logs controller test

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
	sudo $(CONTROLLER) $(ARGS)

test: user
	sudo $(CONTROLLER) ping
	sudo $(CONTROLLER) get-max
	sudo $(CONTROLLER) monitor-status