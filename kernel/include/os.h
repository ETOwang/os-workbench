// When we test your kernel implementation, the framework
// directory will be replaced. Any modifications you make
// to its files (e.g., kernel.h) will be lost.

// Note that some code requires data structure definitions
// (such as `sem_t`) to compile, and these definitions are
// not present in kernel.h.

// Include these definitions in os.h.
#ifndef __OS_H__
#define __OS_H__
// 前向声明的结构体定义
struct spinlock
{
    int locked;       // 锁状态
    const char *name; // 锁名称
    int cpu;          // 持有锁的CPU
};
struct task
{
    procinfo_t *pi;
    spinlock_t lock;        // 任务锁
    Context *context;       // 上下文
    const char *name;       // 任务名称
    int status;             // 任务状态
    int cpu;                // 运行的CPU
    task_t *next;           // 下一个任务
    void *fence;            // 用于检测栈溢出的栅栏
    char stack[STACK_SIZE]; // 任务栈区域

    // 新增的进程级文件描述符表
    struct file *open_files[NOFILE];
};
struct semaphore
{
    int value;         // 信号量值
    const char *name;  // 信号量名称
    spinlock_t lock;   // 信号量内部锁
    task_t *wait_list; // 等待信号量的任务列表
};
struct procinfo
{
    int pid;
    int xstate;
    task_t *parent;
    AddrSpace as;
    char *cwd;
    void *brk;
};
struct handler_record
{
    int seq;           // 处理器序列号
    int event;         // 事件类型
    handler_t handler; // 处理函数指针
};
typedef struct handler_record handler_record_t;
#endif // __OS_H__