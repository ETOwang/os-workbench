#include <common.h>
#include <os.h>
#include <limits.h>
#define MAX_CPU 32
#define MAX_TASK 128
// 任务链表
static spinlock_t task_lock;
static task_t *tasks[MAX_TASK];
static task_t monitor_task[MAX_CPU];
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
    if (cur == NULL)
    {
        cur = &monitor_task[cpu];
    }
    return cur;
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
    if (current->status == TASK_RUNNING)
    {
        current->status = TASK_READY; // 将当前任务状态设置为就绪
    }
    task_t *next = NULL;
    int cur = task_index + 1;
    while (cur != task_index)
    {
        cur = cur % MAX_TASK;
        task_t *task = tasks[cur];
        if (task != NULL && task->status == TASK_READY)
        {
            task_index = cur;
            next = task; // 找到下一个就绪任务
            break;
        }
        cur++;
    }
    if (next)
    {
        next->status = TASK_RUNNING;
        set_current_task(next);
        kmt->spin_unlock(&task_lock);
        return next->context;
    }
    // 没有可运行的任务，保持当前任务运行
    if (current->status == TASK_READY)
    {
        printf("No runnable task, keep current task running\n");
        current->status = TASK_RUNNING;
        kmt->spin_unlock(&task_lock);
        return ctx;
    }
    kmt->spin_unlock(&task_lock);
    return monitor_task[cpu_current()].context; // 返回监视任务的上下文
}

// 初始化KMT模块
static void kmt_init()
{
    kmt->spin_init(&task_lock, "task_lock");
    // 注册中断处理器，用于任务调度
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    for (int i = 0; i < MAX_TASK; i++)
    {
        tasks[i] = NULL; // 初始化任务列表
    }
}

// 创建任务
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
{
    if (!task || !name || !entry)
        return -1;

    kmt->spin_lock(&task_lock);
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
    printf("try_lock: %s\n", lk->name);
    panic_on(!lk, "Spinlock is NULL");
    // printf("kmt_spin_lock: %s\n", lk->name);
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
    printf("finish_lock: %s\n", lk->name);
}

// 释放自旋锁
static void kmt_spin_unlock(spinlock_t *lk)
{
    printf("try_unlock: %s\n", lk->name);
    panic_on(!lk, "Spinlock is NULL");
    if (!holding(lk))
    {
        panic("Spinlock is not held by current CPU");
    }
    lk->cpu = -1;
    // 释放锁
    atomic_xchg(&lk->locked, 0);
     printf("finish_unlock: %s\n", lk->name);
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
        sem->wait_list->status = TASK_READY; // 将等待的任务状态设置为就绪
        sem->wait_list = sem->wait_list->next;
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
