#ifndef SYSCALL_THROTTLE_ACCOUNTING_H
#define SYSCALL_THROTTLE_ACCOUNTING_H

#include <linux/types.h>

/*
 * Risultato del tentativo di riservare un posto nella
 * finestra temporale globale corrente.
 */
struct syscall_throttle_accounting_result {
    /*
     * Numero di syscall già ammesse nella finestra.
     */
    __u32 count;

    /*
     * Limite utilizzato durante la valutazione.
     */
    __u32 max;

    /*
     * True quando non rimangono posti disponibili.
     */
    bool exceeded;
};

/*
 * Azzera il contatore globale della finestra.
 *
 * La funzione non dorme e non alloca memoria.
 */
void syscall_throttle_accounting_reset(void);

/*
 * Prova a riservare un posto nella finestra corrente.
 *
 * La gestione temporale appartiene al timer autonomo
 * del throttle engine.
 */
void syscall_throttle_accounting_record(
    __u32 max,
    struct syscall_throttle_accounting_result *result
);

#endif /* SYSCALL_THROTTLE_ACCOUNTING_H */
