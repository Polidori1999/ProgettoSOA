#ifndef SYSCALL_THROTTLE_DISPATCHER_HOOK_H
#define SYSCALL_THROTTLE_DISPATCHER_HOOK_H

/*
 * Risolve gli indirizzi kernel necessari al futuro
 * hook del dispatcher delle syscall.
 *
 * In questa fase non viene modificato il kernel text.
 */
int syscall_throttle_dispatcher_hook_init(void);

/*
 * Cleanup del sottosistema dispatcher.
 *
 * Al momento non esiste ancora una patch da rimuovere,
 * ma questa funzione rappresenterà il punto di cleanup
 * nelle milestone successive.
 */
void syscall_throttle_dispatcher_hook_exit(void);

#endif