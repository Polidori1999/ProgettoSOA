#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>
#include <linux/cred.h>

#include <syscall_throttle_ioctl.h>

#include "statistics.h"

/*
 * Protegge tutto lo stato statistico globale.
 *
 * Le sezioni critiche sono brevi e non contengono
 * operazioni bloccanti.
 */
static DEFINE_SPINLOCK(statistics_lock);

/*
 * Numero di task attualmente bloccati dal throttler.
 */
static __u32 current_blocked_threads;

/*
 * Massimo numero simultaneo di task bloccati.
 */
static __u32 peak_blocked_threads;

/*
 * Ultimo istante in cui current_blocked_threads
 * è cambiato.
 */
static u64 last_change_ns;

/*
 * Tempo complessivo durante il quale almeno un task
 * è rimasto bloccato.
 */
static u64 active_blocking_time_ns;

/*
 * Integrale temporale del numero di task bloccati:
 *
 * current_blocked_threads * intervallo temporale.
 */
static u64 weighted_blocking_time_ns;

/*
 * Massimo ritardo osservato prima dell'esecuzione
 * effettiva di una syscall.
 */
static u64 peak_delay_ns;
static kuid_t peak_delay_uid;
static char peak_delay_program[
    SYSCALL_THROTTLE_PROGRAM_NAME_LEN
];
static bool peak_delay_valid;


/*
 * Aggiorna gli integrali temporali fino a now_ns.
 *
 * La funzione deve essere chiamata con
 * statistics_lock già acquisito.
 */
static void syscall_throttle_statistics_update_time(
    u64 now_ns)
{
    u64 elapsed_ns;

    if (last_change_ns == 0) {
        last_change_ns = now_ns;
        return;
    }

    elapsed_ns = now_ns - last_change_ns;

    if (current_blocked_threads > 0) {
        active_blocking_time_ns += elapsed_ns;

        weighted_blocking_time_ns +=
            elapsed_ns * current_blocked_threads;
    }

    last_change_ns = now_ns;
}


u64 syscall_throttle_statistics_block_enter(void)
{
    unsigned long flags;
    u64 now_ns;

    now_ns = ktime_get_ns();

    spin_lock_irqsave(
        &statistics_lock,
        flags
    );

    syscall_throttle_statistics_update_time(now_ns);

    ++current_blocked_threads;

    if (current_blocked_threads >
        peak_blocked_threads) {
        peak_blocked_threads =
            current_blocked_threads;
    }

    spin_unlock_irqrestore(
        &statistics_lock,
        flags
    );

    return now_ns;
}


void syscall_throttle_statistics_block_exit(
    u64 blocked_since_ns,
    bool syscall_will_execute,
    kuid_t effective_uid,
    const char *program_name)
{
    unsigned long flags;
    u64 delay_ns;
    u64 now_ns;

    now_ns = ktime_get_ns();

    if (now_ns >= blocked_since_ns)
        delay_ns = now_ns - blocked_since_ns;
    else
        delay_ns = 0;

    spin_lock_irqsave(
        &statistics_lock,
        flags
    );

    syscall_throttle_statistics_update_time(now_ns);

    if (current_blocked_threads > 0)
        --current_blocked_threads;

    if (syscall_will_execute &&
        (!peak_delay_valid ||
         delay_ns > peak_delay_ns)) {

        peak_delay_ns = delay_ns;
        peak_delay_uid = effective_uid;
        peak_delay_valid = true;

        strscpy(
            peak_delay_program,
            program_name,
            sizeof(peak_delay_program)
        );
    }

    spin_unlock_irqrestore(
        &statistics_lock,
        flags
    );
}


long syscall_throttle_statistics_get(unsigned long arg)
{
    struct syscall_throttle_statistics snapshot;
    unsigned long flags;
    u64 now_ns;

    memset(&snapshot, 0, sizeof(snapshot));

    now_ns = ktime_get_ns();

    spin_lock_irqsave(
        &statistics_lock,
        flags
    );

    /*
     * Include nello snapshot anche il tempo trascorso
     * dall'ultima variazione del numero di bloccati.
     */
    syscall_throttle_statistics_update_time(now_ns);

    snapshot.active_blocking_time_ns =
        active_blocking_time_ns;

    snapshot.weighted_blocking_time_ns =
        weighted_blocking_time_ns;

    snapshot.peak_delay_ns =
        peak_delay_ns;

    snapshot.current_blocked_threads =
        current_blocked_threads;

    snapshot.peak_blocked_threads =
        peak_blocked_threads;

    snapshot.peak_delay_valid =
        peak_delay_valid ? 1 : 0;

    if (peak_delay_valid) {
        snapshot.peak_delay_uid =
            from_kuid_munged(
                current_user_ns(),
                peak_delay_uid
            );

        strscpy(
            snapshot.peak_delay_program,
            peak_delay_program,
            sizeof(snapshot.peak_delay_program)
        );
    }

    spin_unlock_irqrestore(
        &statistics_lock,
        flags
    );

    if (copy_to_user(
            (void __user *)arg,
            &snapshot,
            sizeof(snapshot)) != 0) {
        return -EFAULT;
    }

    return 0;
}