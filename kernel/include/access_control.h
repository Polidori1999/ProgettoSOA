#ifndef SYSCALL_THROTTLE_ACCESS_CONTROL_H
#define SYSCALL_THROTTLE_ACCESS_CONTROL_H

#include <linux/cred.h>
#include <linux/types.h>
#include <linux/uidgid.h>

/*
 * Le operazioni di configurazione del modulo sono
 * consentite soltanto a un thread con effective UID 0.
 */
static inline bool syscall_throttle_is_root(void)
{
    return uid_eq(current_euid(), GLOBAL_ROOT_UID);
}

#endif