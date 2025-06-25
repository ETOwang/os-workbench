#include <common.h>
#include <limits.h>
#include <syscall.h>
static spinlock_t task_lock;
static task_t *tasks[MAX_TASK];
static int task_index;
static struct cpu
{
    int noff;
    int intena;
    task_t *current_task;
    task_t monitor_task;
} cpus[MAX_CPU];

static task_t *get_current_task()
{
    TRACE_ENTRY;
    int cpu = cpu_current();
    task_t *cur = cpus[cpu].current_task;
    panic_on(cur == NULL, "Current task is NULL");
    TRACE_EXIT;
    return cur;
}

static void set_current_task(task_t *task)
{
    TRACE_ENTRY;
    panic_on(task == NULL, "Task is NULL");
    int cpu = cpu_current();
    cpus[cpu].current_task = task;
    task->cpu = cpu;
    TRACE_EXIT;
}
/*
 to solve the data race
*/
static Context *kmt_mark_as_free(Event ev, Context *ctx)
{
    TRACE_ENTRY;
    kmt->spin_lock(&task_lock);
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] != NULL)
        {
            kmt->spin_lock(&tasks[i]->lock);
            if (tasks[i]->cpu == cpu_current() && tasks[i] != get_current_task())
            {
                tasks[i]->cpu = -1;
            }
            else if (tasks[i]->cpu == -1 && tasks[i]->status == TASK_DEAD)
            {
                if (tasks[i]->pi)
                {
                    unprotect(&tasks[i]->pi->as);
                    pmm->free(tasks[i]->pi);
                    tasks[i]->pi = NULL;
                }
            }
            kmt->spin_unlock(&tasks[i]->lock);
        }
    }
    kmt->spin_unlock(&task_lock);
    TRACE_EXIT;
    return NULL;
}
static Context *kmt_context_save(Event ev, Context *ctx)
{
    TRACE_ENTRY;
    task_t *current = get_current_task();
    current->context = ctx;
    TRACE_EXIT;
    return NULL;
}
#include "syscall.inc"
static Context *kmt_syscall(Event ev, Context *ctx)
{
    SyscallHandler handler = syscall_table[ctx->GPRx];
    if (handler)
    {
        ctx->GPRx = handler(ctx);
    }
    else
    {
        panic("Unknown syscall number");
    }
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx)
{
    TRACE_ENTRY;
    kmt->spin_lock(&task_lock);
    task_t *current = get_current_task();
    kmt->spin_lock(&current->lock);
    if (current->status == TASK_RUNNING)
    {
        current->status = TASK_READY;
    }
    panic_on(current->status == TASK_RUNNING, "Current task is running");
    task_t *next = NULL;
    int current_search_start_index = task_index;

    for (int i = 0; i < MAX_TASK; ++i)
    {
        int check_idx = (current_search_start_index + i) % MAX_TASK;
        task_t *cur = tasks[check_idx];

        if (cur == NULL || (cur->cpu != -1 && cur->cpu != current->cpu))
        {
            continue;
        }
        panic_on((uintptr_t)cur->fence != FENCE_PATTERN, "Stack overflow detected");
        if (cur == current)
        {
            if (current->status == TASK_READY)
            {
                next = current;
                task_index = (check_idx + 1) % MAX_TASK;
                break;
            }
        }
        else
        {
            kmt->spin_lock(&cur->lock);
            if (cur->status == TASK_READY)
            {
                next = cur;
                task_index = (check_idx + 1) % MAX_TASK;
                break;
            }
            kmt->spin_unlock(&cur->lock);
        }
    }

    if (next)
    {
        next->status = TASK_RUNNING;
        set_current_task(next);
        if (next != current)
        {
            kmt->spin_unlock(&next->lock);
        }
        kmt->spin_unlock(&current->lock);
        kmt->spin_unlock(&task_lock);
        TRACE_EXIT;
        return next->context;
    }
    kmt->spin_unlock(&current->lock);
    kmt->spin_unlock(&task_lock);
    int cpu_id = cpu_current();
    cpus[cpu_id].monitor_task.status = TASK_RUNNING;
    set_current_task(&cpus[cpu_id].monitor_task);
    TRACE_EXIT;
    return cpus[cpu_id].monitor_task.context;
}
task_t *kmt_get_son()
{
    task_t *cur = get_current_task();
    task_t *son = NULL;
    kmt->spin_lock(&task_lock);
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] == NULL)
        {
            continue;
        }
        kmt->spin_lock(&tasks[i]->lock);
        if (tasks[i]->pi != NULL && tasks[i]->pi->parent == cur && tasks[i]->status != TASK_DEAD)
        {
            son = tasks[i];
            kmt->spin_unlock(&tasks[i]->lock);
            break;
        }
        kmt->spin_unlock(&tasks[i]->lock);
    }
    kmt->spin_unlock(&task_lock);
    return son;
}
void kmt_add_task(task_t *task)
{
    panic_on(task == NULL, "task is NULL");
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
}
static Context *kmt_pgfault(Event ev, Context *ctx)
{
    printf("rsp:%p\n", ctx->rsp);
    printf("rsp0:%p\n", ctx->rsp0);
    printf("Page fault at %p\n", ev.ref);
    printf("Page fault msg: %s\n", ev.msg);
    printf("Page fault cause: %p\n", ev.cause);
    halt(1);
    return NULL;
}
static void kmt_init()
{
    TRACE_ENTRY;
    kmt->spin_init(&task_lock, "task_lock");
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MIN + 1, EVENT_NULL, kmt_mark_as_free);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    os->on_irq(1, EVENT_SYSCALL, kmt_syscall);
    os->on_irq(1, EVENT_PAGEFAULT, kmt_pgfault);
    for (int i = 0; i < MAX_TASK; i++)
    {
        tasks[i] = NULL;
    }
    for (int i = 0; i < MAX_CPU; i++)
    {
        cpus[i].monitor_task.name = "monitor_task";
        cpus[i].monitor_task.status = TASK_READY;
        cpus[i].current_task = &cpus[i].monitor_task;
    }
    TRACE_EXIT;
}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
{
    TRACE_ENTRY;
    if (!task || !name)
        return -1;
    task->pi = NULL;
    task->fence = (void *)FENCE_PATTERN;
    Area stack_area = RANGE(task->stack, task->stack + STACK_SIZE);
    task->context = kcontext(stack_area, entry, arg);
    task->name = name;
    task->status = TASK_READY;
    task->cpu = -1;
    task->next = NULL;
    kmt->spin_init(&task->lock, name);
    kmt_add_task(task);
    TRACE_EXIT;
    return 0;
}

