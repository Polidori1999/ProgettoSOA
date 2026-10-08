#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>
#include "access_control.h"
#include "uid_registry.h"

#include <syscall_throttle_ioctl.h>


/*
 * Ogni snapshot Ã¨ immutabile dopo essere stato pubblicato.
 *
 * Gli scrittori costruiscono un nuovo snapshot e lo
 * pubblicano tramite rcu_assign_pointer().
 *
 * I lettori consultano lo snapshot corrente tramite RCU.
 */
struct syscall_throttle_uid_snapshot {
    __u32 count;

    kuid_t uids[
        SYSCALL_THROTTLE_MAX_REGISTERED_UIDS
    ];
};

/*
 * Puntatore allo snapshot attualmente visibile.
 *
 * Inizialmente Ã¨ NULL e rappresenta un registro vuoto.
 */
static struct syscall_throttle_uid_snapshot __rcu
    *active_uid_snapshot;

/*
 * Il mutex serializza soltanto gli aggiornamenti.
 *
 * I lettori del data plane non acquisiscono questo mutex.
 */
static DEFINE_MUTEX(uid_update_lock);



static int syscall_throttle_uid_find(
    const struct syscall_throttle_uid_snapshot *snapshot,
    kuid_t uid)
{
    __u32 i;

    if (snapshot == NULL)
        return -1;

    for (i = 0; i < snapshot->count; ++i) {
        if (uid_eq(snapshot->uids[i], uid))
            return (int)i;
    }

    return -1;
}

static void syscall_throttle_uid_copy_snapshot(
    struct syscall_throttle_uid_snapshot *destination,
    const struct syscall_throttle_uid_snapshot *source)
{
    if (source == NULL)
        return;

    destination->count = source->count;

    memcpy(destination->uids,
           source->uids,
           source->count * sizeof(source->uids[0]));
}

/*
 * Percorso comune per aggiunta e rimozione: cambia solo la modifica alla copia.
 * Lo snapshot pubblicato resta immutabile per i lettori RCU.
 */
static long syscall_throttle_uid_update(unsigned long arg, bool add)
{
    struct syscall_throttle_uid_snapshot *new_snapshot;
    struct syscall_throttle_uid_snapshot *old_snapshot;
    __u32 raw_uid;
    kuid_t uid;
    int found_index;
    long result = 0;

    /* Il controllo dei privilegi precede anche la lettura dei dati utente. */
    if (!syscall_throttle_is_root())
        return -EPERM;

    if (copy_from_user(&raw_uid, (const void __user *)arg,
                       sizeof(raw_uid)) != 0)
        return -EFAULT;

    /* Traduce l'UID del namespace chiamante nel tipo usato dal kernel. */
    uid = make_kuid(current_user_ns(), raw_uid);
    if (!uid_valid(uid))
        return -EINVAL;

    /* Alloca fuori dal mutex per ridurre il tempo di esclusione degli scrittori. */
    new_snapshot = kzalloc(sizeof(*new_snapshot), GFP_KERNEL);
    if (new_snapshot == NULL)
        return -ENOMEM;

    mutex_lock(&uid_update_lock);
    old_snapshot = rcu_dereference_protected(
        active_uid_snapshot, lockdep_is_held(&uid_update_lock));
    found_index = syscall_throttle_uid_find(old_snapshot, uid);

    /* Controlla gli errori senza modificare il registro corrente. */
    if (add && found_index >= 0) {
        result = -EEXIST;
    } else if (add && old_snapshot != NULL &&
               old_snapshot->count >= SYSCALL_THROTTLE_MAX_REGISTERED_UIDS) {
        result = -ENOSPC;
    } else if (!add && found_index < 0) {
        result = -ENOENT;
    } else {
        syscall_throttle_uid_copy_snapshot(new_snapshot, old_snapshot);

        if (add) {
            new_snapshot->uids[new_snapshot->count] = uid;
            ++new_snapshot->count;
        } else {
            /* Compatta gli elementi successivi, mantenendo il loro ordine. */
            memmove(&new_snapshot->uids[found_index],
                    &new_snapshot->uids[found_index + 1],
                    (new_snapshot->count - (__u32)found_index - 1U) *
                        sizeof(new_snapshot->uids[0]));
            --new_snapshot->count;
        }

        /* Pubblica la copia completa: i nuovi lettori possono consultarla. */
        rcu_assign_pointer(active_uid_snapshot, new_snapshot);
    }
    mutex_unlock(&uid_update_lock);

    if (result != 0) {
        /* La copia non Ã¨ stata pubblicata e puÃ² essere liberata subito. */
        kfree(new_snapshot);
        return result;
    }

    /* Attende i lettori del vecchio snapshot prima di liberarlo, senza mutex. */
    synchronize_rcu();
    kfree(old_snapshot);

    pr_info("syscall_throttle: UID %u %s\n", raw_uid,
            add ? "registrato" : "deregistrato");
    return 0;
}

/* Le funzioni pubbliche mantengono l'interfaccia usata dal driver ioctl. */
long syscall_throttle_uid_register(unsigned long arg)
{
    return syscall_throttle_uid_update(arg, true);
}

long syscall_throttle_uid_unregister(unsigned long arg)
{
    return syscall_throttle_uid_update(arg, false);
}

long syscall_throttle_uid_get_list(unsigned long arg)
{
    struct syscall_throttle_uid_list user_list = {0};
    const struct syscall_throttle_uid_snapshot *snapshot;
    __u32 i;

    /*
     * Anche uid-list usa una lettura RCU.
     */
    rcu_read_lock();

    snapshot = rcu_dereference(active_uid_snapshot);

    if (snapshot != NULL) {
        user_list.count = snapshot->count;

        for (i = 0; i < snapshot->count; ++i) {
            user_list.uids[i] = from_kuid_munged(
                current_user_ns(),
                snapshot->uids[i]
            );
        }
    }

    rcu_read_unlock();

    if (copy_to_user((void __user *)arg,
                     &user_list,
                     sizeof(user_list)) != 0) {
        return -EFAULT;
    }

    return 0;
}

bool syscall_throttle_uid_matches(kuid_t uid)
{
    const struct syscall_throttle_uid_snapshot *snapshot;
    bool found;

    rcu_read_lock();

    snapshot = rcu_dereference(active_uid_snapshot);

    /* Legge il registro senza acquisire il mutex degli aggiornamenti. */
    found = syscall_throttle_uid_find(snapshot, uid) >= 0;

    rcu_read_unlock();

    return found;
}

void syscall_throttle_uid_registry_cleanup(void)
{
    struct syscall_throttle_uid_snapshot *old_snapshot;

    mutex_lock(&uid_update_lock);

    old_snapshot = rcu_dereference_protected(
        active_uid_snapshot,
        lockdep_is_held(&uid_update_lock)
    );

    /*
     * Impedisce a eventuali nuovi lettori di ottenere
     * il vecchio puntatore.
     */
    RCU_INIT_POINTER(active_uid_snapshot, NULL);

    mutex_unlock(&uid_update_lock);

    /*
     * Prima di liberare lo snapshot attendiamo la fine
     * degli eventuali lettori giÃ  entrati in RCU.
     */
    synchronize_rcu();

    kfree(old_snapshot);
}