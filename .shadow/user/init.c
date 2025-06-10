#include "myulib.h"

void print(const char *s)
{
    for (int i = 0; s[i]; i++)
    {
        my_kputc(s[i]);
    }
}

void print_int(int x)
{
    if (x < 0)
    {
        my_kputc('-');
        x = -x;
    }
    if (x >= 10)
    {
        print_int(x / 10);
    }
    my_kputc('0' + x % 10);
}

int main()
{
    print("BusyBox Init Process Starting...\n");

    // 文件系统已经在VFS初始化时自动挂载了，不需要重新挂载
    print("Root filesystem already mounted by kernel\n");

    // 切换到根目录
    if (my_chdir("/") < 0)
    {
        print("Warning: Failed to change to root directory\n");
    }

    // 检查BusyBox是否存在
    print("Looking for BusyBox binary...\n");

    // 尝试打开 /bin/busybox
    print("Trying /bin/busybox...\n");
    int busybox_fd = my_openat(AT_FDCWD, "/bin/busybox", O_RDONLY);
    if (busybox_fd < 0)
    {
        print("Failed to open /bin/busybox, trying /busybox...\n");
        busybox_fd = my_openat(AT_FDCWD, "/busybox", O_RDONLY);
        if (busybox_fd < 0)
        {
            print("Failed to open /busybox too\n");
        }
        else
        {
            print("Found /busybox!\n");
        }
    }
    else
    {
        print("Found /bin/busybox!\n");
    }

    if (busybox_fd < 0)
    {
        print("Error: BusyBox binary not found!\n");
        print("Trying to create a simple shell...\n");

        // 简单的shell循环
        while (1)
        {
            print("# ");
            // 这里应该读取用户输入并执行命令
            // 暂时只是一个无限循环
            my_sleep(1);
        }
    }
    else
    {
        my_close(busybox_fd);
        print("BusyBox found! Executing...\n");

        // 准备参数
        char *argv[] = {"busybox", "sh", (char *)0};
        char *envp[] = {"PATH=/bin:/usr/bin", "HOME=/", (char *)0};

        // 尝试执行BusyBox shell
        int exec_result = my_execve("/bin/busybox", argv, envp);
        if (exec_result < 0)
        {
            exec_result = my_execve("/busybox", argv, envp);
        }

        if (exec_result < 0)
        {
            print("Error: Failed to execute BusyBox!\n");
            print("Error code: ");
            print_int(exec_result);
            print("\n");
        }

        // 如果execve失败，进入简单shell
        print("Falling back to simple shell...\n");
        while (1)
        {
            print("# ");
            my_sleep(1);
        }
    }

    return 0;
}
