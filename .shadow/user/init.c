// Linux port of xv6-riscv shell (no libc)

#include "myulib.h"
#include <fcntl.h>

// EXEC:   ls
// REDIR:  ls > a.txt
// PIPE:   ls | wc -l
// LIST:   (ls ; ls)
enum
{
    EXEC = 1,
    REDIR,
    PIPE,
    LIST,
    BACK
};

#define MAXARGS 10
#define NULL ((void *)0)

struct cmd
{
    int type;
};

struct execcmd
{
    int type;
    char *argv[MAXARGS], *eargv[MAXARGS];
};

struct redircmd
{
    int type, fd, mode;
    char *file, *efile;
    struct cmd *cmd;
};

struct pipecmd
{
    int type;
    struct cmd *left, *right;
};

struct listcmd
{
    int type;
    struct cmd *left, *right;
};

struct backcmd
{
    int type;
    struct cmd *cmd;
};

struct cmd *parsecmd(char *);

// cmd is the "abstract syntax tree" (AST) of the command;
// runcmd() never returns.
void runcmd(struct cmd *cmd)
{
    print("runcmd 64\n", NULL);
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        syscall(SYS_exit, 1);

    switch (cmd->type)
    {
    case EXEC:
        ecmd = (struct execcmd *)cmd;
        if (ecmd->argv[0] == 0)
            syscall(SYS_exit, 1);
        char *c = zalloc(5 + strlen(ecmd->argv[0]) + 1);
        strcpy(c, "/bin/");
        strcpy(c + strlen(c), ecmd->argv[0]);
        syscall(SYS_execve, c, ecmd->argv, NULL);
        print("fail to exec ", c, "\n", NULL);
        break;

    case REDIR:
        rcmd = (struct redircmd *)cmd;
        syscall(SYS_close, rcmd->fd);
        if (syscall(SYS_openat, AT_FDCWD, rcmd->file, rcmd->mode, 0644) < 0)
        {
            print("fail to open ", rcmd->file, "\n", NULL);
            syscall(SYS_exit, 1);
        }
        runcmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd *)cmd;
        if (syscall(SYS_fork) == 0)
            runcmd(lcmd->left);
        syscall(SYS_wait4, -1, 0, 0, 0);
        runcmd(lcmd->right);
        break;

        // case PIPE:
        //     pcmd = (struct pipecmd *)cmd;
        //     assert(syscall(SYS_pipe, p) >= 0);
        //     if (syscall(SYS_fork) == 0) {
        //         syscall(SYS_close, 1);
        //         syscall(SYS_dup, p[1]);
        //         syscall(SYS_close, p[0]);
        //         syscall(SYS_close, p[1]);
        //         runcmd(pcmd->left);
        //     }
        //     if (syscall(SYS_fork) == 0) {
        //         syscall(SYS_close, 0);
        //         syscall(SYS_dup, p[0]);
        //         syscall(SYS_close, p[0]);
        //         syscall(SYS_close, p[1]);
        //         runcmd(pcmd->right);
        //     }
        //     syscall(SYS_close, p[0]);
        //     syscall(SYS_close, p[1]);
        //     syscall(SYS_wait4, -1, 0, 0, 0);
        //     syscall(SYS_wait4, -1, 0, 0, 0);
        //     break;

    case BACK:
        bcmd = (struct backcmd *)cmd;
        if (syscall(SYS_fork) == 0)
            runcmd(bcmd->cmd);
        break;

    default:
        assert(0);
    }
    syscall(SYS_exit, 0);
}

int getcmd(char *buf, int nbuf)
{
    syscall(SYS_getcwd, buf, nbuf);
    print("(sh-xv6):", NULL);
    print(buf,"$ ",NULL);
    for (int i = 0; i < nbuf; i++)
    {
        buf[i] = '\0';
    }
    int nread = syscall(SYS_read, 0, buf, nbuf);
    if (nread <= 0)
        return -1;
    return 0;
}
void main()
{
    static char buf[100];

    // Read and run input commands.
    while (getcmd(buf, sizeof(buf)) >= 0)
    {
        print(buf, NULL);
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
        {
            // Chdir must be called by the parent, not the child.
            buf[strlen(buf) - 1] = 0; // chop \n
            if (syscall(SYS_chdir, buf + 3) < 0)
                print("cannot cd ", buf + 3, "\n", NULL);
            continue;
        }
        if (syscall(SYS_fork) == 0)
            runcmd(parsecmd(buf));
        syscall(SYS_wait4, -1, 0, 0, 0);
    }
}

