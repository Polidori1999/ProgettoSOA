#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include "access_control.h"
#include "program_registry.h"

#include <syscall_throttle_ioctl.h>


/*
 * Ogni snapshot diventa immutabile dopo la pubblicazione.
 *
 * Gli scrittori creano una nuova copia del registro,
 * applicano la modifica e pubblicano il nuovo puntatore.
 *
 * I lettori usano RCU e non acquisiscono mutex.
 */
struct syscall_throttle_program_snapshot {
    __u32 count;

    struct syscall_throttle_program
        programs[
            SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS
        ];
};

/*
 * NULL rappresenta un registro programmi vuoto.
 */
static struct syscall_throttle_program_snapshot __rcu
    *active_program_snapshot;

/*
 * Serializza solamente gli aggiornamenti.
 */
static DEFINE_MUTEX(program_update_lock);



static int syscall_throttle_program_validate(
    const struct syscall_throttle_program *program)
{
    /*
     * Il nome non può essere vuoto.
     */
    if (program->name[0] == '\0')
        return -EINVAL;

    /*
     * Nei 16 byte deve comparire il terminatore.
     *
     * In questo modo accettiamo al massimo:
     *
     *     15 caratteri + '\0'
     */
    if (memchr(program->name,
               '\0',
               SYSCALL_THROTTLE_PROGRAM_NAME_LEN) == NULL) {
        return -EINVAL;
    }

    return 0;
}

static int syscall_throttle_program_copy_from_user(
    unsigned long arg,
    struct syscall_throttle_program *program)
{
    if (copy_from_user(program,
                       (const void __user *)arg,
                       sizeof(*program)) != 0) {
        return -EFAULT;
    }

    return syscall_throttle_program_validate(program);
}

