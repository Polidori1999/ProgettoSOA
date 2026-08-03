#include <linux/spinlock.h>
#include <linux/types.h>

#include "accounting.h"

/*
 * Protegge il numero globale di syscall già ammesse
 * nella finestra corrente.
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

    if (window_count < max) {
        ++window_count;

        result->count = window_count;
        result->exceeded = false;
    } else {
        result->count = window_count;
        result->exceeded = true;
    }

    raw_spin_unlock_irqrestore(
        &accounting_lock,
        flags
    );
}
