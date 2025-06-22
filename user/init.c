#include "ulib.h"

int main(int argc, char *argv[])
{
    // Simple init that just execs sh
    char *sh_argv[] = {"/bin/sh", NULL};
    // Use execve to replace this process with sh
    syscall(SYS_execve, (long)"/bin/sh", (long)sh_argv, (long)NULL, 0);
    // If execve fails, try sh without path
    char *sh_argv2[] = {"sh", NULL};
    syscall(SYS_execve, (long)"sh", (long)sh_argv2, (long)NULL, 0);
    // If both fail, exit
    syscall(SYS_exit, 1, 0, 0, 0);
    return 0; // Never reached
}