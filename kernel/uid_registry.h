#ifndef SYSCALL_THROTTLE_UID_REGISTRY_H
#define SYSCALL_THROTTLE_UID_REGISTRY_H

#include <linux/types.h>
#include <linux/uidgid.h>

/*
 * Funzioni usate dal control plane tramite ioctl.
 */
long syscall_throttle_uid_register(unsigned long arg);
long syscall_throttle_uid_unregister(unsigned long arg);
long syscall_throttle_uid_get_list(unsigned long arg);

/*
 * Funzione usata dal futuro data plane.
 *
 * La lettura avviene tramite RCU e non acquisisce mutex.
 */
bool syscall_throttle_uid_matches(kuid_t uid);

/*
 * Libera lo snapshot RCU durante la rimozione del modulo.
 */
void syscall_throttle_uid_registry_cleanup(void);

#endif /* SYSCALL_THROTTLE_UID_REGISTRY_H */