static int syscall_throttle_program_find(
    const struct syscall_throttle_program_snapshot *snapshot,
    const char *name)
{
    __u32 i;

    if (snapshot == NULL)
        return -1;

    for (i = 0; i < snapshot->count; ++i) {
        if (strncmp(snapshot->programs[i].name,
                    name,
                    SYSCALL_THROTTLE_PROGRAM_NAME_LEN) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static void syscall_throttle_program_copy_snapshot(
    struct syscall_throttle_program_snapshot *destination,
    const struct syscall_throttle_program_snapshot *source)
{
    if (source == NULL)
        return;

    destination->count = source->count;

    memcpy(destination->programs,
           source->programs,
           source->count *
               sizeof(source->programs[0]));
}

long syscall_throttle_program_register(unsigned long arg)
{
    struct syscall_throttle_program_snapshot *new_snapshot;
    struct syscall_throttle_program_snapshot *old_snapshot;
    struct syscall_throttle_program program;
    int result;

    if (!syscall_throttle_is_root())
        return -EPERM;

    result = syscall_throttle_program_copy_from_user(
        arg,
        &program
    );

    if (result != 0)
        return result;

    /*
     * L'allocazione può dormire, quindi viene eseguita
     * prima di acquisire il mutex degli scrittori.
     */
    new_snapshot = kzalloc(sizeof(*new_snapshot),
                           GFP_KERNEL);
    if (new_snapshot == NULL)
        return -ENOMEM;

    result = 0;

    mutex_lock(&program_update_lock);

    old_snapshot = rcu_dereference_protected(
        active_program_snapshot,
        lockdep_is_held(&program_update_lock)
    );

    if (syscall_throttle_program_find(
            old_snapshot,
            program.name) >= 0) {
        result = -EEXIST;

    } else if (old_snapshot != NULL &&
               old_snapshot->count >=
               SYSCALL_THROTTLE_MAX_REGISTERED_PROGRAMS) {
        result = -ENOSPC;

    } else {
        syscall_throttle_program_copy_snapshot(
            new_snapshot,
            old_snapshot
        );

        new_snapshot->programs[new_snapshot->count] =
            program;

        ++new_snapshot->count;

        /*
         * I nuovi lettori vedranno il nuovo snapshot.
         */
        rcu_assign_pointer(
            active_program_snapshot,
            new_snapshot
        );
    }

    mutex_unlock(&program_update_lock);

    if (result != 0) {
        kfree(new_snapshot);
        return result;
    }

    /*
     * Aspettiamo la conclusione degli eventuali lettori
     * che stanno ancora usando il vecchio snapshot.
     */
    synchronize_rcu();

    kfree(old_snapshot);

    pr_info(
        "syscall_throttle: programma '%s' registrato\n",
        program.name
    );

    return 0;
}

long syscall_throttle_program_unregister(unsigned long arg)
{
    struct syscall_throttle_program_snapshot *new_snapshot;
    struct syscall_throttle_program_snapshot *old_snapshot;
    struct syscall_throttle_program program;
    __u32 source_index;
    __u32 destination_index;
    int found_index;
    int result;

    if (!syscall_throttle_is_root())
        return -EPERM;

    result = syscall_throttle_program_copy_from_user(
        arg,
        &program
    );

    if (result != 0)
        return result;

    new_snapshot = kzalloc(sizeof(*new_snapshot),
                           GFP_KERNEL);
    if (new_snapshot == NULL)
        return -ENOMEM;

    result = 0;

    mutex_lock(&program_update_lock);

    old_snapshot = rcu_dereference_protected(
        active_program_snapshot,
        lockdep_is_held(&program_update_lock)
    );

    found_index = syscall_throttle_program_find(
        old_snapshot,
        program.name
    );

    if (found_index < 0) {
        result = -ENOENT;

    } else {
        destination_index = 0;

        for (source_index = 0;
             source_index < old_snapshot->count;
             ++source_index) {
            if (source_index == (__u32)found_index)
                continue;

            new_snapshot->programs[destination_index] =
                old_snapshot->programs[source_index];

            ++destination_index;
        }

        new_snapshot->count = destination_index;

        rcu_assign_pointer(
            active_program_snapshot,
            new_snapshot
        );
    }

    mutex_unlock(&program_update_lock);

    if (result != 0) {
        kfree(new_snapshot);
        return result;
    }

    synchronize_rcu();

    kfree(old_snapshot);

    pr_info(
        "syscall_throttle: programma '%s' deregistrato\n",
        program.name
    );

    return 0;
}

long syscall_throttle_program_get_list(unsigned long arg)
{
    struct syscall_throttle_program_list *user_list;
    const struct syscall_throttle_program_snapshot *snapshot;
    int result;

    /*
     * La struttura condivisa supera 1 KiB, quindi non
     * viene allocata sullo stack kernel.
     */
    user_list = kzalloc(sizeof(*user_list), GFP_KERNEL);
    if (user_list == NULL)
        return -ENOMEM;

    rcu_read_lock();

    snapshot = rcu_dereference(active_program_snapshot);

    if (snapshot != NULL) {
        user_list->count = snapshot->count;

        memcpy(user_list->programs,
               snapshot->programs,
               snapshot->count *
                   sizeof(snapshot->programs[0]));
    }

    rcu_read_unlock();

    if (copy_to_user((void __user *)arg,
                     user_list,
                     sizeof(*user_list)) != 0) {
        result = -EFAULT;
    } else {
        result = 0;
    }

    kfree(user_list);

    return result;
}

bool syscall_throttle_program_matches(const char *name)
{
    const struct syscall_throttle_program_snapshot *snapshot;
    bool found;

    if (name == NULL || name[0] == '\0')
        return false;

    found = false;

    rcu_read_lock();

    snapshot = rcu_dereference(active_program_snapshot);

    if (syscall_throttle_program_find(
            snapshot,
            name) >= 0) {
        found = true;
    }

    rcu_read_unlock();

    return found;
}

void syscall_throttle_program_registry_cleanup(void)
{
    struct syscall_throttle_program_snapshot *old_snapshot;

    mutex_lock(&program_update_lock);

    old_snapshot = rcu_dereference_protected(
        active_program_snapshot,
        lockdep_is_held(&program_update_lock)
    );

    /*
     * Da questo momento i nuovi lettori vedono
     * un registro vuoto.
     */
    RCU_INIT_POINTER(active_program_snapshot, NULL);

    mutex_unlock(&program_update_lock);

    /*
     * Attendiamo la conclusione degli eventuali lettori
     * che avevano ottenuto il vecchio puntatore.
     */
    synchronize_rcu();

    kfree(old_snapshot);
}
