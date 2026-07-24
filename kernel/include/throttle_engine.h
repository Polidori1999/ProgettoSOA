#ifndef SYSCALL_THROTTLE_ENGINE_H
#define SYSCALL_THROTTLE_ENGINE_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#include "accounting.h"

/*
 * Risultato della valutazione di una syscall.
 *
 * Questa struttura contiene tutte le informazioni
 * che serviranno sia al monitor temporaneo sia al
 * futuro dispatcher.
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
 * Verifica se la syscall corrente è sottoposta al
 * monitoraggio e, in caso positivo, aggiorna il
 * contatore globale.
 *
 * Restituisce:
 *
 * true:
 *     syscall registrata e matching positivo;
 *     il campo decision contiene il risultato.
 *
 * false:
 *     syscall non soggetta al controllo.
 *
 * La funzione non dorme e non alloca memoria.
 */
bool syscall_throttle_engine_evaluate(
    long syscall_id,
    struct syscall_throttle_decision *decision
);

#endif