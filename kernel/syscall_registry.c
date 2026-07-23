#include <asm/unistd.h>

#include <linux/bitmap.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>

#include <syscall_throttle_ioctl.h>

#include "syscall_registry.h"

/*
 * Bitmap delle syscall registrate.
 *
 * A ogni numero di syscall corrisponde un bit:
 *
 *     0 = syscall non registrata
 *     1 = syscall registrata
 */
static DECLARE_BITMAP(registered_syscalls, NR_syscalls);

static __u32 registered_syscall_count;

/*
 * Protegge le modifiche alla bitmap e al contatore.
 */
static DEFINE_MUTEX(registered_syscalls_lock);

static bool syscall_throttle_syscall_is_root(void)
{
    return uid_eq(current_euid(), GLOBAL_ROOT_UID);
}

static bool syscall_throttle_syscall_number_valid(__u32 syscall_nr)
{
    return syscall_nr < NR_syscalls;
}

long syscall_throttle_syscall_register(unsigned long arg)
{
    __u32 syscall_nr;
    long result;

    if (!syscall_throttle_syscall_is_root())
        return -EPERM;

    if (copy_from_user(&syscall_nr,
                       (const void __user *)arg,
                       sizeof(syscall_nr)) != 0) {
        return -EFAULT;
    }

    if (!syscall_throttle_syscall_number_valid(syscall_nr))
        return -EINVAL;

    result = 0;

    mutex_lock(&registered_syscalls_lock);

    if (test_bit(syscall_nr, registered_syscalls)) {
        result = -EEXIST;

    } else if (registered_syscall_count >=
               SYSCALL_THROTTLE_MAX_REGISTERED_SYSCALLS) {
        result = -ENOSPC;

    } else {
        set_bit(syscall_nr, registered_syscalls);
        ++registered_syscall_count;
    }

    mutex_unlock(&registered_syscalls_lock);

    if (result == 0) {
        pr_info(
            "syscall_throttle: syscall %u registrata\n",
            syscall_nr
        );
    }

    return result;
}

long syscall_throttle_syscall_unregister(unsigned long arg)
{
    __u32 syscall_nr;
    long result;

    if (!syscall_throttle_syscall_is_root())
        return -EPERM;

    if (copy_from_user(&syscall_nr,
                       (const void __user *)arg,
                       sizeof(syscall_nr)) != 0) {
        return -EFAULT;
    }

    if (!syscall_throttle_syscall_number_valid(syscall_nr))
        return -EINVAL;

    result = 0;

    mutex_lock(&registered_syscalls_lock);

    if (!test_bit(syscall_nr, registered_syscalls)) {
        result = -ENOENT;

    } else {
        clear_bit(syscall_nr, registered_syscalls);
        --registered_syscall_count;
    }

    mutex_unlock(&registered_syscalls_lock);

    if (result == 0) {
        pr_info(
            "syscall_throttle: syscall %u deregistrata\n",
            syscall_nr
        );
    }

    return result;
}

long syscall_throttle_syscall_get_list(unsigned long arg)
{
    struct syscall_throttle_syscall_list *snapshot;
    unsigned long syscall_nr;
    __u32 index;
    long result;

    /*
     * La struttura può occupare oltre 4 KiB:
     * non deve essere allocata sullo stack kernel.
     */
    snapshot = kzalloc(sizeof(*snapshot), GFP_KERNEL);
    if (snapshot == NULL)
        return -ENOMEM;

    mutex_lock(&registered_syscalls_lock);

    snapshot->count = registered_syscall_count;
    index = 0;

    for_each_set_bit(
        syscall_nr,
        registered_syscalls,
        NR_syscalls
    ) {
        if (index >=
            SYSCALL_THROTTLE_MAX_REGISTERED_SYSCALLS) {
            break;
        }

        snapshot->numbers[index] = (__u32)syscall_nr;
        ++index;
    }

    mutex_unlock(&registered_syscalls_lock);

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

bool syscall_throttle_syscall_matches(unsigned int syscall_nr)
{
    /*
     * test_bit permette una lettura molto rapida.
     * Questa funzione verrà usata dal monitor reale.
     */
    if (syscall_nr >= NR_syscalls)
        return false;

    return test_bit(syscall_nr, registered_syscalls);
}