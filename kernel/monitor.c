#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>

#include "monitor.h"
#include "throttle_engine.h"

/*
 * Tracepoint raw_syscalls:sys_enter individuato
 * durante l'inizializzazione del modulo.
 */
static struct tracepoint *sys_enter_tracepoint;

/*
 * Indica se la callback è stata registrata.
 */
static bool sys_enter_probe_registered;

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
 * Observer temporaneo.
 *
 * Matching e accounting sono delegati al motore
 * comune, che verrà successivamente chiamato anche
 * dal dispatcher reale.
 */
static void syscall_throttle_sys_enter_probe(
    void *private_data,
    struct pt_regs *registers,
    long syscall_id)
{
    struct syscall_throttle_decision decision;
    __u32 visible_uid;

    (void)private_data;
    (void)registers;

    if (!syscall_throttle_engine_evaluate(
            syscall_id,
            &decision)) {
        return;
    }

    visible_uid = from_kuid_munged(
        current_user_ns(),
        decision.effective_uid
    );

    pr_info_ratelimited(
        "syscall_throttle: accounting syscall=%ld "
        "euid=%u programma='%s' "
        "count=%u max=%u stato=%s "
        "uid_match=%u program_match=%u\n",
        decision.syscall_id,
        visible_uid,
        decision.program_name,
        decision.accounting.count,
        decision.accounting.max,
        decision.accounting.exceeded
            ? "ECCEDENTE"
            : "AMMESSA",
        decision.uid_match ? 1U : 0U,
        decision.program_match ? 1U : 0U
    );
}

int syscall_throttle_monitor_init(void)
{
    int result;

    sys_enter_tracepoint = NULL;
    sys_enter_probe_registered = false;

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
     * Attende le callback già iniziate sulle altre CPU.
     */
    tracepoint_synchronize_unregister();

    sys_enter_probe_registered = false;
    sys_enter_tracepoint = NULL;

    pr_info(
        "syscall_throttle: observer sys_enter "
        "deregistrato\n"
    );
}