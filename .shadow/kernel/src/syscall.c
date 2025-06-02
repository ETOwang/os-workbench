#include <common.h>
#include <syscall.h>
#include <vfs.h>

static int parse_path(char *buf, task_t *task, int dirfd, const char *path)
{
    if(strlen(path)>PATH_MAX){
        return -1;
    }
    if (path[0] == '/')
    {
        strcpy(buf,path);
    }
    else if (dirfd == AT_FDCWD)
    {
        if (strlen(task->pi->cwd) + strlen(path) + 2 > PATH_MAX)
        {
            return -1;
        }
        strcpy(buf, task->pi->cwd);
        if (buf[strlen(buf) - 1] != '/')
        {
            strcat(buf, "/");
        }
        strcat(buf, path);
    }
    else
    {
        const char *ph = vfs->getdirpath(dirfd);
        if (!ph)
        {
            return -1;
        }
        if (strlen(ph) + strlen(path) + 2 > PATH_MAX)
        {
            return -1;
        }
        strcpy(buf, ph);
        if (buf[strlen(buf) - 1] != '/')
        {
            strcat(buf, "/");
        }
        strcat(buf, path);
    }
    return 0;
}
// 文件系统相关系统调用
static uint64_t syscall_chdir(task_t *task, const char *path)
{
    if (path == NULL || strlen(path) >= PATH_MAX)
    {
        return -1;
    }
    int ret = vfs->opendir(path);
    if (ret < 0)
    {
        return -1;
    }
    vfs->closedir(ret);
    strncpy(task->pi->cwd, path, strlen(path) + 1);
    return 0;
}

static uint64_t syscall_getcwd(task_t *task, char *buf, size_t size)
{
    if (size > PATH_MAX)
    {
        return (uint64_t)NULL;
    }
    if (!buf)
    {
        buf = (char *)pmm->alloc(PATH_MAX);
        size = PATH_MAX;
    }
    if (strlen(task->pi->cwd) >= size)
    {
        return (uint64_t)NULL;
    }
    strncpy(buf, task->pi->cwd, strlen(task->pi->cwd) + 1);
    return (uint64_t)buf;
}

static uint64_t syscall_openat(task_t *task, int fd, const char *filename, int flags, mode_t mode)
{
    if (filename == NULL)
    {
        return -1;
    }
    char full_path[PATH_MAX];
    if(parse_path(full_path, task, fd, filename) < 0){
          return -1;
    }
    // TODO:use mode
    int vfd = vfs->open(full_path, flags);
    if (vfd < 0)
    {
        return -1;
    }
    return vfd;
}

static uint64_t syscall_pipe2(task_t *task, int pipefd[2], int flags)
{
    // 简化实现 - 创建两个文件描述符
    if (pipefd == NULL)
    {
        return -1;
    }

    // 这里应该创建真正的管道，暂时返回错误
    return -1;
}

static uint64_t syscall_dup(task_t *task, int oldfd)
{
    return vfs->dup(oldfd);
}

static uint64_t syscall_dup3(task_t *task, int oldfd, int newfd, int flags)
{
    return vfs->dup3(oldfd, newfd, flags);
}

static uint64_t syscall_getdents64(task_t *task, int fd, struct dirent *buf, size_t len)
{
    if (buf == NULL || len == 0)
    {
        return -1;
    }
    //TODO:check len
    return vfs->readdir(fd, buf);
}

static uint64_t syscall_linkat(task_t *task, int olddirfd, const char *oldpath,
                               int newdirfd, const char *newpath, int flags)
{
    if (oldpath == NULL || newpath == NULL)
    {
        return -1;
    }
    char old_full_path[PATH_MAX];
    if(parse_path(old_full_path, task, olddirfd, oldpath) < 0){
        return -1;
    }
    char new_full_path[PATH_MAX];
    if(parse_path(new_full_path, task, newdirfd, newpath) < 0){
        return -1;
    }
    //TODO:use flags
    return vfs->link(old_full_path, new_full_path);
}

static uint64_t syscall_unlinkat(task_t *task, int dirfd, const char *path, int flags)
{
    if (path == NULL)
    {
        return -1;
    }
    char full_path[PATH_MAX];
    if (parse_path(full_path, task, dirfd, path) < 0)
    {
        return -1;
    }
    return vfs->unlink(full_path);
}

static uint64_t syscall_mkdirat(task_t *task, int dirfd, const char *path, mode_t mode)
{
    if (path == NULL)
    {
        return -1;
    }
    char full_path[PATH_MAX];
    if (parse_path(full_path, task, dirfd, path) < 0)
    {
        return -1;
    }
    //TODO:use mode
    return vfs->mkdir(full_path);
}

static uint64_t syscall_umount2(task_t *task, const char *target, int flags)
{
    if (target == NULL)
    {
        return -1;
    }

    return vfs->umount(target);
}

static uint64_t syscall_mount(task_t *task, const char *source, const char *target,
                              const char *filesystemtype, unsigned long mountflags,
                              const void *data)
{
    if (source == NULL || target == NULL || filesystemtype == NULL)
    {
        return -1;
    }

    return vfs->mount(source, target, filesystemtype, mountflags, (void *)data);
}

