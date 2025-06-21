#include <common.h>
#include <syscall.h>
#include <vfs.h>
#include <elf.h>

// ELF加载器函数声明
static int load_elf(task_t *task, const char *elf_data, size_t file_size, void **entry_point);
static int load_elf_segment(task_t *task, const char *elf_data, size_t file_size, Elf64_Phdr *phdr);
static int copy_segment_data(const char *elf_data, size_t file_size, Elf64_Phdr *phdr,
                             void *page, uintptr_t page_vaddr, uintptr_t vaddr_start,
                             uintptr_t vaddr_end, size_t pgsize);

static int parse_path(char *buf, task_t *task, int dirfd, const char *path)
{
    if (strlen(path) > PATH_MAX)
    {
        return -1;
    }
    if (path[0] == '/')
    {
        strcpy(buf, path);
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
    char full_path[PATH_MAX];
    if (parse_path(full_path, task, AT_FDCWD, path) < 0)
    {
        return -1;
    }
    int ret = vfs->opendir(full_path);
    if (ret < 0)
    {
        return -1;
    }
    vfs->closedir(ret);
    strncpy(task->pi->cwd, full_path, strlen(full_path) + 1);
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
    if (parse_path(full_path, task, fd, filename) < 0)
    {
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
    // TODO:check len
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
    if (parse_path(old_full_path, task, olddirfd, oldpath) < 0)
    {
        return -1;
    }
    char new_full_path[PATH_MAX];
    if (parse_path(new_full_path, task, newdirfd, newpath) < 0)
    {
        return -1;
    }
    // TODO:use flags
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
    // TODO:use mode
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
    return vfs->stat(fd, statbuf);
}

// 内存管理相关系统调用
static uint64_t syscall_sbrk(task_t *task, intptr_t increment)
{
    panic_on(increment < 0, "unimplemented sbrk with negative increment");
    panic_on(increment % task->pi->as.pgsize != 0, "Increment must be a multiple of page size");
    void *current_brk = task->pi->brk;
    panic_on((uintptr_t)current_brk % task->pi->as.pgsize != 0, "Current brk must be a multiple of page size");
    if (increment == 0)
    {
        return (uint64_t)current_brk;
    }
    for (size_t i = 0; i < increment / task->pi->as.pgsize; i++)
    {
        void *mem = pmm->alloc(task->pi->as.pgsize);
        map(&task->pi->as, current_brk + i * task->pi->as.pgsize, mem, MMAP_READ | MMAP_WRITE);
    }
    task->pi->brk = (void *)((uintptr_t)current_brk + increment);
    return (uint64_t)current_brk;
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
    // TODO:real implementation
    AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
    clock_t ticks = uptime.us / 10000;
    buf->tms_utime = ticks / 2;
    buf->tms_stime = ticks / 2;
    buf->tms_cutime = 0;
    buf->tms_cstime = 0;

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
    // TODO:do something
    return 0;
}

static uint64_t syscall_nanosleep(task_t *task, const struct timespec *req, struct timespec *rem)
{
    if (req == NULL)
    {
        return -1;
    }
    // TODO:real implementation
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
    int result = uproc->fork(task);
    return result;
}

static uint64_t syscall_execve(task_t *task, const char *pathname, char *const argv[], char *const envp[])
{
    if (pathname == NULL)
    {
        return -1;
    }
    int fd = vfs->open(pathname, 0);
    if (fd < 0)
    {
        return -1;
    }
    struct kstat stat;
    if (syscall_fstat(task, fd, &stat) < 0)
    {
        vfs->close(fd);
        return -1;
    }
    size_t file_size = stat.st_size;
    if (file_size == 0 || file_size < sizeof(Elf64_Ehdr))
    {
        vfs->close(fd);
        return -1;
    }

    char *elf_data = pmm->alloc(file_size);
    if (elf_data == NULL)
    {
        vfs->close(fd);
        return -1;
    }

    ssize_t bytes_read = vfs->read(fd, elf_data, file_size);
    vfs->close(fd);
    if (bytes_read != file_size)
    {
        pmm->free(elf_data);
        return -1;
    }

    void *entry_point;
    if (load_elf(task, elf_data, file_size, &entry_point) < 0)
    {
        pmm->free(elf_data);
        return -1;
    }
    pmm->free(elf_data);
    int argc = 0;
    int envc = 0;
    size_t args_size = 0;
    size_t envs_size = 0;
    if (argv != NULL)
    {
        while (argv[argc] != NULL)
        {
            args_size += strlen(argv[argc]) + 1;
            argc++;
        }
    }
    if (envp != NULL)
    {
        while (envp[envc] != NULL)
        {
            envs_size += strlen(envp[envc]) + 1;
            envc++;
        }
    }

    size_t stack_needed = (argc + 1) * sizeof(char *) +
                          (envc + 1) * sizeof(char *) +
                          args_size + envs_size +
                          16;

    size_t pages_needed = 1;
    size_t total_stack_needed = stack_needed + 4096;
    if (total_stack_needed > pages_needed * task->pi->as.pgsize)
    {
        pages_needed = (total_stack_needed + task->pi->as.pgsize - 1) / task->pi->as.pgsize;
    }

    for (size_t i = 0; i < pages_needed; i++)
    {
        char *mem = pmm->alloc(task->pi->as.pgsize);
        if (mem == NULL)
        {
            return -1;
        }
        uintptr_t stack_addr = UVMEND - (i + 1) * task->pi->as.pgsize;
        map(&task->pi->as, (void *)stack_addr, (void *)mem, MMAP_READ | MMAP_WRITE);
    }

    task->context = ucontext(&task->pi->as, RANGE(task->stack, task->stack + STACK_SIZE), entry_point);

    char *stack_top = (char *)task->context->rsp;
    char *stack_ptr = stack_top;

    char **argv_ptrs = pmm->alloc((argc + 1) * sizeof(char *));
    char **envp_ptrs = pmm->alloc((envc + 1) * sizeof(char *));

    for (int i = envc - 1; i >= 0; i--)
    {
        size_t len = strlen(envp[i]) + 1;
        stack_ptr -= len;
        memcpy(stack_ptr, envp[i], len);
        envp_ptrs[i] = stack_ptr;
    }

    for (int i = argc - 1; i >= 0; i--)
    {
        size_t len = strlen(argv[i]) + 1;
        stack_ptr -= len;
        memcpy(stack_ptr, argv[i], len);
        argv_ptrs[i] = stack_ptr;
    }

    stack_ptr = (char *)((uintptr_t)stack_ptr & ~7);

    stack_ptr -= (envc + 1) * sizeof(char *);
    char **envp_array = (char **)stack_ptr;
    for (int i = 0; i < envc; i++)
    {
        envp_array[i] = envp_ptrs[i];
    }
    envp_array[envc] = NULL;

    stack_ptr -= (argc + 1) * sizeof(char *);
    char **argv_array = (char **)stack_ptr;
    for (int i = 0; i < argc; i++)
    {
        argv_array[i] = argv_ptrs[i];
    }
    argv_array[argc] = NULL;
    stack_ptr -= sizeof(int);
    *((int *)stack_ptr) = argc;
    pmm->free(argv_ptrs);
    pmm->free(envp_ptrs);
    task->context->rsp = (uintptr_t)stack_ptr;
    return 0;
}

static int load_elf(task_t *task, const char *elf_data, size_t file_size, void **entry_point)
{
    if (task == NULL || elf_data == NULL || entry_point == NULL)
    {
        return -1;
    }
    if (file_size < sizeof(Elf64_Ehdr))
    {
        return -1;
    }
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        return -1;
    }
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        return -1;
    }
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB || ehdr->e_ident[EI_VERSION] != EV_CURRENT)
    {
        return -1;
    }
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    {
        return -1;
    }

    if (ehdr->e_machine != EM_X86_64)
    {
        return -1;
    }
    if (ehdr->e_phoff >= file_size || ehdr->e_phnum == 0)
    {
        return -1;
    }
    size_t phdr_table_size = ehdr->e_phnum * sizeof(Elf64_Phdr);
    if (ehdr->e_phoff + phdr_table_size > file_size)
    {
        return -1;
    }
    Elf64_Phdr *phdr = (Elf64_Phdr *)(elf_data + ehdr->e_phoff);
    unprotect(&task->pi->as);
    protect(&task->pi->as);
    uintptr_t brk_addr = 0;
    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type == PT_LOAD)
        {
            if (load_elf_segment(task, elf_data, file_size, &phdr[i]) < 0)
            {
                return -1;
            }
            printf("p_vaddr:0x%lx\n", (uintptr_t)phdr[i].p_vaddr);
            brk_addr=brk_addr<phdr[i].p_vaddr + phdr[i].p_memsz?phdr[i].p_vaddr + phdr[i].p_memsz:brk_addr;
        }
    }
    brk_addr = (brk_addr + 4095) & ~4095; // 向上对齐到 4096
    task->pi->brk = (void *)brk_addr;
    printf("current brk %p\n", (uintptr_t)task->pi->brk);
    uintptr_t entry_addr = ehdr->e_entry;
    if (entry_addr < UVSTART || entry_addr >= UVMEND)
    {
        printf("Invalid entry point: 0x%lx (valid range: 0x%lx - 0x%lx)\n",
               entry_addr, (uintptr_t)UVSTART, (uintptr_t)UVMEND);
        return -1;
    }
    *entry_point = (void *)entry_addr;
    printf("ELF loaded successfully, entry point: 0x%lx\n", entry_addr);
    return 0;
}

