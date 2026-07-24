#include <linux/compiler.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include "accounting.h"
#include "access_control.h"
#include "accounting.h"
#include "config.h"

#include "config.h"

/*
 * Configurazione globale del monitor.
 *
 * Queste variabili sono private di config.c:
 * nessun altro file può modificarle direttamente.
 */
static __u32 max_syscalls_per_second = 1000;
static bool monitor_enabled;



long syscall_throttle_config_set_max(unsigned long arg)
{
    __u32 value;

    /*
     * SET_MAX modifica la configurazione ed è quindi
     * riservato a effective UID 0.
     */
    if (!syscall_throttle_is_root())
        return -EPERM;

    if (copy_from_user(&value,
                       (const void __user *)arg,
                       sizeof(value)) != 0)
        return -EFAULT;

    if (value == 0)
        return -EINVAL;

    WRITE_ONCE(max_syscalls_per_second, value);

    pr_info("syscall_throttle: MAX impostato a %u\n", value);

    return 0;
}

long syscall_throttle_config_get_max(unsigned long arg)
{
    __u32 value;

    value = READ_ONCE(max_syscalls_per_second);

    if (copy_to_user((void __user *)arg,
                     &value,
                     sizeof(value)) != 0)
        return -EFAULT;

    pr_info("syscall_throttle: MAX letto, valore=%u\n", value);

    return 0;
}

long syscall_throttle_config_enable_monitor(void)
{
    if (!syscall_throttle_is_root())
        return -EPERM;

    /*
     * Il monitor riparte sempre da una finestra vuota.
     */
    syscall_throttle_accounting_reset();

    WRITE_ONCE(monitor_enabled, true);

    pr_info(
        "syscall_throttle: monitor attivato\n"
    );

    return 0;
}

long syscall_throttle_config_disable_monitor(void)
{
    if (!syscall_throttle_is_root())
        return -EPERM;

    /*
     * Prima impediamo alle nuove callback di entrare
     * nel percorso di accounting.
     */
    WRITE_ONCE(monitor_enabled, false);

    syscall_throttle_accounting_reset();

    pr_info(
        "syscall_throttle: monitor disattivato\n"
    );

    return 0;
}

long syscall_throttle_config_get_monitor(unsigned long arg)
{
    __u32 value;

    value = READ_ONCE(monitor_enabled) ? 1 : 0;

    if (copy_to_user((void __user *)arg,
                     &value,
                     sizeof(value)) != 0)
        return -EFAULT;

    pr_info("syscall_throttle: stato monitor letto, valore=%u\n",
            value);

    return 0;
}

__u32 syscall_throttle_config_max_value(void)
{
    return READ_ONCE(max_syscalls_per_second);
}

bool syscall_throttle_config_monitor_enabled(void)
{
    return READ_ONCE(monitor_enabled);
}