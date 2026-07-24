#include <linux/errno.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "dispatcher_hook.h"

/*
 * Firma di kallsyms_lookup_name():
 *
 * unsigned long kallsyms_lookup_name(const char *name);
 *
 * Non possiamo richiamarla direttamente perché non è
 * esportata ai moduli. Ne ricaviamo l'indirizzo tramite
 * una kprobe temporanea.
 */
typedef unsigned long (*kallsyms_lookup_name_fn)(
    const char *name
);

/*
 * Indirizzi che verranno usati nelle prossime fasi.
 *
 * Per ora vengono soltanto risolti e stampati.
 */
static unsigned long x64_sys_call_address;
static unsigned long sys_call_table_address;

/*
 * Recupera l'indirizzo di kallsyms_lookup_name tramite
 * una kprobe temporanea.
 *
 * La probe viene rimossa immediatamente: non resta attiva
 * durante l'esecuzione delle syscall.
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
            "syscall_throttle: impossibile risolvere "
            "kallsyms_lookup_name tramite kprobe: %d\n",
            ret
        );

        return ret;
    }

    /*
     * Dopo register_kprobe(), addr contiene l'indirizzo
     * del simbolo indicato in symbol_name.
     */
    *lookup_name =
        (kallsyms_lookup_name_fn)
        (unsigned long)lookup_probe.addr;

    /*
     * La kprobe serviva soltanto per ottenere l'indirizzo.
     */
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

int syscall_throttle_dispatcher_hook_init(void)
{
    kallsyms_lookup_name_fn lookup_name;
    int ret;

    x64_sys_call_address = 0;
    sys_call_table_address = 0;

    ret = syscall_throttle_find_kallsyms_lookup_name(
        &lookup_name
    );

    if (ret != 0)
        return ret;

    /*
     * Risoluzione del dispatcher x86-64 e della tabella
     * delle syscall.
     */
    x64_sys_call_address =
        lookup_name("x64_sys_call");

    sys_call_table_address =
        lookup_name("sys_call_table");

    if (x64_sys_call_address == 0) {
        pr_err(
            "syscall_throttle: simbolo "
            "x64_sys_call non trovato\n"
        );

        return -ENOENT;
    }

    if (sys_call_table_address == 0) {
        pr_err(
            "syscall_throttle: simbolo "
            "sys_call_table non trovato\n"
        );

        x64_sys_call_address = 0;

        return -ENOENT;
    }

    pr_info(
        "syscall_throttle: address discovery completata\n"
    );

    pr_info(
        "syscall_throttle: x64_sys_call=%px\n",
        (void *)x64_sys_call_address
    );

    pr_info(
        "syscall_throttle: sys_call_table=%px\n",
        (void *)sys_call_table_address
    );

    /*
     * In questa milestone non modifichiamo alcun byte
     * del kernel.
     */
    pr_info(
        "syscall_throttle: dispatcher hook non ancora "
        "installato\n"
    );

    return 0;
}

void syscall_throttle_dispatcher_hook_exit(void)
{
    /*
     * Non esiste ancora alcuna patch da ripristinare.
     * Azzeriamo gli indirizzi per rendere esplicito che
     * non devono più essere utilizzati.
     */
    x64_sys_call_address = 0;
    sys_call_table_address = 0;

    pr_info(
        "syscall_throttle: dispatcher hook cleanup "
        "completato\n"
    );
}