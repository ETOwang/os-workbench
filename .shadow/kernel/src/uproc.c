#include <common.h>
#include <string.h>
#include <am.h>
#include "initcode.inc"
#define PTE_ADDR(pte) ((pte) & 0x000ffffffffff000ULL)
#define MAX_PID 32767
static spinlock_t uproc_lock;
static int next_pid = 1;
extern void kmt_add_task(task_t *task);
extern task_t *kmt_get_son();
static int uproc_alloc_pid()
{
    kmt->spin_lock(&uproc_lock);
    int ret = next_pid;
    next_pid = next_pid % MAX_PID + 1;
    kmt->spin_unlock(&uproc_lock);
    return ret;
}
static void user_init()
{
    task_t *task = pmm->alloc(sizeof(task_t));
    task->pi = pmm->alloc(sizeof(procinfo_t));
    task->pi->parent = NULL;
    task->pi->pid = uproc_alloc_pid();
    task->pi->cwd = pmm->alloc(PATH_MAX);
    strcpy(task->pi->cwd, "/");
    panic_on(task->pi == NULL, "Failed to allocate procinfo for init process");
    protect(&task->pi->as);
    char *mem = pmm->alloc(task->pi->as.pgsize);
    map(&task->pi->as, (void *)(long)UVMEND - task->pi->as.pgsize, (void *)mem, MMAP_READ | MMAP_WRITE);
    panic_on(_init_len > 3*task->pi->as.pgsize, "init code too large");
    char *entry = pmm->alloc(3*task->pi->as.pgsize);
    memcpy(entry, _init, _init_len);
    //TODO:better function
    for (size_t i = 0; i < 3; i++)
    {
        map(&task->pi->as, (void *)UVSTART+i*task->pi->as.pgsize, (void *)entry, MMAP_READ);
    }

    task->fence = (void *)FENCE_PATTERN;
    Area stack_area = RANGE(task->stack, task->stack + STACK_SIZE);
    task->context = ucontext(&task->pi->as, stack_area, (void *)UVSTART);
    task->name = "initproc";
    task->status = TASK_READY;
    task->cpu = -1;
    task->next = NULL;
    kmt->spin_init(&task->lock, task->name);
    kmt_add_task(task);
    TRACE_EXIT;
}
static void uproc_init()
{
    vme_init((void *(*)(int))pmm->alloc, pmm->free);
    kmt->spin_init(&uproc_lock, "uproc_lock");
    user_init();
}

static int uproc_exit(task_t *task, int status)
{
    panic_on(task == NULL, "Task is NULL");
    panic_on(task->pi == NULL, "Task procinfo is NULL");
    task->pi->xstate = status;
    task->status = TASK_ZOMBIE;
    return 0;
}

static int uproc_kputc(task_t *task, char ch)
{
    putch(ch);
    return 0;
}

int uvmcopy(AddrSpace *old, AddrSpace *new, uint64_t sz)
{
    panic_on(old == NULL || new == NULL, "old or new AddrSpace is NULL");
    uintptr_t pg_sz = old->pgsize;
    for (uintptr_t offset = 0; offset < sz; offset += pg_sz)
    {
        uintptr_t current_va = (uintptr_t)UVSTART + offset;
        if (current_va >= (uintptr_t)UVMEND)
        {
            break;
        }
        uintptr_t *ptep = ptewalk(old, current_va);
        if (ptep && (*ptep & PROT_READ))
        {
            int map_prot = MMAP_READ;
            if (*ptep & MMAP_WRITE)
            {
                map_prot |= MMAP_WRITE;
            }
            void *old_pa = (void *)PTE_ADDR(*ptep);
            void *new_pa = pmm->alloc(pg_sz);
            panic_on(new_pa == NULL, "Failed to allocate new physical page");
            memcpy(new_pa, old_pa, pg_sz);
            map(new, (void *)current_va, new_pa, map_prot);
        }
    }
    return 0;
}
static int uproc_fork(task_t *task)
{
    panic_on(task == NULL, "Task is NULL");
    int pid = uproc_alloc_pid();
    task_t *son = pmm->alloc(sizeof(task_t));
    son->name = "son";
    son->cpu = -1;
    son->next = NULL;
    son->fence = task->fence;
    kmt->spin_init(&son->lock, son->name);
    son->status = TASK_READY;
    son->pi = pmm->alloc(sizeof(procinfo_t));
    son->pi->cwd = pmm->alloc(PATH_MAX);
    son->pi->parent = task;
    strcpy(son->pi->cwd, task->pi->cwd);
    protect(&son->pi->as);
    uvmcopy(&task->pi->as, &son->pi->as, UVMEND - UVSTART);
    son->pi->pid = pid;
    son->context = (Context *)(son->stack + STACK_SIZE - sizeof(Context));
    *son->context = *task->context;
    son->context->GPRx = 0;
    son->context->cr3 = son->pi->as.ptr;
    son->context->rsp0 = (uint64_t)son->stack + STACK_SIZE;
    kmt_add_task(son);
    return pid;
}

