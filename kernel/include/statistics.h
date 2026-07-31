#ifndef SYSCALL_THROTTLE_STATISTICS_H
#define SYSCALL_THROTTLE_STATISTICS_H

#include <linux/types.h>
#include <linux/uidgid.h>

/*
 * Registra una transizione dello stato del monitor.
 *
 * enabled=true:
 *     inizia un intervallo di osservazione attivo.
 *
 * enabled=false:
 *     conclude l'intervallo di osservazione corrente.
 */
void syscall_throttle_statistics_monitor_state_changed(
    bool enabled
);

/*
 * Registra il primo ingresso effettivo di un task
 * nello stato bloccato.
 *
 * Restituisce il timestamp monotono di inizio attesa.
 */
u64 syscall_throttle_statistics_block_enter(void);

/*
 * Registra l'uscita definitiva di un task dallo stato
 * bloccato.
 *
 * syscall_will_execute indica se, dopo l'attesa,
 * l'implementazione originale della syscall verrà
 * effettivamente invocata.
 */
void syscall_throttle_statistics_block_exit(
    u64 blocked_since_ns,
    bool syscall_will_execute,
    kuid_t effective_uid,
    const char *program_name
);

/*
 * Copia in user-space uno snapshot coerente delle
 * statistiche globali.
 */
long syscall_throttle_statistics_get(unsigned long arg);

#endif /* SYSCALL_THROTTLE_STATISTICS_H */