#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/printk.h>

#include <syscall_throttle_ioctl.h>

#include "config.h"
#include "dispatcher_hook.h"

#include "program_registry.h"
#include "syscall_registry.h"
#include "uid_registry.h"

static long syscall_throttle_ioctl(
    struct file *file,
    unsigned int cmd,
    unsigned long arg)
{
    (void)file;

    if (_IOC_TYPE(cmd) != SYSCALL_THROTTLE_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case SYSCALL_THROTTLE_IOC_PING:
        return 0;

    case SYSCALL_THROTTLE_IOC_SET_MAX:
        return syscall_throttle_config_set_max(arg);

    case SYSCALL_THROTTLE_IOC_GET_MAX:
        return syscall_throttle_config_get_max(arg);

    case SYSCALL_THROTTLE_IOC_ENABLE_MONITOR:
        return syscall_throttle_config_enable_monitor();

    case SYSCALL_THROTTLE_IOC_DISABLE_MONITOR:
        return syscall_throttle_config_disable_monitor();

    case SYSCALL_THROTTLE_IOC_GET_MONITOR:
        return syscall_throttle_config_get_monitor(arg);

    case SYSCALL_THROTTLE_IOC_REGISTER_UID:
        return syscall_throttle_uid_register(arg);

    case SYSCALL_THROTTLE_IOC_UNREGISTER_UID:
        return syscall_throttle_uid_unregister(arg);

    case SYSCALL_THROTTLE_IOC_GET_UIDS:
        return syscall_throttle_uid_get_list(arg);

    case SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM:
        return syscall_throttle_program_register(arg);

    case SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM:
        return syscall_throttle_program_unregister(arg);

    case SYSCALL_THROTTLE_IOC_GET_PROGRAMS:
        return syscall_throttle_program_get_list(arg);

    case SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL:
        return syscall_throttle_syscall_register(arg);

    case SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL:
        return syscall_throttle_syscall_unregister(arg);

    case SYSCALL_THROTTLE_IOC_GET_SYSCALLS:
        return syscall_throttle_syscall_get_list(arg);

    default:
        return -ENOTTY;
    }
}

static const struct file_operations syscall_throttle_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = syscall_throttle_ioctl,
};

static struct miscdevice syscall_throttle_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "syscall_throttle",
    .fops = &syscall_throttle_fops,
    .mode = 0666,
};

static int __init syscall_throttle_init(void)
{
    int ret;

    ret = misc_register(&syscall_throttle_device);
    if (ret != 0) {
        pr_err(
            "syscall_throttle: registrazione device "
            "fallita: %d\n",
            ret
        );

        return ret;
    }

    /*
     * Installa la Kprobe sul dispatcher delle syscall.
     */
    ret = syscall_throttle_dispatcher_hook_init();
    if (ret != 0) {
        pr_err(
            "syscall_throttle: inizializzazione "
            "dispatcher hook fallita: %d\n",
            ret
        );

        goto error_device;
    }

    pr_info(
        "syscall_throttle: device /dev/%s registrato, "
        "minor=%d, MAX=%u, monitor=%s\n",
        syscall_throttle_device.name,
        syscall_throttle_device.minor,
        syscall_throttle_config_max_value(),
        syscall_throttle_config_monitor_enabled()
            ? "attivo"
            : "disattivo"
    );

    return 0;

    error_device:
        misc_deregister(&syscall_throttle_device);

    return ret;
}

static void __exit syscall_throttle_exit(void)
{
    /*
     * Impedisce nuove operazioni di configurazione.
     */
    misc_deregister(&syscall_throttle_device);

    /*
     * Rimuove la Kprobe e aspetta i dispatcher
     * già entrati.
     */
    syscall_throttle_dispatcher_hook_exit();

    /*
     * Dopo la rimozione del dispatcher nessun fast
     * path può più consultare i registri.
     */
    syscall_throttle_program_registry_cleanup();
    syscall_throttle_uid_registry_cleanup();

    pr_info("syscall_throttle: device deregistrato\n");
}

module_init(syscall_throttle_init);
module_exit(syscall_throttle_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonardo Polidori");
MODULE_DESCRIPTION("Linux kernel module for syscall throttling");