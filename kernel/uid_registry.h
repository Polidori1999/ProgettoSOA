#ifndef SYSCALL_THROTTLE_UID_REGISTRY_H
#define SYSCALL_THROTTLE_UID_REGISTRY_H

/*
 * Gestione degli ioctl relativi agli UID.
 */
long syscall_throttle_uid_register(unsigned long arg);
long syscall_throttle_uid_unregister(unsigned long arg);
long syscall_throttle_uid_get_list(unsigned long arg);

#endif /* SYSCALL_THROTTLE_UID_REGISTRY_H */