static int uproc_wait(task_t *task, int pid, int *status, int options)
{
    panic_on(task == NULL, "Task is NULL");
    panic_on(task->pi == NULL, "Task procinfo is NULL");
    bool found = false;
    while (1)
    {
        task_t *son = kmt_get_son();
        if (son != NULL)
        {
            kmt->spin_lock(&son->lock);
            bool match_pid = pid == -1 || son->pi->pid == pid;
            bool is_zombie = (son->status == TASK_ZOMBIE);
            if (match_pid)
            {
                if (is_zombie)
                {
                    if (status != NULL)
                    {
                        *status = son->pi->xstate;
                    }
                    int ret_pid = son->pi->pid;
                    son->pi->parent = NULL;
                    son->status = TASK_DEAD;
                    kmt->spin_unlock(&son->lock);
                    return ret_pid;
                }
                found = true;
            }
            kmt->spin_unlock(&son->lock);
        }
        if (options & WNOHANG)
        {
            if (!found)
                return -1; // 没有子进程
            return 0;
        }
        // TODO:
        // yield();
    }
}

static int uproc_getpid(task_t *task)
{
    panic_on(task->pi == NULL, "Task procinfo is NULL");
    return task->pi->pid;
}
static int uproc_getppid(task_t *task)
{
    panic_on(task->pi == NULL, "Task procinfo is NULL");
    if (task->pi->parent != NULL)
    {
        return task->pi->parent->pi->pid;
    }
    return 0;
}
static int uproc_sleep(task_t *task, int seconds)
{
    if (seconds > 0)
    {
        AM_TIMER_UPTIME_T start_time = io_read(AM_TIMER_UPTIME);
        uint64_t duration_us = (uint64_t)seconds * 1000000;
        uint64_t end_us = start_time.us + duration_us;
        while (io_read(AM_TIMER_UPTIME).us < end_us)
            ;
    }
    return 0;
}

static int64_t uproc_uptime(task_t *task, struct timespec *ts)
{
    if (ts != NULL)
    {
        AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
        ts->tv_sec = uptime.us / 1000000;
        ts->tv_nsec = (uptime.us % 1000000) * 1000;
        return 0;
    }
    return -1;
}
static void *uproc_mmap(task_t *task, void *addr, int length, int prot, int flags)
{
    panic_on(task == NULL, "Task is NULL");
    panic_on(task->pi == NULL, "Task procinfo is NULL");
    if (addr == NULL)
    {
        addr = (void *)UVSTART;
    }
    if (length <= 0)
    {
        return NULL;
    }
    return NULL;
}
// Define the uproc module structure with pointers to implemented functions
MODULE_DEF(uproc) = {
    .init = uproc_init,
    .kputc = uproc_kputc,
    .fork = uproc_fork,
    .wait = uproc_wait,
    .exit = uproc_exit,
    .getpid = uproc_getpid,
    .sleep = uproc_sleep,
    .uptime = uproc_uptime,
    .getppid = uproc_getppid,
    .mmap = uproc_mmap};