static uint64_t syscall_fstat(task_t *task, int fd, struct kstat *statbuf)
{
    if (statbuf == NULL)
    {
        return -1;
    }
    // 简化实现 - 需要从文件描述符获取文件信息
    // 这里暂时填充一些默认值
    statbuf->st_dev = 0;
    statbuf->st_ino = fd;
    statbuf->st_mode = 0644;
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0;
    statbuf->st_size = 0;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = 0;
    statbuf->st_atime_sec = 0;
    statbuf->st_atime_nsec = 0;
    statbuf->st_mtime_sec = 0;
    statbuf->st_mtime_nsec = 0;
    statbuf->st_ctime_sec = 0;
    statbuf->st_ctime_nsec = 0;
    return 0;
}

// 内存管理相关系统调用
static uint64_t syscall_brk(task_t *task, void *addr)
{
    // 获取当前堆结束地址
    void *current_brk = task->pi->as.area.end;

    // 如果addr为NULL，返回当前brk值
    if (addr == NULL)
    {
        return (uint64_t)current_brk;
    }

    // 检查新地址是否合理
    if (addr < task->pi->as.area.start || addr > (void *)UVMEND)
    {
        return (uint64_t)current_brk; // 失败时返回当前brk
    }

    // 简化实现：直接更新堆结束地址
    // 实际实现应该分配/释放内存页
    task->pi->as.area.end = addr;

    return (uint64_t)addr;
}

static uint64_t syscall_munmap(task_t *task, void *addr, size_t length)
{
    if (addr == NULL || length == 0)
    {
        return -1;
    }

    // 简化实现 - 实际需要取消内存映射
    return 0;
}

static uint64_t syscall_mmap(task_t *task, void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (length == 0)
    {
        return (uint64_t)-1;
    }

    // 调用uproc的mmap
    void *result = uproc->mmap(task, addr, length, prot, flags);
    return (uint64_t)result;
}

// 其他系统调用
static uint64_t syscall_times(task_t *task, struct tms *buf)
{
    if (buf == NULL)
    {
        return -1;
    }

    // 获取当前时间
    AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
    clock_t ticks = uptime.us / 10000; // 转换为10ms滴答数

    // 简化实现 - 填充时间信息
    buf->tms_utime = ticks / 2; // 用户时间
    buf->tms_stime = ticks / 2; // 系统时间
    buf->tms_cutime = 0;        // 子进程用户时间
    buf->tms_cstime = 0;        // 子进程系统时间

    return ticks;
}

static uint64_t syscall_uname(task_t *task, struct utsname *buf)
{
    if (buf == NULL)
    {
        return -1;
    }

    // 填充系统信息
    strcpy(buf->sysname, "Linux");
    strcpy(buf->nodename, "localhost");
    strcpy(buf->release, "5.10.0");
    strcpy(buf->version, "#1 SMP");
    strcpy(buf->machine, "x86_64");
    // domainname字段可能不存在于标准utsname中

    return 0;
}

static uint64_t syscall_sched_yield(task_t *task)
{
    //TODO:do something
    return 0;
}

static uint64_t syscall_nanosleep(task_t *task, const struct timespec *req, struct timespec *rem)
{
    if (req == NULL)
    {
        return -1;
    }
    //TODO:real implementation
    return uproc->sleep(task, req->tv_sec);
}

static uint64_t syscall_gettimeofday(task_t *task, struct timespec *ts, void *tz)
{
    if (ts == NULL)
    {
        return -1;
    }

    return uproc->uptime(task, ts);
}

// 进程管理相关系统调用
static uint64_t syscall_clone(task_t *task, int flags, void *stack, int *ptid, int *ctid, unsigned long newtls)
{
    // 简化实现 - 调用uproc的fork
    return uproc->fork(task);
}

static uint64_t syscall_execve(task_t *task, const char *pathname, char *const argv[], char *const envp[])
{
    if (pathname == NULL)
    {
        return -1;
    }

    // 简化实现 - 这里应该加载新的程序
    // 暂时返回错误
    return -1;
}

MODULE_DEF(syscall) = {
    .chdir = syscall_chdir,
    .getcwd = syscall_getcwd,
    .openat = syscall_openat,
    .pipe2 = syscall_pipe2,
    .dup = syscall_dup,
    .dup3 = syscall_dup3,
    .getdents64 = syscall_getdents64,
    .linkat = syscall_linkat,
    .unlinkat = syscall_unlinkat,
    .mkdirat = syscall_mkdirat,
    .umount2 = syscall_umount2,
    .mount = syscall_mount,
    .fstat = syscall_fstat,
    .brk = syscall_brk,
    .munmap = syscall_munmap,
    .mmap = syscall_mmap,
    .times = syscall_times,
    .uname = syscall_uname,
    .sched_yield = syscall_sched_yield,
    .nanosleep = syscall_nanosleep,
    .gettimeofday = syscall_gettimeofday,
    .clone = syscall_clone,
    .execve = syscall_execve,
};