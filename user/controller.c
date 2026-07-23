#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "controller.h"

#include <syscall_throttle_ioctl.h>

#define DEVICE_PATH "/dev/syscall_throttle"

static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s ping\n"
            "  %s get-max\n"
            "  %s set-max NUMERO\n"
            "  %s monitor-on\n"
            "  %s monitor-off\n"
            "  %s monitor-status\n"
            "  %s uid-add UID\n"
            "  %s uid-remove UID\n"
            "  %s uid-list\n"
            "  %s program-add NOME\n"
            "  %s program-remove NOME\n"
            "  %s program-list\n"
            "  %s syscall-add NUMERO\n"
            "  %s syscall-remove NUMERO\n"
            "  %s syscall-list\n",
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name,
            program_name);
}

static int parse_max(const char *text, __u32 *value) {
    unsigned long parsed;
    char *end;

    errno = 0;
    end = NULL;

    parsed = strtoul(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed == 0 ||
        parsed > UINT_MAX) {
        return -1;
    }

    *value = (__u32) parsed;
    return 0;
}

static int parse_uid(const char *text, __u32 *value) {
    unsigned long parsed;
    char *end;

    errno = 0;
    end = NULL;

    parsed = strtoul(text, &end, 10);

    /*
     * A differenza di MAX, l'UID zero è valido.
     */
    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed > UINT_MAX) {
        return -1;
    }

    *value = (__u32) parsed;
    return 0;
}


static int parse_syscall_number(
    const char *text,
    __u32 *value) {
    unsigned long parsed;
    char *end;

    errno = 0;
    end = NULL;

    parsed = strtoul(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed > UINT_MAX) {
        return -1;
    }

    *value = (__u32) parsed;
    return 0;
}

static int parse_program_name(
    const char *text,
    struct syscall_throttle_program *program) {
    size_t length;

    length = strlen(text);

    if (length == 0 ||
        length >= SYSCALL_THROTTLE_PROGRAM_NAME_LEN) {
        return -1;
    }

    /*
     * Azzera tutta la struttura, garantendo la presenza
     * del terminatore '\0' dopo il nome copiato.
     */
    memset(program, 0, sizeof(*program));
    memcpy(program->name, text, length);

    return 0;
}

