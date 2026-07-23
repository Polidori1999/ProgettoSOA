#include <linux/spinlock.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

#include "accounting.h"

/*
 * Protegge:
 *
 * - inizio della finestra corrente;
 * - numero di syscall critiche nella finestra.
 *
 * Usiamo un raw spinlock perché la funzione viene
 * richiamata dal tracepoint e non deve mai dormire.
 *
 * La sezione critica è molto breve:
 * nessuna allocazione, nessun log, nessuna copia user.
 */
static DEFINE_RAW_SPINLOCK(accounting_lock);

/*
 * Istante della prima syscall critica della finestra.
 *
 * Il valore è espresso in nanosecondi monotoni.
 * Zero indica che non esiste ancora una finestra.
 */
static u64 window_start_ns;

/*
 * Numero di syscall critiche osservate nella finestra.
 */
static __u32 window_count;

void syscall_throttle_accounting_reset(void)
{
    unsigned long flags;

    raw_spin_lock_irqsave(
        &accounting_lock,
        flags
    );

    window_start_ns = 0;
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
    u64 now_ns;
    bool new_window;

    if (result == NULL)
        return;

    /*
     * Leggiamo il tempo prima di acquisire il lock.
     */
    now_ns = ktime_get_ns();
    new_window = false;

    raw_spin_lock_irqsave(
        &accounting_lock,
        flags
    );

    /*
     * La prima syscall crea una nuova finestra.
     *
     * Quando è trascorso almeno un secondo, la syscall
     * corrente diventa il primo evento della nuova
     * finestra.
     */
    if (window_start_ns == 0 ||
        now_ns - window_start_ns >= NSEC_PER_SEC) {
        window_start_ns = now_ns;
        window_count = 0;
        new_window = true;
    }

    ++window_count;

    result->count = window_count;
    result->max = max;
    result->exceeded = window_count > max;
    result->new_window = new_window;

    raw_spin_unlock_irqrestore(
        &accounting_lock,
        flags
    );
}