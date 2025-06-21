#include "ulib.h"

int main(int argc, char *argv[])
{
    // Simple init that just execs sh
    char *sh_argv[] = {"/bin/sh", NULL};

    // Use execve to replace this process with sh
    syscall(SYS_execve, "/bin/sh", sh_argv, NULL);
    // If execve fails, try sh without path
    char *sh_argv2[] = {"sh", NULL};
    syscall(SYS_execve, "sh", sh_argv2, NULL);
    // If both fail, exit
    syscall(SYS_exit, 1);
    return 0; // Never reached
}