static void kmt_teardown(task_t *task)
{
    if (!task)
        return;
    kmt->spin_lock(&task->lock);
    task->status = TASK_DEAD;
    kmt->spin_lock(&task_lock);
    for (int i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] == NULL || tasks[i] == task)
        {
            continue;
        }
        kmt->spin_lock(&tasks[i]->lock);
        if (tasks[i]->pi != NULL && tasks[i]->pi->parent == task)
        {
            tasks[i]->pi->parent = NULL;
        }
        kmt->spin_unlock(&tasks[i]->lock);
    }
    kmt->spin_unlock(&task_lock);
    kmt->spin_unlock(&task->lock);
}

static void kmt_spin_init(spinlock_t *lk, const char *name)
{
    TRACE_ENTRY;
    panic_on(lk == NULL, "Spinlock is NULL");
    lk->locked = 0;
    lk->name = name;
    lk->cpu = -1;
    TRACE_EXIT;
}
static bool holding(spinlock_t *lk)
{
    TRACE_ENTRY;
    panic_on(!lk, "Spinlock is NULL");
    TRACE_EXIT;
    // if(!lk->locked){
    //     panic("lk is unlocked");
    // }
    // if(lk->cpu!=cpu_current()){
    //    printf("cpu:%d, lk->cpu:%d\n",cpu_current(),lk->cpu);
    //    panic("cpu is not the same");
    // }
    return lk->locked && lk->cpu == cpu_current();
}
static void push_off()
{
    TRACE_ENTRY;
    int old = ienabled();
    iset(false);
    /**
     * first iset(false) to disable interrupt
     * then get cpu id
     */
    int cpu = cpu_current();
    if (cpus[cpu].noff == 0)
    {
        cpus[cpu].intena = old;
    }
    cpus[cpu].noff++;
    TRACE_EXIT;
}

