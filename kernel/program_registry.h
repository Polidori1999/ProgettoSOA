#ifndef SYSCALL_THROTTLE_PROGRAM_REGISTRY_H
#define SYSCALL_THROTTLE_PROGRAM_REGISTRY_H

/*
 * Gestione degli ioctl relativi ai nomi
 * dei programmi registrati.
 */
long syscall_throttle_program_register(unsigned long arg);
long syscall_throttle_program_unregister(unsigned long arg);
long syscall_throttle_program_get_list(unsigned long arg);

#endif /* SYSCALL_THROTTLE_PROGRAM_REGISTRY_H */