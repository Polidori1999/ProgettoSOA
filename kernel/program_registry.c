#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/slab.h>

#include <syscall_throttle_ioctl.h>

#include "program_registry.h"

/*
 * Programmi attualmente registrati.
 */
static struct syscall_throttle_program registered_programs[
    SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS
];

static __u32 registered_program_count;

/*
 * Protegge l'array e il relativo contatore.
 */
static DEFINE_MUTEX(registered_programs_lock);

static bool syscall_throttle_program_is_root(void)
{
    return uid_eq(current_euid(), GLOBAL_ROOT_UID);
}

/*
 * Cerca un nome nella lista.
 *
 * Deve essere invocata con registered_programs_lock
 * già acquisito.
 */
static int syscall_throttle_program_find_locked(const char *name)
{
    __u32 i;

    for (i = 0; i < registered_program_count; ++i) {
        if (strcmp(registered_programs[i].name, name) == 0)
            return (int)i;
    }

    return -1;
}

/*
 * Copia e valida il nome proveniente dallo user-space.
 */
static int syscall_throttle_program_copy_from_user(
    unsigned long arg,
    struct syscall_throttle_program *program)
{
    size_t length;

    memset(program, 0, sizeof(*program));

    if (copy_from_user(program,
                       (const void __user *)arg,
                       sizeof(*program)) != 0)
        return -EFAULT;

    /*
     * Se strnlen restituisce 16, il buffer non contiene
     * il terminatore '\0'.
     */
    length = strnlen(program->name,
                     SYSCALL_THROTTLE_PROGRAM_NAME_LEN);

    if (length == 0)
        return -EINVAL;

    if (length == SYSCALL_THROTTLE_PROGRAM_NAME_LEN)
        return -ENAMETOOLONG;

    return 0;
}

long syscall_throttle_program_register(unsigned long arg)
{
    struct syscall_throttle_program program;
    long result;
    int ret;

    if (!syscall_throttle_program_is_root())
        return -EPERM;

    ret = syscall_throttle_program_copy_from_user(
        arg,
        &program
    );

    if (ret != 0)
        return ret;

    result = 0;

    mutex_lock(&registered_programs_lock);

    if (syscall_throttle_program_find_locked(
            program.name) >= 0) {
        result = -EEXIST;

    } else if (registered_program_count >=
               SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS) {
        result = -ENOSPC;

    } else {
        /*
         * Pulisce la destinazione per non conservare
         * dati residui dopo il terminatore.
         */
        memset(
            &registered_programs[registered_program_count],
            0,
            sizeof(registered_programs[0])
        );

        strscpy(
            registered_programs[registered_program_count].name,
            program.name,
            SYSCALL_THROTTLE_PROGRAM_NAME_LEN
        );

        ++registered_program_count;
    }

    mutex_unlock(&registered_programs_lock);

    if (result == 0) {
        pr_info(
            "syscall_throttle: programma '%s' registrato\n",
            program.name
        );
    }

    return result;
}

long syscall_throttle_program_unregister(unsigned long arg)
{
    struct syscall_throttle_program program;
    size_t elements_to_move;
    long result;
    int index;
    int ret;

    if (!syscall_throttle_program_is_root())
        return -EPERM;

    ret = syscall_throttle_program_copy_from_user(
        arg,
        &program
    );

    if (ret != 0)
        return ret;

    result = 0;

    mutex_lock(&registered_programs_lock);

    index = syscall_throttle_program_find_locked(
        program.name
    );

    if (index < 0) {
        result = -ENOENT;

    } else {
        elements_to_move =
            registered_program_count - (size_t)index - 1;

        /*
         * Compatta l'array spostando a sinistra
         * gli elementi successivi.
         */
        if (elements_to_move > 0) {
            memmove(
                &registered_programs[index],
                &registered_programs[index + 1],
                elements_to_move *
                    sizeof(registered_programs[0])
            );
        }

        --registered_program_count;

        memset(
            &registered_programs[registered_program_count],
            0,
            sizeof(registered_programs[0])
        );
    }

    mutex_unlock(&registered_programs_lock);

    if (result == 0) {
        pr_info(
            "syscall_throttle: programma '%s' deregistrato\n",
            program.name
        );
    }

    return result;
}

long syscall_throttle_program_get_list(unsigned long arg)
{
    struct syscall_throttle_program_list *snapshot;
    __u32 i;
    long result;

    /*
     * La struttura supera 1 KiB e non deve essere
     * allocata sullo stack del kernel.
     *
     * kzalloc alloca memoria dinamica già inizializzata
     * a zero.
     */
    snapshot = kzalloc(sizeof(*snapshot), GFP_KERNEL);
    if (snapshot == NULL)
        return -ENOMEM;

    mutex_lock(&registered_programs_lock);

    snapshot->count = registered_program_count;

    for (i = 0; i < registered_program_count; ++i) {
        strscpy(
            snapshot->programs[i].name,
            registered_programs[i].name,
            SYSCALL_THROTTLE_PROGRAM_NAME_LEN
        );
    }

    mutex_unlock(&registered_programs_lock);

    /*
     * copy_to_user viene eseguita dopo aver rilasciato
     * il mutex, perché può provocare un page fault e
     * sospendere il thread.
     */
    if (copy_to_user((void __user *)arg,
                     snapshot,
                     sizeof(*snapshot)) != 0) {
        result = -EFAULT;
                     } else {
                         result = 0;
                     }

    kfree(snapshot);

    return result;
}