static void pop_off()
{
    TRACE_ENTRY;
    int cpu = cpu_current();
    panic_on(cpus[cpu].noff == 0, "pop_off: no push_off");
    cpus[cpu].noff--;
    if (cpus[cpu].noff == 0)
    {
        iset(cpus[cpu].intena);
    }
    TRACE_EXIT;
}
static void kmt_spin_lock(spinlock_t *lk)
{
    TRACE_ENTRY;
    panic_on(!lk, "Spinlock is NULL");
    push_off();
    panic_on(holding(lk), lk->name);
    while (atomic_xchg(&lk->locked, 1))
        ;
    __sync_synchronize();
    lk->cpu = cpu_current();
    TRACE_EXIT;
}

static void kmt_spin_unlock(spinlock_t *lk)
{
    TRACE_ENTRY;
    panic_on(!lk, "Spinlock is NULL");
    panic_on(!holding(lk), lk->name);
    lk->cpu = -1;
    __sync_synchronize();
    atomic_xchg(&lk->locked, 0);
    pop_off();
    TRACE_EXIT;
}

static void kmt_sem_init(sem_t *sem, const char *name, int value)
{
    TRACE_ENTRY;
    panic_on(sem == NULL, "Semaphore is NULL");
    sem->value = value;
    sem->name = name;
    sem->wait_list = NULL;
    kmt->spin_init(&sem->lock, name);
    TRACE_EXIT;
}

static void kmt_sem_wait(sem_t *sem)
{
    TRACE_ENTRY;
    panic_on(sem == NULL, "Semaphore is NULL");
    kmt->spin_lock(&sem->lock);
    sem->value--;
    if (sem->value < 0)
    {
        task_t *current = get_current_task();
        panic_on(current == &cpus[cpu_current()].monitor_task, "Current task is monitor task");
        panic_on(current->status != TASK_RUNNING, "Current task is not running");
        kmt->spin_lock(&current->lock);
        current->status = TASK_BLOCKED;
        if (sem->wait_list == NULL)
        {
            sem->wait_list = current;
        }
        else
        {
            task_t *cur = sem->wait_list;
            panic_on(cur->status != TASK_BLOCKED, "Wait list task is not blocked");
            while (cur->next != NULL)
            {
                cur = cur->next;
            }
            panic_on(cur->status != TASK_BLOCKED, "Wait list task is not blocked");
            cur->next = current;
            current->next = NULL;
        }
        kmt->spin_unlock(&current->lock);
        kmt->spin_unlock(&sem->lock);
        yield();
        TRACE_EXIT;
        return;
    }
    kmt->spin_unlock(&sem->lock);
    TRACE_EXIT;
}

static void kmt_sem_signal(sem_t *sem)
{
    TRACE_ENTRY;
    panic_on(sem == NULL, "Semaphore is NULL");
    kmt->spin_lock(&sem->lock);
    sem->value++;
    if (sem->wait_list)
    {
        task_t *task_to_wake = sem->wait_list;
        kmt->spin_lock(&task_to_wake->lock);
        if (task_to_wake->status != TASK_DEAD)
        {
            panic_on(task_to_wake->status != TASK_BLOCKED, "Task status is wrong");
            task_to_wake->status = TASK_READY;
        }
        sem->wait_list = task_to_wake->next;
        task_to_wake->next = NULL;
        kmt->spin_unlock(&task_to_wake->lock);
    }
    kmt->spin_unlock(&sem->lock);
    TRACE_EXIT;
}

static void kmt_wakeup(void *chan)
{
    kmt->spin_lock(&task_lock);
    task_t *task = get_current_task();
    for (size_t i = 0; i < MAX_TASK; i++)
    {
        if (tasks[i] == NULL)
        {
            continue;
        }
        kmt->spin_lock(&tasks[i]->lock);
        if (tasks[i] != task && tasks[i]->chan == chan && tasks[i]->status == TASK_BLOCKED)
        {
            tasks[i]->status = TASK_RUNNING;
        }
        kmt->spin_unlock(&tasks[i]->lock);
    }
}
static void kmt_sleep(void *chan, spinlock_t *lk)
{
    // Atomically release lock and sleep on chan.
    // Reacquires lock when awakened.

    task_t *p = get_current_task();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    kmt->spin_unlock(lk);
    // Go to sleep.
    kmt->spin_lock(&p->lock);
    p->chan = chan;
    p->status = TASK_BLOCKED;
    kmt->spin_unlock(&p->lock);
    while (p->status == TASK_BLOCKED)
    {
        kmt->spin_lock(&p->lock);
        kmt->spin_unlock(&p->lock);
    }

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.

    kmt->spin_lock(lk);
}
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
    .wakeup = kmt_wakeup,
    .sleep = kmt_sleep,
};
