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







/*
 * Gestione dei nomi dei programmi.
 *
 * Il nome contiene al massimo 15 caratteri,
 * più il terminatore '\0'.
 */
#define SYSCALL_THROTTLE_PROGRAM_NAME_LEN 16
#define SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS 64

/*
 * Nome di un programma trasferibile tra
 * user-space e kernel-space.
 */
struct syscall_throttle_program {
    char name[SYSCALL_THROTTLE_PROGRAM_NAME_LEN];
};

/*
 * Snapshot dei programmi registrati.
 */
struct syscall_throttle_program_list {
    __u32 count;

    struct syscall_throttle_program
        programs[SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS];
};

#define SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 9, \
         struct syscall_throttle_program)

#define SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 10, \
         struct syscall_throttle_program)

#define SYSCALL_THROTTLE_IOC_GET_PROGRAMS \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 11, \
         struct syscall_throttle_program_list)


/*
 * Registro dei numeri di syscall.
 *
 * Il limite riguarda il numero di syscall che possono
 * essere contemporaneamente registrate, non il valore
 * massimo del numero di syscall.
 */
#define SYSCALL_THROTTLE_MAX_REGISTERED_SYSCALLS 1024

/*
 * Snapshot dei numeri di syscall registrati.
 */
struct syscall_throttle_syscall_list {
    __u32 count;

    __u32 numbers[
        SYSCALL_THROTTLE_MAX_REGISTERED_SYSCALLS
    ];
};

/*
 * Registra un numero di syscall.
 */
#define SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 12, __u32)

/*
 * Deregistra un numero di syscall.
 */
#define SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL \
    _IOW(SYSCALL_THROTTLE_IOC_MAGIC, 13, __u32)

/*
 * Restituisce lo snapshot delle syscall registrate.
 */
#define SYSCALL_THROTTLE_IOC_GET_SYSCALLS \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 14, \
         struct syscall_throttle_syscall_list)


/*
 * Statistiche globali del meccanismo di throttling.
 *
 * I valori temporali sono espressi in nanosecondi.
 *
 * La media temporale dei thread bloccati viene
 * calcolata in user-space come:
 *
 * weighted_blocking_time_ns /
 * monitor_enabled_time_ns
 */
struct syscall_throttle_statistics {
    __u64 monitor_enabled_time_ns;
    __u64 weighted_blocking_time_ns;
    __u64 peak_delay_ns;

    __u32 current_blocked_threads;
    __u32 peak_blocked_threads;

    __u32 peak_delay_uid;
    __u32 peak_delay_valid;

    char peak_delay_program[
        SYSCALL_THROTTLE_PROGRAM_NAME_LEN
    ];
};

/*
 * Restituisce uno snapshot delle statistiche globali.
 */
#define SYSCALL_THROTTLE_IOC_GET_STATS \
    _IOR(SYSCALL_THROTTLE_IOC_MAGIC, 15, \
         struct syscall_throttle_statistics)



#endif /* SYSCALL_THROTTLE_IOCTL_H */