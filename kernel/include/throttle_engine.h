#ifndef SYSCALL_THROTTLE_ENGINE_H
#define SYSCALL_THROTTLE_ENGINE_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#include "accounting.h"

/*
 * Risultato della valutazione di una syscall.
 */
struct syscall_throttle_decision {
    long syscall_id;

    kuid_t effective_uid;
    char program_name[TASK_COMM_LEN];

    bool uid_match;
    bool program_match;

    struct syscall_throttle_accounting_result accounting;
};

/*
 * Inizializza il timer globale delle finestre.
 */
void syscall_throttle_engine_init(void);

/*
 * Avvia una nuova sequenza di finestre periodiche.
 */
void syscall_throttle_engine_monitor_enabled(void);

/*
 * Verifica nuovamente monitor, syscall, programma ed
 * effective UID e prova a riservare un posto globale.
 */
bool syscall_throttle_engine_evaluate(
    long syscall_id,
    struct syscall_throttle_decision *decision
);

/*
 * Applica il throttling completo.
 *
 * Restituisce:
 *
 *  1: syscall monitorata e ammessa;
 *  0: syscall non monitorata, monitor-off o shutdown;
 * -ERESTARTSYS: attesa interrotta da un segnale.
 */
int syscall_throttle_engine_enforce(
    long syscall_id,
    struct syscall_throttle_decision *decision
);

/*
 * Arresta il timer e risveglia i waiter.
 */
void syscall_throttle_engine_monitor_disabled(void);

/*
 * Impedisce nuove attese durante l'unload.
 */
void syscall_throttle_engine_shutdown(void);

#endif /* SYSCALL_THROTTLE_ENGINE_H */
