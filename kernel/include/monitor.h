#ifndef SYSCALL_THROTTLE_MONITOR_H
#define SYSCALL_THROTTLE_MONITOR_H

/*
 * Collega il modulo al tracepoint sys_enter.
 */
int syscall_throttle_monitor_init(void);

/*
 * Scollega il modulo dal tracepoint e attende
 * la conclusione delle callback ancora in esecuzione.
 */
void syscall_throttle_monitor_exit(void);

#endif /* SYSCALL_THROTTLE_MONITOR_H */