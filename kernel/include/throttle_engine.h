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
/*
 * Verifica se la syscall corrente è sottoposta al
 * monitoraggio e aggiorna l'accounting.
 */
bool syscall_throttle_engine_evaluate(
    long syscall_id,
    struct syscall_throttle_decision *decision
);


/*
 * Applica il controllo completo alla syscall corrente.
 *
 * Restituisce:
 *
 *  1:
 *      syscall controllata e ammessa;
 *
 *  0:
 *      syscall non controllata, monitor disattivato
 *      oppure motore in shutdown;
 *
 * -ERESTARTSYS:
 *      attesa interrotta da un segnale.
 *
 * Quando la finestra è piena, la funzione sospende il
 * task fino alla sua scadenza e ripete l'accounting.
 */
int syscall_throttle_engine_enforce(
    long syscall_id,
    struct syscall_throttle_decision *decision
);


/*
 * Risveglia i thread quando il monitor viene
 * disattivato.
 */
void syscall_throttle_engine_monitor_disabled(void);

/*
 * Impedisce nuove attese e risveglia i thread durante
 * la rimozione del modulo.
 */
void syscall_throttle_engine_shutdown(void);

#endif /* SYSCALL_THROTTLE_ENGINE_H */