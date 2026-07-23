#include <asm/unistd.h>

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>

#include "config.h"
#include "monitor.h"
#include "program_registry.h"
#include "syscall_registry.h"
#include "uid_registry.h"

/*
 * Tracepoint raw_syscalls:sys_enter individuato
 * durante l'inizializzazione del modulo.
 */
static struct tracepoint *sys_enter_tracepoint;

/*
 * Indica se la callback è stata registrata.
 */
static bool sys_enter_probe_registered;

/*
 * Callback usata da for_each_kernel_tracepoint().
 *
 * Il nome interno del tracepoint:
 *
 *     raw_syscalls:sys_enter
 *
 * è "sys_enter".
 */
static void syscall_throttle_find_sys_enter(
    struct tracepoint *tracepoint,
    void *private_data)
{
    struct tracepoint **result;

    result = private_data;

    if (*result != NULL)
        return;

    if (strcmp(tracepoint->name, "sys_enter") == 0)
        *result = tracepoint;
}

/*
 * Callback invocata all'ingresso di una syscall.
 *
 * Questa prima versione:
 *
 * - non modifica il flusso;
 * - non blocca il thread;
 * - non prende mutex;
 * - non alloca memoria;
 * - produce solo un log limitato quando il matching
 *   della configurazione è positivo.
 */
static void syscall_throttle_sys_enter_probe(
    void *private_data,
    struct pt_regs *registers,
    long syscall_id)
{
    kuid_t effective_uid;
    __u32 visible_uid;
    char program_name[TASK_COMM_LEN];
    bool uid_match;
    bool program_match;

    /*
     * Parametri non ancora utilizzati.
     */
    (void)private_data;
    (void)registers;

    /*
     * Fast rejection: se il monitor è spento,
     * non consultiamo i registri.
     */
    if (!syscall_throttle_config_monitor_enabled())
        return;

    /*
     * Alcuni percorsi possono produrre identificatori
     * negativi oppure non appartenenti alla tabella
     * syscall nativa.
     */
    if (syscall_id < 0 || syscall_id >= NR_syscalls)
        return;

    /*
     * La bitmap delle syscall viene controllata prima
     * delle strutture più costose.
     */
    if (!syscall_throttle_syscall_matches(
            (unsigned int)syscall_id)) {
        return;
    }

    effective_uid = current_euid();

    /*
     * Entrambe queste funzioni effettuano letture RCU
     * e restituiscono soltanto un valore booleano.
     */
    uid_match = syscall_throttle_uid_matches(
        effective_uid
    );

    get_task_comm(program_name, current);

    program_match = syscall_throttle_program_matches(
        program_name
    );

    /*
     * La traccia richiede:
     *
     *     UID registrato OR programma registrato
     */
    if (!uid_match && !program_match)
        return;

    visible_uid = from_kuid_munged(
        current_user_ns(),
        effective_uid
    );

    /*
     * Il rate limit evita di riempire il log kernel
     * quando una syscall viene eseguita rapidamente.
     */
    pr_info_ratelimited(
        "syscall_throttle: osservata syscall=%ld "
        "euid=%u programma='%s' "
        "uid_match=%u program_match=%u\n",
        syscall_id,
        visible_uid,
        program_name,
        uid_match ? 1U : 0U,
        program_match ? 1U : 0U
    );
}

int syscall_throttle_monitor_init(void)
{
    int result;

    sys_enter_tracepoint = NULL;
    sys_enter_probe_registered = false;

    /*
     * Cerca il tracepoint tra quelli definiti
     * direttamente nel kernel.
     */
    for_each_kernel_tracepoint(
        syscall_throttle_find_sys_enter,
        &sys_enter_tracepoint
    );

    if (sys_enter_tracepoint == NULL) {
        pr_err(
            "syscall_throttle: tracepoint "
            "sys_enter non trovato\n"
        );

        return -ENOENT;
    }

    result = tracepoint_probe_register(
        sys_enter_tracepoint,
        (void *)syscall_throttle_sys_enter_probe,
        NULL
    );

    if (result != 0) {
        pr_err(
            "syscall_throttle: registrazione "
            "probe sys_enter fallita: %d\n",
            result
        );

        sys_enter_tracepoint = NULL;
        return result;
    }

    sys_enter_probe_registered = true;

    pr_info(
        "syscall_throttle: observer sys_enter "
        "registrato\n"
    );

    return 0;
}

void syscall_throttle_monitor_exit(void)
{
    int result;

    if (!sys_enter_probe_registered)
        return;

    result = tracepoint_probe_unregister(
        sys_enter_tracepoint,
        (void *)syscall_throttle_sys_enter_probe,
        NULL
    );

    if (result != 0) {
        pr_warn(
            "syscall_throttle: deregistrazione "
            "probe sys_enter fallita: %d\n",
            result
        );
    }

    /*
     * Aspetta che le callback già iniziate su altre CPU
     * abbiano terminato prima di rimuovere il modulo.
     */
    tracepoint_synchronize_unregister();

    sys_enter_probe_registered = false;
    sys_enter_tracepoint = NULL;

    pr_info(
        "syscall_throttle: observer sys_enter "
        "deregistrato\n"
    );
}