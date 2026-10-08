KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)/kernel
MODULE_NAME := syscall_throttle

CC := gcc
USER_BUILD_DIR := $(CURDIR)/build
CONTROLLER := $(USER_BUILD_DIR)/syscall_throttle_ctl

TEST_BUILD_DIR := $(USER_BUILD_DIR)/tests
TEST_COMMON_OBJECT := $(TEST_BUILD_DIR)/test_common.o
TEST_CONTROL_PLANE := $(TEST_BUILD_DIR)/test_control_plane
TEST_REGISTRIES := $(TEST_BUILD_DIR)/test_registries
TEST_THROTTLING := $(TEST_BUILD_DIR)/test_throttling
TEST_CONCURRENCY := $(TEST_BUILD_DIR)/test_concurrency
TEST_REVALIDATION := $(TEST_BUILD_DIR)/test_revalidation

USER_SOURCES := \
	user/main.c \
	user/controller.c

USER_OBJECTS := \
	$(USER_BUILD_DIR)/main.o \
	$(USER_BUILD_DIR)/controller.o

CPPFLAGS := -I$(CURDIR)/include
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -O2

TEST_CPPFLAGS := $(CPPFLAGS) -I$(CURDIR)/tests/include

.PHONY: all module user tests \
	test-control-plane test-registries test-throttling \
	test-concurrency test-revalidation test \
	rebuild clean clean-module clean-user \
	load unload reload logs controller \
	demo1 demo2 demo3 demo4 demo5 demo-all

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
# Programmi di test user-space
#
tests: $(TEST_CONTROL_PLANE) $(TEST_REGISTRIES) $(TEST_THROTTLING) \
       $(TEST_CONCURRENCY) $(TEST_REVALIDATION)

test-control-plane: module $(TEST_CONTROL_PLANE)
	./tests/run_test.sh $(TEST_CONTROL_PLANE)

test-registries: module $(TEST_REGISTRIES)
	./tests/run_test.sh $(TEST_REGISTRIES)

test-throttling: module $(TEST_THROTTLING)
	./tests/run_test.sh $(TEST_THROTTLING)

test-concurrency: module $(TEST_CONCURRENCY)
	./tests/run_test.sh $(TEST_CONCURRENCY)

test-revalidation: module $(TEST_REVALIDATION)
	./tests/run_test.sh $(TEST_REVALIDATION)

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

$(TEST_COMMON_OBJECT): tests/test_common.c \
                       tests/include/test_common.h \
                       | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/test_control_plane.o: \
        tests/test_control_plane.c \
        tests/include/test_common.h \
        include/syscall_throttle_ioctl.h \
        | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_CONTROL_PLANE): \
        $(TEST_BUILD_DIR)/test_control_plane.o \
        $(TEST_COMMON_OBJECT)
	$(CC) $^ -o $@

$(TEST_BUILD_DIR)/test_registries.o: \
        tests/test_registries.c \
        tests/include/test_common.h \
        include/syscall_throttle_ioctl.h \
        | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_REGISTRIES): \
        $(TEST_BUILD_DIR)/test_registries.o \
        $(TEST_COMMON_OBJECT)
	$(CC) $^ -o $@

$(TEST_BUILD_DIR)/test_throttling.o: \
        tests/test_throttling.c \
        tests/include/test_common.h \
        include/syscall_throttle_ioctl.h \
        | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_THROTTLING): \
        $(TEST_BUILD_DIR)/test_throttling.o \
        $(TEST_COMMON_OBJECT)
	$(CC) $^ -o $@

$(TEST_BUILD_DIR)/test_concurrency.o: \
        tests/test_concurrency.c \
        tests/include/test_common.h \
        include/syscall_throttle_ioctl.h \
        | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -pthread -c $< -o $@

$(TEST_CONCURRENCY): \
        $(TEST_BUILD_DIR)/test_concurrency.o \
        $(TEST_COMMON_OBJECT)
	$(CC) $^ -pthread -o $@

$(TEST_BUILD_DIR)/test_revalidation.o: \
        tests/test_revalidation.c \
        tests/include/test_common.h \
        include/syscall_throttle_ioctl.h \
        | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -pthread -c $< -o $@

$(TEST_REVALIDATION): \
        $(TEST_BUILD_DIR)/test_revalidation.o \
        $(TEST_COMMON_OBJECT)
	$(CC) $^ -pthread -o $@

#
# Suite completa di test
#
test:
	$(MAKE) test-control-plane
	$(MAKE) test-registries
	$(MAKE) test-throttling
	$(MAKE) test-concurrency
	$(MAKE) test-revalidation

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

demo-all:
	$(MAKE) demo1
	$(MAKE) demo2
	$(MAKE) demo3
	$(MAKE) demo4
	$(MAKE) demo5