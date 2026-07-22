#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>

#include <syscall_throttle_ioctl.h>

#include "uid_registry.h"

/*
 * Lista degli UID sottoposti al monitor.
 *
 * Gli UID vengono memorizzati come kuid_t, cioè nella
 * rappresentazione interna utilizzata dal kernel.
 */
static kuid_t registered_uids[
    SYSCALL_THROTTLE_MAX_REGISTERED_UIDS
];

static __u32 registered_uid_count;

/*
 * Protegge sia l'array sia il contatore.
 */
static DEFINE_MUTEX(registered_uids_lock);

/*
 * Le modifiche alla configurazione sono consentite
 * soltanto a un thread con effective UID 0.
 */
static bool syscall_throttle_uid_is_root(void)
{
    return uid_eq(current_euid(), GLOBAL_ROOT_UID);
}

/*
 * Cerca un UID nella lista.
 *
 * Deve essere chiamata mentre registered_uids_lock
 * è già acquisito.
 */
static int syscall_throttle_uid_find_locked(kuid_t uid)
{
    __u32 i;

    for (i = 0; i < registered_uid_count; ++i) {
        if (uid_eq(registered_uids[i], uid))
            return (int)i;
    }

    return -1;
}

long syscall_throttle_uid_register(unsigned long arg)
{
    __u32 raw_uid;
    kuid_t uid;
    long result;

    if (!syscall_throttle_uid_is_root())
        return -EPERM;

    if (copy_from_user(&raw_uid,
                       (const void __user *)arg,
                       sizeof(raw_uid)) != 0)
        return -EFAULT;

    uid = make_kuid(&init_user_ns, raw_uid);

    if (!uid_valid(uid))
        return -EINVAL;

    result = 0;

    mutex_lock(&registered_uids_lock);

    if (syscall_throttle_uid_find_locked(uid) >= 0) {
        result = -EEXIST;

    } else if (registered_uid_count >=
               SYSCALL_THROTTLE_MAX_REGISTERED_UIDS) {
        result = -ENOSPC;

    } else {
        registered_uids[registered_uid_count] = uid;
        ++registered_uid_count;
    }

    mutex_unlock(&registered_uids_lock);

    if (result == 0) {
        pr_info("syscall_throttle: UID %u registrato\n",
                raw_uid);
    }

    return result;
}

long syscall_throttle_uid_unregister(unsigned long arg)
{
    __u32 raw_uid;
    __u32 i;
    kuid_t uid;
    int index;
    long result;

    if (!syscall_throttle_uid_is_root())
        return -EPERM;

    if (copy_from_user(&raw_uid,
                       (const void __user *)arg,
                       sizeof(raw_uid)) != 0)
        return -EFAULT;

    uid = make_kuid(&init_user_ns, raw_uid);

    if (!uid_valid(uid))
        return -EINVAL;

    result = 0;

    mutex_lock(&registered_uids_lock);

    index = syscall_throttle_uid_find_locked(uid);

    if (index < 0) {
        result = -ENOENT;

    } else {
        /*
         * Rimuove l'elemento compattando l'array.
         */
        for (i = (__u32)index;
             i + 1 < registered_uid_count;
             ++i) {
            registered_uids[i] = registered_uids[i + 1];
        }

        --registered_uid_count;
    }

    mutex_unlock(&registered_uids_lock);

    if (result == 0) {
        pr_info("syscall_throttle: UID %u deregistrato\n",
                raw_uid);
    }

    return result;
}

long syscall_throttle_uid_get_list(unsigned long arg)
{
    struct syscall_throttle_uid_list snapshot = { 0 };
    __u32 i;

    /*
     * Costruisce uno snapshot coerente della lista.
     */
    mutex_lock(&registered_uids_lock);

    snapshot.count = registered_uid_count;

    for (i = 0; i < registered_uid_count; ++i) {
        snapshot.uids[i] =
            (__u32)from_kuid_munged(
                &init_user_ns,
                registered_uids[i]
            );
    }

    mutex_unlock(&registered_uids_lock);

    /*
     * La copia può provocare page fault, quindi viene
     * eseguita dopo aver rilasciato il mutex.
     */
    if (copy_to_user((void __user *)arg,
                     &snapshot,
                     sizeof(snapshot)) != 0)
        return -EFAULT;

    return 0;
}