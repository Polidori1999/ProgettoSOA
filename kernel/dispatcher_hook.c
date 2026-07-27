#include <asm/syscall.h>
#include <asm/unistd.h>

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kprobes.h>
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <linux/uidgid.h>
#include <linux/user_namespace.h>

#include "dispatcher_hook.h"
#include "throttle_engine.h"
/*
 * Firma della funzione kernel non esportata:
 *
 * unsigned long kallsyms_lookup_name(const char *name);
 */
typedef unsigned long (*kallsyms_lookup_name_fn)(
    const char *name
);

/*
 * Tabella delle syscall native x86-64.
 *
 * sys_call_ptr_t è definito da <asm/syscall.h> come:
 *
 * long (*)(const struct pt_regs *);
 */
static const sys_call_ptr_t *resolved_sys_call_table;

/*
 * Kprobe persistente sul dispatcher x86-64.
 */
static struct kprobe dispatcher_probe;

/*
 * Indica se la Kprobe è stata registrata.
 */
static bool dispatcher_probe_registered;

/*
 * Numero di esecuzioni già redirette verso il nostro
 * dispatcher e non ancora completate.
 *
 * Serve a impedire che l'unload termini mentre una CPU
 * sta ancora eseguendo codice del modulo.
 */
static atomic_t active_dispatchers = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(dispatcher_wait_queue);

/*
 * Recupera kallsyms_lookup_name attraverso una Kprobe
 * temporanea.
 */
static int syscall_throttle_find_kallsyms_lookup_name(
    kallsyms_lookup_name_fn *lookup_name)
{
    struct kprobe lookup_probe = {
        .symbol_name = "kallsyms_lookup_name",
    };

    int ret;

    if (lookup_name == NULL)
        return -EINVAL;

    *lookup_name = NULL;

    ret = register_kprobe(&lookup_probe);
    if (ret != 0) {
        pr_err(
            "syscall_throttle: risoluzione di "
            "kallsyms_lookup_name fallita: %d\n",
            ret
        );

        return ret;
    }

    *lookup_name =
        (kallsyms_lookup_name_fn)
        (unsigned long)lookup_probe.addr;

    unregister_kprobe(&lookup_probe);

    if (*lookup_name == NULL) {
        pr_err(
            "syscall_throttle: indirizzo di "
            "kallsyms_lookup_name non valido\n"
        );

        return -ENOENT;
    }

    return 0;
}

/*
 * Dispatcher trasparente.
 *
 * In M4.3a viene raggiunto soltanto per getpid.
 * Non applica ancora matching, accounting o attesa.
 */