static int load_elf_segment(task_t *task, const char *elf_data, size_t file_size, Elf64_Phdr *phdr)
{
    if (phdr->p_memsz == 0)
    {
        return 0; // 空段，跳过
    }
    size_t pgsize = task->pi->as.pgsize;
    uintptr_t vaddr_start = phdr->p_vaddr;
    uintptr_t vaddr_end = vaddr_start + phdr->p_memsz;
    if (vaddr_start < UVSTART || vaddr_start >= UVMEND || vaddr_end > UVMEND)
    {
        printf("Invalid virtual address range: 0x%lx - 0x%lx (valid: 0x%lx - 0x%lx)\n",
               vaddr_start, vaddr_end, (uintptr_t)UVSTART, (uintptr_t)UVMEND);
        return -1;
    }
    uintptr_t page_start = vaddr_start & ~(pgsize - 1);
    uintptr_t page_end = (vaddr_end + pgsize - 1) & ~(pgsize - 1);
    size_t pages_needed = (page_end - page_start) / pgsize;
    if (pages_needed == 0 || pages_needed > 1024) // 限制最大1024页
    {
        printf("Invalid page count: %zu\n", pages_needed);
        return -1;
    }
    for (size_t j = 0; j < pages_needed; j++)
    {
        void *page = pmm->alloc(pgsize);
        if (page == NULL)
        {
            return -1;
        }
        memset(page, 0, pgsize);
        uintptr_t page_vaddr = page_start + j * pgsize;
        if (page_vaddr >= UVMEND)
        {
            printf("Invalid page virtual address: 0x%lx\n", page_vaddr);
            pmm->free(page);
            return -1;
        }
        if (page_vaddr < vaddr_end && (page_vaddr + pgsize) > vaddr_start)
        {
            if (copy_segment_data(elf_data, file_size, phdr, page, page_vaddr, vaddr_start, vaddr_end, pgsize) < 0)
            {
                pmm->free(page);
                return -1;
            }
        }
        int prot = MMAP_READ;
        if (phdr->p_flags & PF_W)
        {
            prot |= MMAP_WRITE;
        }

        map(&task->pi->as, (void *)page_vaddr, page, prot);
    }

    return 0;
}

