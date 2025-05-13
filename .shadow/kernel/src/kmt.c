#include <common.h>
#include <os.h>
#include <limits.h>
#define MAX_CPU 32
#define MAX_TASK 128
static spinlock_t task_lock;
static task_t *tasks[MAX_TASK];
static task_t monitor_task[MAX_CPU]; // 每个CPU上的监视任务（初始进程）
static int task_index;
static struct cpu
{
    int noff;
    int intena;
    task_t *current_task;
} cpus[MAX_CPU];
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
    task_t *cur = cpus[cpu].current_task;
    // 如果当前没有任务，则返回该CPU的监视任务
    if (cur == NULL)
    {
        cur = &monitor_task[cpu];
    }
    return cur;
}

// 设置当前CPU上的任务
static void set_current_task(task_t *task)
{
    panic_on(task == NULL, "Task is NULL");
    int cpu = cpu_current();
    cpus[cpu].current_task = task;
    task->cpu = cpu;
}
// 保存上下文
static Context *kmt_context_save(Event ev, Context *ctx)
{
    task_t *current = get_current_task();
    panic_on(current == NULL, "Current task is NULL");
    current->context = ctx;
    return NULL; // 返回NULL表示需要继续调用其他中断处理函数
}

// 任务调度
static Context *kmt_schedule(Event ev, Context *ctx)
{
    kmt->spin_lock(&task_lock);

    // 获取当前任务
    task_t *current = get_current_task();
    kmt->spin_lock(&current->lock);
    // 更新当前任务状态
    if (current->status == TASK_RUNNING)
    {
        current->status = TASK_READY;
    }

    // 查找下一个可运行的任务
    task_t *next = NULL;
    int current_search_start_index = task_index; // Record the starting point of search

    // 尝试从普通任务队列中查找
    for (int i = 0; i < MAX_TASK; ++i)
    {
        int check_idx = (current_search_start_index + i) % MAX_TASK;
        task_t *cur = tasks[check_idx];

        if (cur == NULL)
        {
            continue;
        }

        if (cur == current)
        {
            // Current task is already locked. Check its status.
            if (current->status == TASK_READY)
            {
                next = current;                          // Candidate: current task itself
                task_index = (check_idx + 1) % MAX_TASK; // Next schedule round starts after this task
                break;
            }
            // If current is not ready, continue search. Its lock (current->lock) is already held.
        }
        else
        {
            // For other tasks, acquire their lock to check status
            kmt->spin_lock(&cur->lock);
            if (cur->status == TASK_READY)
            {
                next = cur;                              // Found a different ready task. Its lock (cur->lock) is now held.
                task_index = (check_idx + 1) % MAX_TASK; // Next schedule round starts after this task
                break;                                   // Exit loop, next->lock (which is cur->lock) is held
            }
            kmt->spin_unlock(&cur->lock); // Release lock if not chosen
        }
    }

    // 找到了下一个可运行的任务
    if (next)
    {
        next->status = TASK_RUNNING;
        set_current_task(next); // set_current_task is assumed to be correct (no internal locks)

        // 如果选中的下一个任务不是原来的当前任务，则释放下一个任务的锁
        if (next != current)
        {
            kmt->spin_unlock(&next->lock);
        }
        // 总是释放原始当前任务的锁。
        kmt->spin_unlock(&current->lock);
        kmt->spin_unlock(&task_lock);
        return next->context;
    }
    // 没有找到其他任务，检查当前任务是否可以继续运行
    if (current->status == TASK_READY)
    {
        current->status = TASK_RUNNING;
        kmt->spin_unlock(&current->lock);
        kmt->spin_unlock(&task_lock);
        return ctx;
    }
    // 原始的 'current' 任务不可运行 (例如，TASK_BLOCKED)。释放它的锁。
    kmt->spin_unlock(&current->lock);
    // 所有任务都不可运行，使用对应CPU的监视任务
    int cpu_id = cpu_current();

    // 需要先获取monitor_task的锁
    kmt->spin_lock(&monitor_task[cpu_id].lock);
    monitor_task[cpu_id].status = TASK_RUNNING;
    // 直接设置，不调用set_current_task
    cpus[cpu_id].current_task = &monitor_task[cpu_id];
    kmt->spin_unlock(&monitor_task[cpu_id].lock);

    kmt->spin_unlock(&task_lock);
    return monitor_task[cpu_id].context;
}

