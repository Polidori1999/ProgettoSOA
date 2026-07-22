#ifndef SYSCALL_THROTTLE_CONTROLLER_H
#define SYSCALL_THROTTLE_CONTROLLER_H

/*
 * Analizza il comando richiesto dall'utente,
 * apre il device ed esegue il relativo ioctl.
 */
int syscall_throttle_controller_run(int argc, char *argv[]);

#endif /* SYSCALL_THROTTLE_CONTROLLER_H */