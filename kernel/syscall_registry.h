#ifndef SYSCALL_THROTTLE_SYSCALL_REGISTRY_H
#define SYSCALL_THROTTLE_SYSCALL_REGISTRY_H

#include <linux/types.h>

/*
 * Gestione degli ioctl relativi ai numeri di syscall.
 */
long syscall_throttle_syscall_register(unsigned long arg);
long syscall_throttle_syscall_unregister(unsigned long arg);
long syscall_throttle_syscall_get_list(unsigned long arg);

/*
 * Funzione destinata al futuro data plane.
 *
 * Permetterà al monitor di controllare rapidamente
 * se una syscall è registrata.
 */
bool syscall_throttle_syscall_matches(unsigned int syscall_nr);

#endif /* SYSCALL_THROTTLE_SYSCALL_REGISTRY_H */