#ifndef SYSCALL_THROTTLE_CONFIG_H
#define SYSCALL_THROTTLE_CONFIG_H

#include <linux/types.h>

/*
 * Operazioni ioctl relative alla configurazione generale.
 */
long syscall_throttle_config_set_max(unsigned long arg);
long syscall_throttle_config_get_max(unsigned long arg);

long syscall_throttle_config_enable_monitor(void);
long syscall_throttle_config_disable_monitor(void);
long syscall_throttle_config_get_monitor(unsigned long arg);

/*
 * Funzioni utilizzate internamente dal modulo per leggere
 * la configurazione corrente.
 */
__u32 syscall_throttle_config_max_value(void);
bool syscall_throttle_config_monitor_enabled(void);

#endif