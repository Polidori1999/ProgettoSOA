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
 *
 * _IOW: il dato viene scritto dallo user-space
 * verso il kernel.
 */
#define SYSCALL_THROTTLE_IOC_SET_MAX \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 1, __u32)

/*
 * Legge il valore corrente di MAX.
 *
 * _IOR: il dato viene letto dallo user-space
 * e scritto dal kernel.
 */
#define SYSCALL_THROTTLE_IOC_GET_MAX \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 2, __u32)

#endif
