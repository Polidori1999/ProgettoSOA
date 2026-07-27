#ifndef SYSCALL_THROTTLE_ACCOUNTING_H
#define SYSCALL_THROTTLE_ACCOUNTING_H

#include <linux/types.h>

/*
 * Registra una syscall critica.
 *
 * La funzione:
 *
 * - non dorme;
 * - non alloca memoria;
 * - protegge lo stato globale tra più CPU;
 * - restituisce il tempo residuo della finestra
 *   quando il limite è stato superato.
 */
struct syscall_throttle_accounting_result {
    /*
     * Numero di syscall ammesse nella finestra.
     */
    __u32 count;

    __u32 max;

    bool exceeded;
    bool new_window;

    /*
     * Tempo residuo della finestra quando exceeded
     * è true; zero negli altri casi.
     */
    u64 wait_ns;
};
/*
 * Azzera la finestra e il contatore.
 */
void syscall_throttle_accounting_reset(void);

/*
 * Prova a riservare un posto nella finestra corrente.
 *
 * Se il numero di syscall già ammesse è inferiore a
 * MAX, incrementa il contatore e restituisce
 * exceeded=false.
 *
 * Se la finestra è piena, non incrementa il contatore,
 * restituisce exceeded=true e indica in wait_ns il
 * tempo residuo prima della finestra successiva.
 *
 * La funzione non dorme e non alloca memoria.
 */
void syscall_throttle_accounting_record(
    __u32 max,
    struct syscall_throttle_accounting_result *result
);

#endif /* SYSCALL_THROTTLE_ACCOUNTING_H */