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

static void print_usage(const char *program_name) {
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
            "  %s uid-list\n",
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

int syscall_throttle_controller_run(int argc, char *argv[]){
    struct syscall_throttle_uid_list uid_list;
    __u32 value;
    __u32 i;
    int fd;
    int status;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "set-max") == 0) {
        if (argc != 3 || parse_max(argv[2], &value) != 0) {
            fprintf(stderr,
                    "Errore: MAX deve essere un intero positivo.\n");
            return 1;
        }
    } else if (strcmp(argv[1], "uid-add") == 0 ||
               strcmp(argv[1], "uid-remove") == 0) {
        if (argc != 3 || parse_uid(argv[2], &value) != 0) {
            fprintf(stderr,
                    "Errore: UID non valido.\n");
            return 1;
        }
               } else if (strcmp(argv[1], "ping") == 0 ||
                          strcmp(argv[1], "get-max") == 0 ||
                          strcmp(argv[1], "monitor-on") == 0 ||
                          strcmp(argv[1], "monitor-off") == 0 ||
                          strcmp(argv[1], "monitor-status") == 0 ||
                          strcmp(argv[1], "uid-list") == 0) {
                   if (argc != 2) {
                       print_usage(argv[0]);
                       return 1;
                   }
                          } else {
                              print_usage(argv[0]);
                              return 1;
                          }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("Impossibile aprire " DEVICE_PATH);
        return 1;
    }

    status = 0;

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
                          for (i = 0; i < uid_list.count; ++i)
                              printf("  %u\n", uid_list.uids[i]);
                      }
                  }

        if (close(fd) == -1) {
            perror("Chiusura del device fallita");
            status = 1;
        }

        return status;
    }
}
