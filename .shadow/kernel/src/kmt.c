#include <common.h>
#include <os.h>
#include <limits.h>
#define MAX_CPU 32
#define MAX_TASK 128
// 任务链表
static spinlock_t task_lock;

static struct cpu
{
    int noff;
    int intena;
    task_t *current_task;
} cpus[MAX_CPU];

static task_t tasks[MAX_TASK];

// 系统栈大小 (64KB)
#define STACK_SIZE (1 << 16)

// 保护栅栏，用于检测栈溢出
#define FENCE_PATTERN 0xABCDABCD

// 任务状态定义
#define TASK_READY 1
#define TASK_RUNNING 2
#define TASK_BLOCKED 3
#define TASK_DEAD 4
// 获取当前CPU上的任务
static task_t *get_current_task()
{
    int cpu = cpu_current();
    return cpus[cpu].current_task;
}

// 设置当前CPU上的任务
static void set_current_task(task_t *task)
{
    int cpu = cpu_current();
    cpus[cpu].current_task = task;
    if (task)
        task->cpu = cpu;
}

// 保存上下文
static Context *kmt_context_save(Event ev, Context *ctx)
{
    task_t *current = get_current_task();
    if (current)
    {
        current->context = ctx;
    }
    return NULL; // 返回NULL表示需要继续调用其他中断处理函数
}

// 任务调度
static Context *kmt_schedule(Event ev, Context *ctx)
{
    kmt->spin_lock(&task_lock);

    // 获取当前任务
    task_t *current = get_current_task();
    if (current)
    {
        current->status = TASK_READY; // 将当前任务状态设置为就绪
    }
    // 寻找下一个可运行的任务
    task_t *next = NULL;
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i].status == TASK_READY)
        {
            next = &tasks[i];
            break;
        }
    }
    if (next)
    {
        next->status = TASK_RUNNING;
        set_current_task(next);
        kmt->spin_unlock(&task_lock);
        return next->context;
    }
    // 没有可运行的任务，保持当前任务运行
    if (current)
    {
        current->status = TASK_RUNNING;
    }
    kmt->spin_unlock(&task_lock);
    return ctx;
}

// 初始化KMT模块
static void kmt_init()
{
    printf("KMT模块初始化\n");
    kmt->spin_init(&task_lock, "task_lock");
    // 注册中断处理器，用于任务调度
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
}

// // 创建任务
// static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
// {
//     if (!task || !name || !entry)
//         return -1;

//     // 分配栈空间
//     task->stack = pmm->alloc(STACK_SIZE + sizeof(task->fence1));
//     if (!task->stack)
//         return -1;

//     // 设置栈底栅栏，用于检测栈溢出
//     task->fence1 = (void *)FENCE_PATTERN;

//     // 初始化任务上下文
//     Area stack_area = {
//         .start = task->stack,
//         .end = (void *)((uintptr_t)task->stack + STACK_SIZE)};
//     task->context = kcontext(stack_area, entry, arg);
//     task->name = name;
//     task->status = TASK_READY;
//     task->id = (int)((uintptr_t)task); // 使用指针地址作为唯一ID
//     task->cpu = -1;                    // 初始未分配到CPU

//     // 添加到任务链表
//     kmt_spin_lock(&task_lock);
//     if (!task_head)
//     {
//         task_head = task;
//         task->next = task;
//         task->prev = task;
//     }
//     else
//     {
//         task->next = task_head;
//         task->prev = task_head->prev;
//         task_head->prev->next = task;
//         task_head->prev = task;
//     }
//     kmt_spin_unlock(&task_lock);

//     printf("创建任务: %s (ID: %d)\n", name, task->id);
//     return 0; // 成功
// }

// // 销毁任务
// static void kmt_teardown(task_t *task)
// {
//     if (!task)
//         return;

//     printf("销毁任务: %s\n", task->name);

//     kmt_spin_lock(&task_lock);

//     // 从任务链表中删除
//     if (task->next == task)
//     { // 只有一个任务
//         task_head = NULL;
//     }
//     else
//     {
//         if (task_head == task)
//         {
//             task_head = task->next;
//         }
//         task->prev->next = task->next;
//         task->next->prev = task->prev;
//     }

