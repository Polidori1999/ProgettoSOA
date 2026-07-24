ccflags-y += -I$(src)/../include

obj-m := syscall_throttle.o

syscall_throttle-y := \
    syscall_throttle_main.o \
    config.o \
    uid_registry.o \
    program_registry.o \
    syscall_registry.o \
    accounting.o \
    monitor.o \
    dispatcher_hook.o