#include "myulib.h"

// 标准文件描述符定义
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


void print(const char *s)
{
    int len = 0;
    // 计算字符串长度
    while (s[len])
        len++;
    // 使用write系统调用输出到STDOUT
    my_write(STDOUT_FILENO, s, len);
}

void print_int(int x)
{
    char buffer[32];
    int i = 0;
    int is_negative = 0;

    if (x < 0)
    {
        is_negative = 1;
        x = -x;
    }

    // 将数字转换为字符串（逆序）
    do
    {
        buffer[i++] = '0' + (x % 10);
        x /= 10;
    } while (x > 0);

    if (is_negative)
        buffer[i++] = '-';

    // 反转字符串并输出
    for (int j = i - 1; j >= 0; j--)
    {
        my_write(STDOUT_FILENO, &buffer[j], 1);
    }
}

// 简单的字符串比较函数
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// 简单的字符串复制函数
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

// 简单的字符串长度函数
int strlen(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

// 从标准输入读取一行
int read_line(char *buffer, int max_len)
{
    int i = 0;
    char ch;

    while (i < max_len - 1)
    {
        // 从STDIN读取一个字符
        int bytes_read = my_read(STDIN_FILENO, &ch, 1);
        if (bytes_read <= 0)
        {
            // 读取失败或EOF
            break;
        }

        if (ch == '\n')
        {
            // 遇到换行符，结束输入
            break;
        }
        else if (ch == '\b' || ch == 127) // 退格键
        {
            if (i > 0)
            {
                i--;
                // 回显退格：输出退格+空格+退格来清除字符
                my_write(STDOUT_FILENO, "\b \b", 3);
            }
        }
        else if (ch >= 32 && ch <= 126) // 可打印字符
        {
            buffer[i++] = ch;
            // 回显字符
            my_write(STDOUT_FILENO, &ch, 1);
        }
        // 忽略其他控制字符
    }

    buffer[i] = '\0';
    return i;
}

// 解析命令行参数
int parse_command(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *token = line;

    while (*token && argc < max_args - 1)
    {
        // 跳过空格
        while (*token == ' ' || *token == '\t')
            token++;

        if (*token == '\0')
            break;

        argv[argc++] = token;

        // 找到下一个空格或字符串结尾
        while (*token && *token != ' ' && *token != '\t')
            token++;

        if (*token)
        {
            *token = '\0';
            token++;
        }
    }

    argv[argc] = (char *)0;
    return argc;
}

// 执行内置命令
int execute_builtin(int argc, char **argv)
{
    if (argc == 0)
        return 0;

    if (strcmp(argv[0], "pwd") == 0)
    {
        // 显示当前工作目录
        print("Current directory: /\n");
        return 1;
    }
    else if (strcmp(argv[0], "cd") == 0)
    {
        // 改变目录
        if (argc > 1)
        {
            if (my_chdir(argv[1]) == 0)
            {
                print("Changed to directory: ");
                print(argv[1]);
                print("\n");
            }
            else
            {
                print("cd: cannot access '");
                print(argv[1]);
                print("': No such file or directory\n");
            }
        }
        else
        {
            my_chdir("/");
            print("Changed to root directory\n");
        }
        return 1;
    }
    else if (strcmp(argv[0], "echo") == 0)
    {
        // 回显命令
        for (int i = 1; i < argc; i++)
        {
            if (i > 1)
                my_write(STDOUT_FILENO, " ", 1);
            print(argv[i]);
        }
        my_write(STDOUT_FILENO, "\n", 1);
        return 1;
    }
    else if (strcmp(argv[0], "help") == 0)
    {
        print("Available commands:\n");
        print("  pwd       - show current directory\n");
        print("  cd [dir]  - change directory\n");
        print("  echo ...  - print arguments\n");
        print("  ls        - list files (if available)\n");
        print("  help      - show this help\n");
        print("  exit      - exit shell\n");
        return 1;
    }
    else if (strcmp(argv[0], "exit") == 0)
    {
        print("Goodbye!\n");
        my_exit(0);
        return 1;
    }
    else if (strcmp(argv[0], "ls") == 0)
    {
        print("ls: command not fully implemented\n");
        print("(file listing requires directory reading syscalls)\n");
        return 1;
    }

    return 0; // 不是内置命令
}

// 简单的shell主循环
void simple_shell()
{
    char command_line[256];
    char *argv[32];
    int argc;

    print("Simple Shell v1.0\n");
    print("Type 'help' for available commands, 'exit' to quit\n\n");

    while (1)
    {
        print("$ ");

        // 读取命令行
        int len = read_line(command_line, sizeof(command_line));
        if (len == 0)
        {
            my_write(STDOUT_FILENO, "\n", 1);
            continue;
        }

        my_write(STDOUT_FILENO, "\n", 1);

        // 解析命令
        argc = parse_command(command_line, argv, 32);
        if (argc == 0)
            continue;

        // 执行内置命令
        if (execute_builtin(argc, argv))
            continue;

        // 尝试执行外部命令
        print("Trying to execute: ");
        print(argv[0]);
        print("\n");

        int pid = my_fork();
        if (pid == 0)
        {
            // 子进程：尝试执行命令
            char *envp[] = {"PATH=/bin:/usr/bin", "HOME=/", (char *)0};

            // 尝试不同的路径
            char full_path[256];
            strcpy(full_path, "/bin/");
            strcpy(full_path + 5, argv[0]);

            if (my_execve(full_path, argv, envp) < 0)
            {
                strcpy(full_path, "/usr/bin/");
                strcpy(full_path + 9, argv[0]);
                if (my_execve(full_path, argv, envp) < 0)
                {
                    if (my_execve(argv[0], argv, envp) < 0)
                    {
                        print("Command not found: ");
                        print(argv[0]);
                        print("\n");
                        my_exit(1);
                    }
                }
            }
        }
        else if (pid > 0)
        {
            // 父进程：等待子进程完成
            // 注意：这里应该用 waitpid，但我们暂时简单等待
            my_sleep(1);
        }
        else
        {
            print("Fork failed\n");
        }
    }
}

int main()
{
    simple_shell();
    return 0;
}
