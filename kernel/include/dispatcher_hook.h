#ifndef SYSCALL_THROTTLE_DISPATCHER_HOOK_H
#define SYSCALL_THROTTLE_DISPATCHER_HOOK_H

/*
 * Risolve la syscall table e installa la Kprobe
 * persistente su x64_sys_call.
 */
int syscall_throttle_dispatcher_hook_init(void);

/*
 * Rimuove la Kprobe e attende il completamento dei
 * dispatcher già rediretti.
 */
void syscall_throttle_dispatcher_hook_exit(void);

#endif