//     // 释放栈空间
//     if (task->stack)
//     {
//         pmm->free(task->stack);
//     }

//     task->status = TASK_DEAD;
//     kmt_spin_unlock(&task_lock);
// }

// 初始化自旋锁
static void kmt_spin_init(spinlock_t *lk, const char *name)
{
    if (!lk)
        return;
    lk->locked = 0;
    lk->name = name;
    lk->cpu = -1;
}
static bool holding(spinlock_t *lk)
{
    panic_on(!lk, "Spinlock is NULL");
    return lk->locked && lk->cpu == cpu_current();
}
static void push_off()
{
    int cpu = cpu_current();
    int old = ienabled();
    iset(false);
    if (cpus[cpu].noff == 0)
    {
        cpus[cpu].intena = old;
    }
    cpus[cpu].noff++;
}

static void pop_off()
{
    int cpu = cpu_current();
    panic_on(cpus[cpu].noff == 0, "pop_off: no push_off");
    cpus[cpu].noff--;
    if (cpus[cpu].noff == 0)
    {
        iset(cpus[cpu].intena);
    }
}
// 获取自旋锁
static void kmt_spin_lock(spinlock_t *lk)
{
    panic_on(!lk, "Spinlock is NULL");
    // 禁用中断并保存中断状态
    push_off();
    if (holding(lk))
    {
        panic("Spinlock is already held by current CPU");
    }
    // 等待锁可用
    while (atomic_xchg(&lk->locked, 1))
        ;
    // 记录锁持有信息
    lk->cpu = cpu_current();
}

// 释放自旋锁
static void kmt_spin_unlock(spinlock_t *lk)
{
    panic_on(!lk, "Spinlock is NULL");
    if (!holding(lk))
    {
        panic("Spinlock is not held by current CPU");
    }
    lk->cpu = -1;
    // 释放锁
    atomic_xchg(&lk->locked, 0);
    pop_off();
}

// // 初始化信号量
// static void kmt_sem_init(sem_t *sem, const char *name, int value)
// {
//     if (!sem)
//         return;

//     sem->value = value;
//     sem->name = name;
//     kmt_spin_init(&sem->lock, name);

//     // 初始化等待队列
//     sem->queue = NULL; // 在实际实现中应该是一个队列结构
// }

// // 等待信号量
// static void kmt_sem_wait(sem_t *sem)
// {
//     if (!sem)
//         return;

//     while (1)
//     {
//         kmt_spin_lock(&sem->lock);
//         if (sem->value > 0)
//         {
//             sem->value--;
//             kmt_spin_unlock(&sem->lock);
//             break;
//         }

//         // 将当前任务设置为阻塞状态
//         task_t *current = get_current_task();
//         if (current)
//         {
//             current->status = TASK_BLOCKED;

//             // 在实际实现中应将任务加入等待队列
//             // sem->queue = ...
//         }

//         kmt_spin_unlock(&sem->lock);
//         yield(); // 让出CPU

//         // 被唤醒后再次尝试获取信号量
//     }
// }

// // 释放信号量
// static void kmt_sem_signal(sem_t *sem)
// {
//     if (!sem)
//         return;

//     kmt_spin_lock(&sem->lock);
//     sem->value++;

//     // 在实际实现中应该从等待队列中唤醒一个任务
//     // 示例伪代码:
//     // if (sem->queue) {
//     //   task_t *task = dequeue(sem->queue);
//     //   task->status = TASK_READY;
//     // }

//     kmt_spin_unlock(&sem->lock);
// }

// 使用MODULE_DEF宏定义kmt模块
MODULE_DEF(kmt) = {
    .init = kmt_init,
    //.create = kmt_create,
    //.teardown = kmt_teardown,
    .spin_init = kmt_spin_init,
    .spin_lock = kmt_spin_lock,
    .spin_unlock = kmt_spin_unlock,
    //.sem_init = kmt_sem_init,
    //.sem_wait = kmt_sem_wait,
    //.sem_signal = kmt_sem_signal,
};
