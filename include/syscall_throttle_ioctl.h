#ifndef SYSCALL_THROTTLE_IOCTL_H
#define SYSCALL_THROTTLE_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * Identificatore della famiglia ioctl del driver.
 */
#define SYSCALL_THROTTLE_IOC_MAGIC 'T'

/*
 * Comando di test senza trasferimento di dati.
 */
#define SYSCALL_THROTTLE_IOC_PING \
    _IO(SYSCALL_THROTTLE_IOC_MAGIC, 0)

/*
 * Imposta il numero massimo di syscall consentite
 * nella finestra temporale di un secondo.
 */
#define SYSCALL_THROTTLE_IOC_SET_MAX \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 1, __u32)

/*
 * Legge il valore corrente di MAX.
 */
#define SYSCALL_THROTTLE_IOC_GET_MAX \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 2, __u32)

/*
 * Attiva il monitor.
 */
#define SYSCALL_THROTTLE_IOC_ENABLE_MONITOR \
    _IO(SYSCALL_THROTTLE_IOC_MAGIC, 3)

/*
 * Disattiva il monitor.
 */
#define SYSCALL_THROTTLE_IOC_DISABLE_MONITOR \
    _IO(SYSCALL_THROTTLE_IOC_MAGIC, 4)

/*
 * Legge lo stato corrente del monitor:
 *     0 = disattivato
 *     1 = attivato
 */
#define SYSCALL_THROTTLE_IOC_GET_MONITOR \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 5, __u32)

/*
 * Numero massimo di UID registrabili.
 */
#define SYSCALL_THROTTLE_MAX_REGISTERED_UIDS 64

/*
 * Snapshot degli UID registrati.
 */
struct syscall_throttle_uid_list {
    __u32 count;
    __u32 uids[SYSCALL_THROTTLE_MAX_REGISTERED_UIDS];
};

/*
 * Registra un UID.
 */
#define SYSCALL_THROTTLE_IOC_REGISTER_UID \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 6, __u32)

/*
 * Deregistra un UID.
 */
#define SYSCALL_THROTTLE_IOC_UNREGISTER_UID \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 7, __u32)

/*
 * Restituisce lo snapshot degli UID registrati.
 */
#define SYSCALL_THROTTLE_IOC_GET_UIDS \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 8, \
         struct syscall_throttle_uid_list)

#endif /* SYSCALL_THROTTLE_IOCTL_H */