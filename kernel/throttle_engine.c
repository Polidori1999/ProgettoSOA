#include <asm/unistd.h>

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/wait.h>

#include <linux/hrtimer.h>
#include <linux/ktime.h>


#include "accounting.h"
#include "config.h"
#include "program_registry.h"
#include "syscall_registry.h"
#include "throttle_engine.h"
#include "uid_registry.h"



/*
 * Wait queue usata dai thread eccedenti.
 *
 * Verrà utilizzata nella prossima sottofase.
 */
static DECLARE_WAIT_QUEUE_HEAD(
    syscall_throttle_wait_queue
);

/*
 * Cambia ogni volta che il monitor viene disattivato
 * o il modulo entra in shutdown.
 *
 * In futuro permetterà ai thread di riconoscere un
 * monitor-off anche se il monitor viene riattivato
 * molto rapidamente.
 */
static atomic64_t control_generation =
    ATOMIC64_INIT(0);

/*
 * Impedisce nuove attese durante l'unload.
 */
static bool engine_shutting_down;

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


int syscall_throttle_engine_enforce(
    long syscall_id,
    struct syscall_throttle_decision *decision)
{
    s64 observed_generation;
    long wait_result;

    if (decision == NULL)
        return -EINVAL;

    for (;;) {
        /*
         * La generazione viene letta prima della
         * valutazione.
         *
         * In questo modo rileviamo anche un monitor-off
         * seguito immediatamente da monitor-on mentre
         * il task stava eseguendo l'accounting.
         */
        observed_generation =
            atomic64_read(&control_generation);

        /*
         * Durante l'unload nessun dispatcher deve
         * iniziare una nuova attesa.
         */
        if (READ_ONCE(engine_shutting_down))
            return 0;

        /*
         * Esegue matching e prova a riservare un posto
         * nella finestra corrente.
         */
        if (!syscall_throttle_engine_evaluate(
                syscall_id,
                decision)) {
            return 0;
        }

        /*
         * La syscall ha ottenuto un posto e può essere
         * eseguita.
         */
        if (!decision->accounting.exceeded)
            return 1;

        /*
         * Un monitor-off potrebbe essere avvenuto tra
         * la lettura della generazione e l'accounting.
         *
         * In tal caso la syscall deve passare senza
         * rimanere in attesa.
         */
        if (READ_ONCE(engine_shutting_down) ||
            !syscall_throttle_config_monitor_enabled() ||
            atomic64_read(&control_generation) !=
                observed_generation) {
            return 0;
        }

        /*
         * Normalmente wait_ns è maggiore di zero per
         * una finestra piena. In caso di scadenza
         * contemporanea, ripetiamo subito l'accounting.
         */
        if (decision->accounting.wait_ns == 0)
            continue;

        /*
         * Il task resta in TASK_INTERRUPTIBLE fino a:
         *
         * - scadenza della finestra;
         * - monitor-off;
         * - shutdown del modulo;
         * - ricezione di un segnale.
         */
        wait_result =
            wait_event_interruptible_hrtimeout(
                syscall_throttle_wait_queue,
                READ_ONCE(engine_shutting_down) ||
                !syscall_throttle_config_monitor_enabled() ||
                atomic64_read(&control_generation) !=
                    observed_generation,
                ns_to_ktime(
                    decision->accounting.wait_ns
                )
            );

        /*
         * La syscall non è ancora stata eseguita.
         * Restituendo -ERESTARTSYS permettiamo al
         * normale percorso syscall di gestire il
         * segnale ed eventualmente riavviare la
         * chiamata.
         */
        if (wait_result == -ERESTARTSYS)
            return -ERESTARTSYS;

        /*
         * Monitor-off e shutdown liberano
         * immediatamente il task.
         *
         * Il confronto della generazione gestisce
         * anche monitor-off seguito rapidamente da
         * monitor-on.
         */
        if (READ_ONCE(engine_shutting_down) ||
            !syscall_throttle_config_monitor_enabled() ||
            atomic64_read(&control_generation) !=
                observed_generation) {
            return 0;
        }

        /*
         * -ETIME indica che il timeout è scaduto.
         *
         * Torniamo all'inizio e proviamo a riservare
         * un posto nella nuova finestra. Se molti task
         * si risvegliano insieme, soltanto i primi MAX
         * saranno ammessi; gli altri attenderanno
         * ancora.
         */
        if (wait_result == -ETIME)
            continue;

        /*
         * Con la condizione usata sopra, zero indica
         * normalmente un risveglio dovuto a una
         * variazione del control plane. Per robustezza
         * ripetiamo comunque la valutazione.
         */
        if (wait_result == 0)
            continue;

        /*
         * Protezione da eventuali valori negativi non
         * previsti.
         */
        return (int)wait_result;
    }
}



void syscall_throttle_engine_monitor_disabled(void)
{
    /*
     * L'incremento rende osservabile l'evento anche se
     * il monitor viene riattivato prima che tutti i
     * thread risvegliati tornino in esecuzione.
     */
    atomic64_inc(&control_generation);

    /*
     * La futura attesa userà TASK_INTERRUPTIBLE.
     */
    wake_up_interruptible_all(
        &syscall_throttle_wait_queue
    );
}

void syscall_throttle_engine_shutdown(void)
{
    /*
     * Deve essere pubblicato prima del wake-up.
     */
    WRITE_ONCE(engine_shutting_down, true);

    syscall_throttle_engine_monitor_disabled();
}