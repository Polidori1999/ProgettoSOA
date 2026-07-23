#ifndef SYSCALL_THROTTLE_ACCOUNTING_H
#define SYSCALL_THROTTLE_ACCOUNTING_H

#include <linux/types.h>

/*
 * Risultato dell'aggiornamento del contatore.
 */
struct syscall_throttle_accounting_result {
    __u32 count;
    __u32 max;

    bool exceeded;
    bool new_window;
};

/*
 * Azzera la finestra e il contatore.
 */
void syscall_throttle_accounting_reset(void);

/*
 * Registra una syscall critica.
 *
 * Questa funzione:
 *
 * - non dorme;
 * - non alloca memoria;
 * - può essere chiamata dal tracepoint;
 * - protegge lo stato globale tra più CPU.
 */
void syscall_throttle_accounting_record(
    __u32 max,
    struct syscall_throttle_accounting_result *result
);

#endif /* SYSCALL_THROTTLE_ACCOUNTING_H */