static long syscall_throttle_dispatch(
    const struct pt_regs *syscall_regs,
    unsigned int syscall_nr)
{
    struct syscall_throttle_decision decision;
    sys_call_ptr_t syscall_function;
    __u32 visible_uid;
    long result;

    result = -ENOSYS;

    if (unlikely(resolved_sys_call_table == NULL))
        goto completed;

    if (unlikely(syscall_nr >= NR_syscalls))
        goto completed;

    syscall_function =
        resolved_sys_call_table[syscall_nr];

    if (unlikely(syscall_function == NULL))
        goto completed;

    /*
     * Il controllo avviene prima dell'esecuzione
     * effettiva della syscall.
     *
     * In questa fase il motore aggiorna soltanto
     * accounting e decisione: non blocca ancora.
     */
    if (syscall_throttle_engine_evaluate(
            syscall_nr,
            &decision)) {

        visible_uid = from_kuid_munged(
            current_user_ns(),
            decision.effective_uid
        );

        /*
         * Log temporaneo di sviluppo.
         * Verrà rimosso o disabilitato quando il
         * dispatcher sarà stabilizzato.
         */
        pr_info_ratelimited(
            "syscall_throttle: dispatcher syscall=%ld "
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

    /*
     * La syscall reale viene eseguita soltanto dopo
     * la valutazione del motore.
     */
    result = syscall_function(syscall_regs);

    completed:
        if (atomic_dec_and_test(&active_dispatchers))
            wake_up(&dispatcher_wait_queue);

    return result;
}

/*
 * Impedisce che il dispatcher venga scelto come
 * probepoint da altre Kprobe.
 */
NOKPROBE_SYMBOL(syscall_throttle_dispatch);

/*
 * Handler atomico della Kprobe.
 *
 * Per ora redirige soltanto getpid. Su x86-64 il secondo
 * argomento di x64_sys_call(), cioè syscall_nr, si trova
 * nel registro RSI rappresentato da regs->si.
 */
static int syscall_throttle_dispatch_pre_handler(
    struct kprobe *probe,
    struct pt_regs *registers)
{
    unsigned int syscall_nr;

    (void)probe;

    syscall_nr = (unsigned int)registers->si;

    /*
     * Tutte le altre syscall seguono ancora il
     * dispatcher originale del kernel.
     */
    if (syscall_nr != __NR_getpid)
        return 0;

    /*
     * Il contatore viene incrementato prima della
     * redirezione, quindi anche una CPU che non è ancora
     * entrata nel dispatcher risulta già attiva.
     */
    atomic_inc(&active_dispatchers);

    /*
     * Cambia esclusivamente il punto di ripresa.
     * Gli argomenti e il return address restano quelli
     * della chiamata originale a x64_sys_call().
     */
    registers->ip =
        (unsigned long)syscall_throttle_dispatch;

    /*
     * Impedisce a Kprobes di eseguire in single-step
     * l'istruzione originale.
     */
    return 1;
}

NOKPROBE_SYMBOL(
    syscall_throttle_dispatch_pre_handler
);

/*
 * Handler intenzionalmente vuoto.
 *
 * La sua presenza impedisce che la Kprobe venga
 * trasformata in una optprobe, perché una optprobe
 * ignorerebbe la modifica di registers->ip.
 */
static void syscall_throttle_dispatch_post_handler(
    struct kprobe *probe,
    struct pt_regs *registers,
    unsigned long flags)
{
    (void)probe;
    (void)registers;
    (void)flags;
}

NOKPROBE_SYMBOL(
    syscall_throttle_dispatch_post_handler
);

int syscall_throttle_dispatcher_hook_init(void)
{
    kallsyms_lookup_name_fn lookup_name;
    unsigned long table_address;
    int ret;

    resolved_sys_call_table = NULL;
    dispatcher_probe_registered = false;
    atomic_set(&active_dispatchers, 0);

    ret = syscall_throttle_find_kallsyms_lookup_name(
        &lookup_name
    );

    if (ret != 0)
        return ret;

    table_address = lookup_name("sys_call_table");
    if (table_address == 0) {
        pr_err(
            "syscall_throttle: sys_call_table "
            "non trovata\n"
        );

        return -ENOENT;
    }

    resolved_sys_call_table =
        (const sys_call_ptr_t *)table_address;

    dispatcher_probe.symbol_name = "x64_sys_call";
    dispatcher_probe.pre_handler =
        syscall_throttle_dispatch_pre_handler;
    dispatcher_probe.post_handler =
        syscall_throttle_dispatch_post_handler;

    /*
     * La tabella deve essere pronta prima di rendere
     * attiva la redirezione.
     */
    ret = register_kprobe(&dispatcher_probe);
    if (ret != 0) {
        pr_err(
            "syscall_throttle: registrazione Kprobe "
            "su x64_sys_call fallita: %d\n",
            ret
        );

        resolved_sys_call_table = NULL;

        return ret;
    }

    dispatcher_probe_registered = true;

    pr_info(
        "syscall_throttle: sys_call_table=%px\n",
        resolved_sys_call_table
    );

    pr_info(
        "syscall_throttle: Kprobe x64_sys_call "
        "registrata, address=%px\n",
        dispatcher_probe.addr
    );

    pr_info(
        "syscall_throttle: redirect trasparente "
        "attivo solo per getpid, syscall=%d\n",
        __NR_getpid
    );

    return 0;
}

void syscall_throttle_dispatcher_hook_exit(void)
{
    /*
     * Prima impedisce nuove redirezioni.
     */
    if (dispatcher_probe_registered) {
        unregister_kprobe(&dispatcher_probe);
        dispatcher_probe_registered = false;
    }

    /*
     * Poi aspetta le getpid già redirette.
     */
    wait_event(
        dispatcher_wait_queue,
        atomic_read(&active_dispatchers) == 0
    );

    resolved_sys_call_table = NULL;

    pr_info(
        "syscall_throttle: redirect dispatcher "
        "rimosso\n"
    );
}