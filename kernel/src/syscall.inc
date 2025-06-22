typedef uint64_t (*SyscallHandler)(Context *ctx);
// 系统调用处理函数实现
static uint64_t handle_kputc(Context *ctx)
{
    return uproc->kputc(get_current_task(), ctx->GPR1);
}

static uint64_t handle_exit(Context *ctx)
{
    return uproc->exit(get_current_task(), ctx->GPR1);
}

static uint64_t handle_gettimeofday(Context *ctx)
{
    return uproc->uptime(get_current_task(), (struct timespec *)ctx->GPR1);
}

static uint64_t handle_sleep(Context *ctx)
{
    return uproc->sleep(get_current_task(), ctx->GPR1);
}

static uint64_t handle_fork(Context *ctx)
{
    return uproc->fork(get_current_task());
}

static uint64_t handle_wait4(Context *ctx)
{
    return uproc->wait(get_current_task(), ctx->GPR1, (int *)ctx->GPR2, ctx->GPR3);
}
static uint64_t handle_getpid(Context *ctx)
{
    return uproc->getpid(get_current_task());
}
static uint64_t handle_getppid(Context *ctx)
{
    return uproc->getppid(get_current_task());
}
static uint64_t handle_getcwd(Context *ctx)
{

    return syscall->getcwd(get_current_task(), (char *)ctx->GPR1, ctx->GPR2);
}

static uint64_t handle_chdir(Context *ctx)
{
    return syscall->chdir(get_current_task(), (const char *)ctx->GPR1);
}
static uint64_t handle_close(Context *ctx)
{
    return syscall->close(get_current_task(), ctx->GPR1);
}
static uint64_t handle_read(Context *ctx)
{
    return syscall->read(get_current_task(), ctx->GPR1, (void *)ctx->GPR2, ctx->GPR3);
}
static uint64_t handle_write(Context *ctx)
{
    return syscall->write(get_current_task(), ctx->GPR1, (const void *)ctx->GPR2, ctx->GPR3);
}

static uint64_t handle_openat(Context *ctx)
{
    return syscall->openat(get_current_task(), ctx->GPR1, (const char *)ctx->GPR2, ctx->GPR3, ctx->GPR4);
}

static uint64_t handle_pipe2(Context *ctx)
{
    return syscall->pipe2(get_current_task(), (int *)ctx->GPR1, ctx->GPR2);
}

static uint64_t handle_dup(Context *ctx)
{
    return syscall->dup(get_current_task(), ctx->GPR1);
}

static uint64_t handle_dup3(Context *ctx)
{
    return syscall->dup3(get_current_task(), ctx->GPR1, ctx->GPR2, ctx->GPR3);
}

static uint64_t handle_getdents64(Context *ctx)
{
    return syscall->getdents64(get_current_task(), ctx->GPR1, (struct dirent *)ctx->GPR2, ctx->GPR3);
}

static uint64_t handle_linkat(Context *ctx)
{
    return syscall->linkat(get_current_task(), ctx->GPR1, (const char *)ctx->GPR2, ctx->GPR3, (const char *)ctx->GPR4, ctx->GPR5);
}

static uint64_t handle_unlinkat(Context *ctx)
{
    return syscall->unlinkat(get_current_task(), ctx->GPR1, (const char *)ctx->GPR2, ctx->GPR3);
}

static uint64_t handle_mkdirat(Context *ctx)
{
    return syscall->mkdirat(get_current_task(), ctx->GPR1, (const char *)ctx->GPR2, ctx->GPR3);
}

static uint64_t handle_umount2(Context *ctx)
{
    return syscall->umount2(get_current_task(), (const char *)ctx->GPR1, ctx->GPR2);
}

static uint64_t handle_mount(Context *ctx)
{
    return syscall->mount(get_current_task(), (const char *)ctx->GPR1, (const char *)ctx->GPR2, (const char *)ctx->GPR3, ctx->GPR4, (const void *)ctx->GPR5);
}

static uint64_t handle_fstat(Context *ctx)
{
    return syscall->fstat(get_current_task(), ctx->GPR1, (struct stat *)ctx->GPR2);
}

static uint64_t handle_sbrk(Context *ctx)
{
    return syscall->sbrk(get_current_task(), (intptr_t)ctx->GPR1);
}

static uint64_t handle_munmap(Context *ctx)
{
    return syscall->munmap(get_current_task(), (void *)ctx->GPR1, ctx->GPR2);
}

static uint64_t handle_mmap(Context *ctx)
{
    return syscall->mmap(get_current_task(), (void *)ctx->GPR1, ctx->GPR2, ctx->GPR3, ctx->GPR4, ctx->GPR5, 0);
}

static uint64_t handle_times(Context *ctx)
{
    return syscall->times(get_current_task(), (struct tms *)ctx->GPR1);
}

static uint64_t handle_uname(Context *ctx)
{
    return syscall->uname(get_current_task(), (struct utsname *)ctx->GPR1);
}

static uint64_t handle_sched_yield(Context *ctx)
{
    return syscall->sched_yield(get_current_task());
}

static uint64_t handle_nanosleep(Context *ctx)
{
    return syscall->nanosleep(get_current_task(), (const struct timespec *)ctx->GPR1, (struct timespec *)ctx->GPR2);
}

static uint64_t handle_clone(Context *ctx)
{
    return syscall->clone(get_current_task(), ctx->GPR1, (void *)ctx->GPR2, (int *)ctx->GPR3, (int *)ctx->GPR4, ctx->GPR5);
}

static uint64_t handle_execve(Context *ctx)
{
    return syscall->execve(get_current_task(), (const char *)ctx->GPR1, (char *const *)ctx->GPR2, (char *const *)ctx->GPR3);
}

static SyscallHandler syscall_table[] = {
    [SYS_kputc] = handle_kputc,
    [SYS_exit] = handle_exit,
    [SYS_gettimeofday] = handle_gettimeofday,
    [SYS_sleep] = handle_sleep,
    [SYS_fork] = handle_fork,
    [SYS_wait4] = handle_wait4,
    [SYS_getpid] = handle_getpid,
    [SYS_getppid] = handle_getppid,
    [SYS_getcwd] = handle_getcwd,
    [SYS_chdir] = handle_chdir,
    [SYS_close] = handle_close,
    [SYS_read] = handle_read,
    [SYS_write] = handle_write,
    [SYS_openat] = handle_openat,
    [SYS_pipe2] = handle_pipe2,
    [SYS_dup] = handle_dup,
    [SYS_dup3] = handle_dup3,
    [SYS_getdents64] = handle_getdents64,
    [SYS_linkat] = handle_linkat,
    [SYS_unlinkat] = handle_unlinkat,
    [SYS_mkdirat] = handle_mkdirat,
    [SYS_umount2] = handle_umount2,
    [SYS_mount] = handle_mount,
    [SYS_fstat] = handle_fstat,
    [SYS_sbrk] = handle_sbrk,
    [SYS_munmap] = handle_munmap,
    [SYS_mmap] = handle_mmap,
    [SYS_times] = handle_times,
    [SYS_uname] = handle_uname,
    [SYS_sched_yield] = handle_sched_yield,
    [SYS_nanosleep] = handle_nanosleep,
    [SYS_clone] = handle_clone,
    [SYS_execve] = handle_execve,
};