int syscall_throttle_controller_run(int argc, char *argv[]) {
    struct syscall_throttle_uid_list uid_list;
    struct syscall_throttle_program program;
    struct syscall_throttle_program_list program_list;
    struct syscall_throttle_syscall_list syscall_list;
    __u32 value;
    __u32 i;
    int fd;
    int status;

    /*
     * Prima fase: validazione del comando e dei suoi argomenti.
     * Il device non viene ancora aperto.
     */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "set-max") == 0) {
        if (argc != 3 ||
            parse_max(argv[2], &value) != 0) {
            fprintf(stderr,
                    "Errore: MAX deve essere un intero positivo.\n");
            return 1;
        }
    } else if (strcmp(argv[1], "uid-add") == 0 ||
               strcmp(argv[1], "uid-remove") == 0) {
        if (argc != 3 ||
            parse_uid(argv[2], &value) != 0) {
            fprintf(stderr,
                    "Errore: UID non valido.\n");
            return 1;
        }
    } else if (strcmp(argv[1], "program-add") == 0 ||
               strcmp(argv[1], "program-remove") == 0) {
        if (argc != 3 ||
            parse_program_name(argv[2], &program) != 0) {
            fprintf(stderr,
                    "Errore: il nome deve contenere "
                    "da 1 a 15 caratteri.\n");
            return 1;
        }
    } else if (strcmp(argv[1], "syscall-add") == 0 ||
               strcmp(argv[1], "syscall-remove") == 0) {
        if (argc != 3 ||
            parse_syscall_number(argv[2], &value) != 0) {
            fprintf(stderr,
                    "Errore: numero di syscall non valido.\n");
            return 1;
        }
    } else if (strcmp(argv[1], "ping") == 0 ||
               strcmp(argv[1], "get-max") == 0 ||
               strcmp(argv[1], "monitor-on") == 0 ||
               strcmp(argv[1], "monitor-off") == 0 ||
               strcmp(argv[1], "monitor-status") == 0 ||
               strcmp(argv[1], "uid-list") == 0 ||
               strcmp(argv[1], "program-list") == 0 ||
               strcmp(argv[1], "syscall-list") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }
    } else {
        print_usage(argv[0]);
        return 1;
    }

    /*
     * Seconda fase: apertura del device.
     */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("Impossibile aprire " DEVICE_PATH);
        return 1;
    }

    status = 0;

    /*
     * Terza fase: esecuzione dell'ioctl associato al comando.
     */
    if (strcmp(argv[1], "ping") == 0) {
        if (ioctl(fd, SYSCALL_THROTTLE_IOC_PING) == -1) {
            perror("ioctl PING fallito");
            status = 1;
        } else {
            printf("PING completato correttamente.\n");
        }
    } else if (strcmp(argv[1], "get-max") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_GET_MAX,
                  &value) == -1) {
            perror("ioctl GET_MAX fallito");
            status = 1;
        } else {
            printf("MAX corrente: %u\n", value);
        }
    } else if (strcmp(argv[1], "set-max") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_SET_MAX,
                  &value) == -1) {
            perror("ioctl SET_MAX fallito");
            status = 1;
        } else {
            printf("MAX impostato a %u.\n", value);
        }
    } else if (strcmp(argv[1], "monitor-on") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_ENABLE_MONITOR) == -1) {
            perror("ioctl ENABLE_MONITOR fallito");
            status = 1;
        } else {
            printf("Monitor attivato.\n");
        }
    } else if (strcmp(argv[1], "monitor-off") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_DISABLE_MONITOR) == -1) {
            perror("ioctl DISABLE_MONITOR fallito");
            status = 1;
        } else {
            printf("Monitor disattivato.\n");
        }
    } else if (strcmp(argv[1], "monitor-status") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_GET_MONITOR,
                  &value) == -1) {
            perror("ioctl GET_MONITOR fallito");
            status = 1;
        } else {
            printf("Monitor: %s\n",
                   value != 0 ? "attivo" : "disattivo");
        }
    } else if (strcmp(argv[1], "uid-add") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_REGISTER_UID,
                  &value) == -1) {
            perror("ioctl REGISTER_UID fallito");
            status = 1;
        } else {
            printf("UID %u registrato.\n", value);
        }
    } else if (strcmp(argv[1], "uid-remove") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
                  &value) == -1) {
            perror("ioctl UNREGISTER_UID fallito");
            status = 1;
        } else {
            printf("UID %u deregistrato.\n", value);
        }
    } else if (strcmp(argv[1], "uid-list") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_GET_UIDS,
                  &uid_list) == -1) {
            perror("ioctl GET_UIDS fallito");
            status = 1;
        } else {
            printf("UID registrati: %u\n", uid_list.count);

            if (uid_list.count == 0) {
                printf("  nessuno\n");
            } else {
                for (i = 0; i < uid_list.count; ++i) {
                    printf("  %u\n", uid_list.uids[i]);
                }
            }
        }
    } else if (strcmp(argv[1], "program-add") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
                  &program) == -1) {
            perror("ioctl REGISTER_PROGRAM fallito");
            status = 1;
        } else {
            printf("Programma '%s' registrato.\n",
                   program.name);
        }
        } else if (strcmp(argv[1], "program-remove") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
                  &program) == -1) {
            perror("ioctl UNREGISTER_PROGRAM fallito");
            status = 1;
        } else {
            printf("Programma '%s' deregistrato.\n",
                   program.name);
        }

    } else if (strcmp(argv[1], "program-list") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_GET_PROGRAMS,
                  &program_list) == -1) {
            perror("ioctl GET_PROGRAMS fallito");
            status = 1;
        } else {
            printf("Programmi registrati: %u\n",
                   program_list.count);

            if (program_list.count == 0) {
                printf("  nessuno\n");
            } else {
                for (i = 0; i < program_list.count; ++i) {
                    printf("  %s\n",
                           program_list.programs[i].name);
                }
            }
        }

    } else if (strcmp(argv[1], "syscall-add") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
                  &value) == -1) {
            perror("ioctl REGISTER_SYSCALL fallito");
            status = 1;
        } else {
            printf("Syscall %u registrata.\n", value);
        }

    } else if (strcmp(argv[1], "syscall-remove") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
                  &value) == -1) {
            perror("ioctl UNREGISTER_SYSCALL fallito");
            status = 1;
        } else {
            printf("Syscall %u deregistrata.\n", value);
        }

    } else if (strcmp(argv[1], "syscall-list") == 0) {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_GET_SYSCALLS,
                  &syscall_list) == -1) {
            perror("ioctl GET_SYSCALLS fallito");
            status = 1;
        } else {
            printf("Syscall registrate: %u\n",
                   syscall_list.count);

            if (syscall_list.count == 0) {
                printf("  nessuna\n");
            } else {
                for (i = 0; i < syscall_list.count; ++i) {
                    printf("  %u\n",
                           syscall_list.numbers[i]);
                }
            }
        }
    }

    /*
     * La chiusura viene eseguita per qualunque comando.
     */
    if (close(fd) == -1) {
        perror("Chiusura del device fallita");
        status = 1;
    }

    return status;
}