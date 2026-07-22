#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <syscall_throttle_ioctl.h>

#define DEVICE_PATH "/dev/syscall_throttle"

static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s ping\n"
            "  %s get-max\n"
            "  %s set-max NUMERO\n",
            program_name,
            program_name,
            program_name);
}

static int parse_max(const char *text, __u32 *value)
{
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

    *value = (__u32)parsed;
    return 0;
}

int main(int argc, char *argv[])
{
    __u32 value;
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
    } else if (strcmp(argv[1], "ping") == 0 ||
               strcmp(argv[1], "get-max") == 0) {
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
    } else {
        if (ioctl(fd,
                  SYSCALL_THROTTLE_IOC_SET_MAX,
                  &value) == -1) {
            perror("ioctl SET_MAX fallito");
            status = 1;
        } else {
            printf("MAX impostato a %u.\n", value);
        }
    }

    if (close(fd) == -1) {
        perror("Chiusura del device fallita");
        status = 1;
    }

    return status;
}
