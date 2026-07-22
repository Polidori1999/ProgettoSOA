#include "controller.h"

int main(int argc, char *argv[])
{
    return syscall_throttle_controller_run(argc, argv);
}