#ifndef SYSCALL_THROTTLE_PROGRAM_REGISTRY_H
#define SYSCALL_THROTTLE_PROGRAM_REGISTRY_H

#include <linux/types.h>

/*
 * Funzioni del control plane richiamate dagli ioctl.
 */
long syscall_throttle_program_register(unsigned long arg);
long syscall_throttle_program_unregister(unsigned long arg);
long syscall_throttle_program_get_list(unsigned long arg);

/*
 * Funzione del futuro data plane.
 *
 * La lettura usa RCU e non acquisisce mutex.
 */
bool syscall_throttle_program_matches(const char *name);

/*
 * Pulizia dello snapshot durante la rimozione del modulo.
 */
void syscall_throttle_program_registry_cleanup(void);

#endif /* SYSCALL_THROTTLE_PROGRAM_REGISTRY_H */