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
#include <linux/time64.h>

#include "statistics.h"

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

/*
 * Timer autonomo destinato a scandire finestre
 * consecutive della durata di un secondo.
 *
 * In questo checkpoint viene inizializzato, ma non
 * viene ancora collegato a monitor-on.
 */
static struct hrtimer window_timer;

/*
 * Cambierà a ogni apertura di una nuova finestra.
 *
 * Nel passo successivo verrà aggiunta alle condizioni
 * di attesa dei waiter.
 */
static atomic64_t window_generation =
    ATOMIC64_INIT(0);


/*
 * Callback della futura finestra periodica.
 *
 * La callback opera in contesto atomico:
 *
 * - azzera il contatore globale;
 * - pubblica una nuova generazione;
 * - risveglia i waiter;
 * - riarma il timer a un secondo.
 */
static enum hrtimer_restart
syscall_throttle_window_timer_callback(
    struct hrtimer *timer)
{
    if (READ_ONCE(engine_shutting_down) ||
        !syscall_throttle_config_monitor_enabled()) {
        return HRTIMER_NORESTART;
    }

    syscall_throttle_accounting_reset();

    atomic64_inc(&window_generation);

    wake_up_interruptible_all(
        &syscall_throttle_wait_queue
    );

    hrtimer_forward_now(
        timer,
        ns_to_ktime(NSEC_PER_SEC)
    );

    return HRTIMER_RESTART;
}


void syscall_throttle_engine_init(void)
{
    WRITE_ONCE(engine_shutting_down, false);

    hrtimer_setup(
        &window_timer,
        syscall_throttle_window_timer_callback,
        CLOCK_MONOTONIC,
        HRTIMER_MODE_REL
    );
}


void syscall_throttle_engine_monitor_enabled(void)
{
    /*
     * Evita di mantenere una precedente istanza
     * eventualmente armata.
     */
    hrtimer_cancel(&window_timer);

    /*
     * Ogni nuova sequenza parte con quota vuota.
     */
    syscall_throttle_accounting_reset();

    atomic64_inc(&window_generation);

    wake_up_interruptible_all(
        &syscall_throttle_wait_queue
    );

    hrtimer_start(
        &window_timer,
        ns_to_ktime(NSEC_PER_SEC),
        HRTIMER_MODE_REL
    );
}


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

    bool task_blocked = false;
    u64 blocked_since_ns = 0;
    kuid_t blocked_uid = KUIDT_INIT(0);
    char blocked_program[TASK_COMM_LEN] = {0};

    int result;

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
        if (READ_ONCE(engine_shutting_down)) {
            result = 0;
            goto out;
        }

        /*
         * Esegue matching e prova a riservare un posto
         * nella finestra corrente.
         */
        if (!syscall_throttle_engine_evaluate(
                syscall_id,
                decision)) {
            result = 0;
            goto out;
        }

        /*
         * La syscall ha ottenuto un posto e può essere
         * eseguita.
         */
        if (!decision->accounting.exceeded) {
            result = 1;
            goto out;
        }

        /*
         * Un monitor-off potrebbe essere avvenuto tra
         * la lettura della generazione e l'accounting.
         */
        if (READ_ONCE(engine_shutting_down) ||
            !syscall_throttle_config_monitor_enabled() ||
            atomic64_read(&control_generation) !=
                observed_generation) {

            result = 0;
            goto out;
        }

        /*
         * In caso di scadenza contemporanea della
         * finestra ripetiamo immediatamente l'accounting,
         * senza considerare il task realmente bloccato.
         */
        if (decision->accounting.wait_ns == 0)
            continue;

        /*
         * Il task entra nello stato statisticamente
         * bloccato soltanto prima della prima attesa
         * effettiva.
         *
         * Se perde la contesa nella finestra successiva
         * e deve attendere ancora, non viene contato una
         * seconda volta.
         */
        if (!task_blocked) {
            blocked_uid =
                decision->effective_uid;

            strscpy(
                blocked_program,
                decision->program_name,
                sizeof(blocked_program)
            );

            blocked_since_ns =
                syscall_throttle_statistics_block_enter();

            task_blocked = true;
        }

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
         * La syscall originale non viene eseguita per
         * questa specifica invocazione.
         */
        if (wait_result == -ERESTARTSYS) {
            result = -ERESTARTSYS;
            goto out;
        }

        /*
         * Monitor-off e shutdown liberano il task e
         * permettono alla syscall originale di passare.
         */
        if (READ_ONCE(engine_shutting_down) ||
            !syscall_throttle_config_monitor_enabled() ||
            atomic64_read(&control_generation) !=
                observed_generation) {

            result = 0;
            goto out;
        }

        /*
         * Alla scadenza del timeout il task ripete
         * l'accounting.
         */
        if (wait_result == -ETIME)
            continue;

        /*
         * Per robustezza, un risveglio senza errore
         * provoca una nuova valutazione.
         */
        if (wait_result == 0)
            continue;

        /*
         * Protezione da valori inattesi.
         */
        result = (int)wait_result;
        goto out;
    }

out:
    /*
     * Uscita unica dallo stato bloccato.
     *
     * result >= 0 significa che il dispatcher eseguirà
     * comunque la syscall originale.
     */
    if (task_blocked) {
        syscall_throttle_statistics_block_exit(
            blocked_since_ns,
            result >= 0,
            blocked_uid,
            blocked_program
        );
    }

    return result;
}



void syscall_throttle_engine_monitor_disabled(void)
{
    /*
     * È innocuo anche quando il timer non è stato
     * avviato. Sarà necessario quando monitor-on verrà
     * collegato al timer autonomo.
     */
    hrtimer_cancel(&window_timer);

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