// 初始化KMT模块
static void kmt_init()
{
    kmt->spin_init(&task_lock, "task_lock");
    // 注册中断处理器，用于任务调度
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    // 初始化任务列表
    for (int i = 0; i < MAX_TASK; i++)
    {
        tasks[i] = NULL;
    }

    // 为每个CPU创建一个监视任务（初始进程）
    for (int i = 0; i < MAX_CPU; i++)
    {
        // 初始化监视任务
        monitor_task[i].status = TASK_RUNNING;
        monitor_task[i].cpu = i;
        monitor_task[i].next = NULL;
        // 初始化监视任务的锁
        kmt->spin_init(&monitor_task[i].lock, "monitor_task_lock");
    }
}

// 创建任务
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
{
    if (!task || !name || !entry)
        return -1;

    // 分配栈空间
    task->stack = pmm->alloc(STACK_SIZE);
    if (!task->stack)
        return -1;

    // 设置栈底栅栏，用于检测栈溢出
    task->fence = (void *)FENCE_PATTERN;
    // 初始化任务上下文
    Area stack_area = RANGE(task->stack, task->stack + STACK_SIZE);
    task->context = kcontext(stack_area, entry, arg);
    task->name = name;
    task->status = TASK_READY;
    kmt->spin_init(&task->lock, name);
    kmt->spin_lock(&task_lock);
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] == NULL)
        {
            tasks[i] = task;
            break;
        }
    }
    kmt->spin_unlock(&task_lock);
    return 0; // 成功
}

// 销毁任务
static void kmt_teardown(task_t *task)
{
    if (!task)
        return;

    kmt->spin_lock(&task_lock);
    // 释放栈空间
    if (task->stack)
    {
        pmm->free(task->stack);
    }
    task->status = TASK_DEAD;
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] == task)
        {
            tasks[i] = NULL;
            break;
        }
    }
    kmt->spin_unlock(&task_lock);
}

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
    //  禁用中断并保存中断状态
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

// 初始化信号量
static void kmt_sem_init(sem_t *sem, const char *name, int value)
{
    panic_on(sem == NULL, "Semaphore is NULL");
    sem->value = value;
    sem->name = name;
    sem->wait_list = NULL;
    kmt->spin_init(&sem->lock, name);
}

// 等待信号量
static void kmt_sem_wait(sem_t *sem)
{
    panic_on(sem == NULL, "Semaphore is NULL");
    kmt->spin_lock(&sem->lock); // 获得自旋锁
    sem->value--;
    if (sem->value < 0)
    {
        task_t *current = get_current_task();
        kmt->spin_lock(&current->lock); // 锁定当前任务
        current->status = TASK_BLOCKED; // 将当前任务状态设置为阻塞
        // 将当前任务添加到等待队列
        if (sem->wait_list == NULL)
        {
            sem->wait_list = current;
        }
        else
        {
            task_t *cur = sem->wait_list;
            while (cur->next != NULL)
            {
                cur = cur->next;
            }
            cur->next = current;
            current->next = NULL;
        }
        kmt->spin_unlock(&current->lock); // 解锁当前任务
        kmt->spin_unlock(&sem->lock);
        // 让出CPU，等待信号量
        yield();
        return;
    }
    kmt->spin_unlock(&sem->lock);
}

// 释放信号量
static void kmt_sem_signal(sem_t *sem)
{
    panic_on(sem == NULL, "Semaphore is NULL");
    kmt->spin_lock(&sem->lock);
    sem->value++;
    if (sem->wait_list)
    {
        kmt->spin_lock(&sem->wait_list->lock);   // 锁定等待的任务
        sem->wait_list->status = TASK_READY;     // 将等待的任务状态设置为就绪
        task_t *nxt = sem->wait_list->next;      // 获取下一个等待的任务
        kmt->spin_unlock(&sem->wait_list->lock); // 解锁等待的任务
        sem->wait_list = nxt;
    }
    kmt->spin_unlock(&sem->lock);
}

// 使用MODULE_DEF宏定义kmt模块
MODULE_DEF(kmt) = {
    .init = kmt_init,
    .create = kmt_create,
    .teardown = kmt_teardown,
    .spin_init = kmt_spin_init,
    .spin_lock = kmt_spin_lock,
    .spin_unlock = kmt_spin_unlock,
    .sem_init = kmt_sem_init,
    .sem_wait = kmt_sem_wait,
    .sem_signal = kmt_sem_signal,
};