static int copy_segment_data(const char *elf_data, size_t file_size, Elf64_Phdr *phdr,
                             void *page, uintptr_t page_vaddr, uintptr_t vaddr_start,
                             uintptr_t vaddr_end, size_t pgsize)
{
    uintptr_t copy_start = page_vaddr > vaddr_start ? page_vaddr : vaddr_start;
    uintptr_t copy_end = (page_vaddr + pgsize) < vaddr_end ? (page_vaddr + pgsize) : vaddr_end;
    if (copy_start < copy_end && phdr->p_filesz > 0)
    {
        size_t file_offset = phdr->p_offset + (copy_start - vaddr_start);
        size_t page_offset = copy_start - page_vaddr;
        size_t copy_size = copy_end - copy_start;
        if (phdr->p_offset >= file_size)
        {
            return -1;
        }
        size_t max_copy_from_file = phdr->p_filesz - (copy_start - vaddr_start);
        if (copy_size > max_copy_from_file)
        {
            copy_size = max_copy_from_file;
        }

        if (file_offset + copy_size <= file_size && copy_size > 0)
        {
            memcpy((char *)page + page_offset, elf_data + file_offset, copy_size);
        }
        else if (copy_size > 0)
        {
            return -1;
        }
    }
    return 0;
}
static uint64_t syscall_read(task_t *task, int fd, char *buf, size_t count)
{
    return vfs->read(fd, buf, count);
}
static uint64_t syscall_write(task_t *task, int fd, const char *buf, size_t count)
{
    return vfs->write(fd, buf, count);
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
    .sbrk = syscall_sbrk,
    .munmap = syscall_munmap,
    .mmap = syscall_mmap,
    .times = syscall_times,
    .uname = syscall_uname,
    .sched_yield = syscall_sched_yield,
    .nanosleep = syscall_nanosleep,
    .gettimeofday = syscall_gettimeofday,
    .clone = syscall_clone,
    .execve = syscall_execve,
    .read = syscall_read,
    .write = syscall_write};