#include <asm/unistd.h>

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>

#include "accounting.h"
#include "config.h"
#include "program_registry.h"
#include "syscall_registry.h"
#include "throttle_engine.h"
#include "uid_registry.h"

bool syscall_throttle_engine_evaluate(
    long syscall_id,
    struct syscall_throttle_decision *decision)
{
    if (decision == NULL)
        return false;

    /*
     * Evita che il chiamante possa osservare valori
     * rimasti da una valutazione precedente.
     */
    memset(decision, 0, sizeof(*decision));

    /*
     * Se il monitor è disattivato, la syscall non
     * deve essere verificata né conteggiata.
     */
    if (!syscall_throttle_config_monitor_enabled())
        return false;

    /*
     * Accettiamo soltanto numeri validi per la ABI
     * x86-64 nativa.
     */
    if (syscall_id < 0 || syscall_id >= NR_syscalls)
        return false;

    /*
     * La bitmap delle syscall è il controllo più
     * economico, quindi viene eseguito per primo.
     */
    if (!syscall_throttle_syscall_matches(
            (unsigned int)syscall_id)) {
        return false;
    }

    decision->syscall_id = syscall_id;
    decision->effective_uid = current_euid();

    decision->uid_match =
        syscall_throttle_uid_matches(
            decision->effective_uid
        );

    get_task_comm(
        decision->program_name,
        current
    );

    decision->program_match =
        syscall_throttle_program_matches(
            decision->program_name
        );

    /*
     * Il progetto richiede:
     *
     * effective UID registrato
     * OR
     * programma registrato.
     */
    if (!decision->uid_match &&
        !decision->program_match) {
        return false;
    }

    /*
     * Soltanto le syscall che superano tutto il
     * matching entrano nel contatore globale.
     */
    syscall_throttle_accounting_record(
        syscall_throttle_config_max_value(),
        &decision->accounting
    );

    return true;
}