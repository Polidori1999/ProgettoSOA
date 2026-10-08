#include <linux/spinlock.h>
#include <linux/types.h>

#include "accounting.h"

/*
 * Protegge il numero globale di syscall giÃ  ammesse
 * nella finestra corrente.
 * Il timer puÃ² azzerarlo mentre altri CPU tentano di riservare un posto;
 * irqsave evita anche interferenze con il callback sulla stessa CPU.
 */
static DEFINE_RAW_SPINLOCK(accounting_lock);

/*
 * Unico contatore globale condiviso da tutte le syscall,
 * tutti i programmi e tutti gli effective UID monitorati.
 */
static __u32 window_count;


void syscall_throttle_accounting_reset(void)
{
    unsigned long flags;

    raw_spin_lock_irqsave(
        &accounting_lock,
        flags
    );

    window_count = 0;

    raw_spin_unlock_irqrestore(
        &accounting_lock,
        flags
    );
}


void syscall_throttle_accounting_record(
    __u32 max,
    struct syscall_throttle_accounting_result *result)
{
    unsigned long flags;

    if (result == NULL)
        return;

    raw_spin_lock_irqsave(
        &accounting_lock,
        flags
    );

    result->max = max;

    /* Verifica e prenotazione sono atomiche rispetto agli altri thread. */
    result->exceeded = window_count >= max;
    if (!result->exceeded)
        ++window_count;

    /* Conta solo le ammissioni: i tentativi senza quota non consumano posti. */
    result->count = window_count;

    raw_spin_unlock_irqrestore(
        &accounting_lock,
        flags
    );
}