struct cmd *execcmd(void)
{
    struct execcmd *cmd;

    cmd = zalloc(sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
                     int fd)
{
    struct redircmd *cmd;

    cmd = zalloc(sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = zalloc(sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = zalloc(sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = zalloc(sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd *)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch (*s)
    {
    case 0:
        break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
        s++;
        break;
    case '>':
        s++;
        if (*s == '>')
        {
            ret = '+';
            s++;
        }
        break;
    default:
        ret = 'a';
        while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }
    if (eq)
        *eq = s;

    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return ret;
}

int peek(char **ps, char *es, char *toks)
{
    char *s;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s)
{
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    peek(&s, es, "");
    assert(s == es);
    nulterminate(cmd);
    print("parse cmd finish\n", NULL);
    return cmd;
}

struct cmd *parseline(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parsepipe(ps, es);
    while (peek(ps, es, "&"))
    {
        gettoken(ps, es, 0, 0);
        cmd = backcmd(cmd);
    }
    if (peek(ps, es, ";"))
    {
        gettoken(ps, es, 0, 0);
        cmd = listcmd(cmd, parseline(ps, es));
    }
    return cmd;
}

struct cmd *parsepipe(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parseexec(ps, es);
    if (peek(ps, es, "|"))
    {
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es)
{
    int tok;
    char *q, *eq;

    while (peek(ps, es, "<>"))
    {
        tok = gettoken(ps, es, 0, 0);
        assert(gettoken(ps, es, &q, &eq) == 'a');
        switch (tok)
        {
        case '<':
            cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
            break;
        case '>':
            cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREAT | O_TRUNC, 1);
            break;
        case '+': // >>
            cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREAT, 1);
            break;
        }
    }
    return cmd;
}

struct cmd *parseblock(char **ps, char *es)
{
    struct cmd *cmd;

    assert(peek(ps, es, "("));
    gettoken(ps, es, 0, 0);
    cmd = parseline(ps, es);
    assert(peek(ps, es, ")"));
    gettoken(ps, es, 0, 0);
    cmd = parseredirs(cmd, ps, es);
    return cmd;
}

struct cmd *parseexec(char **ps, char *es)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    if (peek(ps, es, "("))
        return parseblock(ps, es);

    ret = execcmd();
    cmd = (struct execcmd *)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    while (!peek(ps, es, "|)&;"))
    {
        if ((tok = gettoken(ps, es, &q, &eq)) == 0)
            break;
        assert(tok == 'a');
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        assert(++argc < MAXARGS);
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
    return ret;
}

// NUL-terminate all the counted strings.
struct cmd *nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        return 0;

    switch (cmd->type)
    {
    case EXEC:
        ecmd = (struct execcmd *)cmd;
        for (i = 0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
        break;

    case REDIR:
        rcmd = (struct redircmd *)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;

    case PIPE:
        pcmd = (struct pipecmd *)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;

    case LIST:
        lcmd = (struct listcmd *)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;

    case BACK:
        bcmd = (struct backcmd *)cmd;
        nulterminate(bcmd->cmd);
        break;
    }
    return cmd;
}


// #include "myulib.h"

//     // 全局变量测试
//     int global_var = 42;
// char global_string[] = "Hello from global";

// // 测试基本fork功能
// void test_basic_fork()
// {
//     print("=== Test 1: Basic Fork ===\n", NULL);

//     int local_var = 100;
//     print("Before fork: global_var=", NULL);
//     print_int(global_var);
//     print(", local_var=", NULL);
//     print_int(local_var);
//     print("\n", NULL);

//     int pid = syscall(SYS_fork);
//     print("Fork returned: ", NULL);
//     print_int(pid);
//     print("\n", NULL);

//     if (pid == 0)
//     {
//         // 子进程
//         print("Child: I am child process\n", NULL);
//         print("Child: global_var=", NULL);
//         print_int(global_var);
//         print(", local_var=", NULL);
//         print_int(local_var);
//         print("\n", NULL);

//         // 修改变量测试写时复制
//         global_var = 999;
//         local_var = 888;
//         print("Child: After modification: global_var=", NULL);
//         print_int(global_var);
//         print(", local_var=", NULL);
//         print_int(local_var);
//         print("\n", NULL);

//         print("Child: Exiting...\n", NULL);
//         syscall(SYS_exit, 42);
//     }
//     else if (pid > 0)
//     {
//         // 父进程
//         print("Parent: I am parent process, child pid=", NULL);
//         print_int(pid);
//         print("\n", NULL);

//         print("Parent: global_var=", NULL);
//         print_int(global_var);
//         print(", local_var=", NULL);
//         print_int(local_var);
//         print("\n", NULL);

//         print("Parent: Waiting for child...\n", NULL);
//         int status = 0;
//         int wait_pid = syscall(SYS_wait4, -1, &status, 0, 0);
//         print("Parent: Child ", NULL);
//         print_int(wait_pid);
//         print(" finished with status ", NULL);
//         print_int(status);
//         print("\n", NULL);

//         print("Parent: After child exit: global_var=", NULL);
//         print_int(global_var);
//         print(", local_var=", NULL);
//         print_int(local_var);
//         print("\n", NULL);
//     }
//     else
//     {
//         print("Fork failed!\n", NULL);
//     }
// }

// // 测试内存访问
// void test_memory_access()
// {
//     print("\n=== Test 2: Memory Access ===\n", NULL);

//     // 测试栈变量
//     char stack_buffer[100];
//     for (int i = 0; i < 10; i++)
//     {
//         stack_buffer[i] = 'A' + i;
//     }
//     stack_buffer[10] = '\0';

//     print("Before fork: stack_buffer=", stack_buffer, "\n", NULL);

//     int pid = syscall(SYS_fork);

//     if (pid == 0)
//     {
//         // 子进程
//         print("Child: Reading stack_buffer=", stack_buffer, "\n", NULL);

//         // 修改栈变量
//         stack_buffer[0] = 'X';
//         stack_buffer[1] = 'Y';
//         print("Child: Modified stack_buffer=", stack_buffer, "\n", NULL);

//         // 测试全局字符串
//         print("Child: global_string=", global_string, "\n", NULL);
//         global_string[0] = 'Z';
//         print("Child: Modified global_string=", global_string, "\n", NULL);

//         syscall(SYS_exit, 0);
//     }
//     else if (pid > 0)
//     {
//         // 父进程等待
//         syscall(SYS_wait4, -1, 0, 0, 0);
//         print("Parent: After child: stack_buffer=", stack_buffer, "\n", NULL);
//         print("Parent: After child: global_string=", global_string, "\n", NULL);
//     }
// }

// // 测试多次fork
// void test_multiple_fork()
// {
//     print("\n=== Test 3: Multiple Fork ===\n", NULL);

//     for (int i = 0; i < 2; i++)
//     {
//         int pid = syscall(SYS_fork);

//         if (pid == 0)
//         {
//             // 子进程
//             print("Child ", NULL);
//             print_int(i);
//             print(": Hello from child\n", NULL);
//             syscall(SYS_exit, i);
//         }
//         else if (pid > 0)
//         {
//             print("Parent: Created child ", NULL);
//             print_int(i);
//             print(" with pid ", NULL);
//             print_int(pid);
//             print("\n", NULL);
//         }
//         else
//         {
//             print("Fork ", NULL);
//             print_int(i);
//             print(" failed\n", NULL);
//         }
//     }

//     // 等待所有子进程
//     print("Parent: Waiting for all children...\n", NULL);
//     for (int i = 0; i < 2; i++)
//     {
//         int status = 0;
//         int wait_pid = syscall(SYS_wait4, -1, &status, 0, 0);
//         print("Parent: Child ", NULL);
//         print_int(wait_pid);
//         print(" exited with status ", NULL);
//         print_int(status);
//         print("\n", NULL);
//     }
// }

// // 主函数
// void main()
// {
//     print("Starting comprehensive fork tests...\n", NULL);

//     test_basic_fork();
//     test_memory_access();
//     test_multiple_fork();

//     print("\n=== All tests completed ===\n", NULL);
//     syscall(SYS_exit, 0);
// }
