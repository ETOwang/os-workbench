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
    printf("chdir: %s\n", full_path);
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

    // 打开可执行文件
    int fd = vfs->open(pathname, 0);
    if (fd < 0)
    {
        return -1;
    }
    // 获取文件大小
    struct kstat stat;
    if (syscall_fstat(task, fd, &stat) < 0)
    {
        vfs->close(fd);
        return -1;
    }

    // 分配内存加载ELF文件
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

    // 读取整个ELF文件
    ssize_t bytes_read = vfs->read(fd, elf_data, file_size);
    vfs->close(fd);
    if (bytes_read != file_size)
    {
        pmm->free(elf_data);
        return -1;
    }

    // 使用ELF加载器加载程序
    void *entry_point;
    if (load_elf(task, elf_data, file_size, &entry_point) < 0)
    {
        pmm->free(elf_data);
        return -1;
    }

    // 释放ELF数据
    pmm->free(elf_data);

    // 计算参数和环境变量的数量和总大小
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

    // 计算栈上需要的总空间
    // argv指针数组 + envp指针数组 + 字符串数据 + 对齐
    size_t stack_needed = (argc + 1) * sizeof(char *) + // argv数组
                          (envc + 1) * sizeof(char *) + // envp数组
                          args_size + envs_size +       // 字符串数据
                          16;                           // 对齐空间

    if (stack_needed > STACK_SIZE / 2)
    {
        return -1;
    }

    // 分配栈空间
    char *mem = pmm->alloc(task->pi->as.pgsize);
    map(&task->pi->as, (void *)(long)UVMEND - task->pi->as.pgsize, (void *)mem, MMAP_READ | MMAP_WRITE);

    // 创建新的上下文，使用ELF入口点
    task->context = ucontext(&task->pi->as, RANGE(task->stack, task->stack + STACK_SIZE), entry_point);
    // 设置新的栈，从栈顶开始向下构建参数
    char *stack_top = (char *)task->context->rsp;
    char *stack_ptr = stack_top;

    // 首先复制字符串数据（从栈顶向下）
    char **argv_ptrs = pmm->alloc((argc + 1) * sizeof(char *));
    char **envp_ptrs = pmm->alloc((envc + 1) * sizeof(char *));

    // 复制环境变量字符串
    for (int i = envc - 1; i >= 0; i--)
    {
        size_t len = strlen(envp[i]) + 1;
        stack_ptr -= len;
        memcpy(stack_ptr, envp[i], len);
        envp_ptrs[i] = stack_ptr;
    }

    // 复制参数字符串
    for (int i = argc - 1; i >= 0; i--)
    {
        size_t len = strlen(argv[i]) + 1;
        stack_ptr -= len;
        memcpy(stack_ptr, argv[i], len);
        argv_ptrs[i] = stack_ptr;
    }

    // 对齐到8字节边界
    stack_ptr = (char *)((uintptr_t)stack_ptr & ~7);

    // 复制envp指针数组
    stack_ptr -= (envc + 1) * sizeof(char *);
    char **envp_array = (char **)stack_ptr;
    for (int i = 0; i < envc; i++)
    {
        envp_array[i] = envp_ptrs[i];
    }
    envp_array[envc] = NULL;

    // 复制argv指针数组
    stack_ptr -= (argc + 1) * sizeof(char *);
    char **argv_array = (char **)stack_ptr;
    for (int i = 0; i < argc; i++)
    {
        argv_array[i] = argv_ptrs[i];
    }
    argv_array[argc] = NULL;

    // 压入argc
    stack_ptr -= sizeof(int);
    *((int *)stack_ptr) = argc;

    // 清理临时分配的内存
    pmm->free(argv_ptrs);
    pmm->free(envp_ptrs);
    // 设置栈指针到我们构建的参数位置
    // 注意：这里需要根据具体的架构调整寄存器设置
    // 对于x86_64，栈指针在RSP中
    task->context->rsp = (uintptr_t)stack_ptr;
    // execve成功时不返回到原程序
    return 0;
}

/**
 * ELF加载器 - 解析并加载ELF文件到进程地址空间
 * @param task 目标任务
 * @param elf_data ELF文件数据
 * @param file_size 文件大小
 * @param entry_point 输出参数，返回程序入口点
 * @return 0成功，-1失败
 */
static int load_elf(task_t *task, const char *elf_data, size_t file_size, void **entry_point)
{
    // 验证参数
    if (task == NULL || elf_data == NULL || entry_point == NULL)
    {
        return -1;
    }

    // 验证文件大小
    if (file_size < sizeof(Elf64_Ehdr))
    {
        return -1;
    }

    // 解析ELF头
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;

    // 验证ELF魔数
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        return -1;
    }

    // 检查是否为64位ELF
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        return -1;
    }

    // 验证程序头表偏移
    if (ehdr->e_phoff >= file_size || ehdr->e_phnum == 0)
    {
        return -1;
    }

    // 获取程序头表
    Elf64_Phdr *phdr = (Elf64_Phdr *)(elf_data + ehdr->e_phoff);

    // 清理当前地址空间
    unprotect(&task->pi->as);
    protect(&task->pi->as);

    // 加载所有LOAD段
    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type == PT_LOAD)
        {
            if (load_elf_segment(task, elf_data, file_size, &phdr[i]) < 0)
            {
                return -1;
            }
        }
    }

    // 验证程序入口点
    uintptr_t entry_addr = ehdr->e_entry;
    if (entry_addr < UVSTART || entry_addr >= UVMEND)
    {
        printf("Invalid entry point: 0x%lx (valid range: 0x%lx - 0x%lx)\n",
               entry_addr, (uintptr_t)UVSTART, (uintptr_t)UVMEND);
        return -1;
    }

    // 设置程序入口点
    *entry_point = (void *)entry_addr;
    printf("ELF loaded successfully, entry point: 0x%lx\n", entry_addr);
    return 0;
}

/**
 * 加载单个ELF段
 * @param task 目标任务
 * @param elf_data ELF文件数据
 * @param file_size 文件大小
 * @param phdr 程序头
 * @return 0成功，-1失败
 */
static int load_elf_segment(task_t *task, const char *elf_data, size_t file_size, Elf64_Phdr *phdr)
{
    // 验证段的基本信息
    if (phdr->p_memsz == 0)
    {
        return 0; // 空段，跳过
    }

    // 计算需要的页面数量
    size_t pgsize = task->pi->as.pgsize;
    uintptr_t vaddr_start = phdr->p_vaddr;
    uintptr_t vaddr_end = vaddr_start + phdr->p_memsz;

    // 验证虚拟地址范围是否在用户空间内
    if (vaddr_start < UVSTART || vaddr_start >= UVMEND || vaddr_end > UVMEND)
    {
        printf("Invalid virtual address range: 0x%lx - 0x%lx (valid: 0x%lx - 0x%lx)\n",
               vaddr_start, vaddr_end, (uintptr_t)UVSTART, (uintptr_t)UVMEND);
        return -1;
    }

    // 页面对齐
    uintptr_t page_start = vaddr_start & ~(pgsize - 1);
    uintptr_t page_end = (vaddr_end + pgsize - 1) & ~(pgsize - 1);
    size_t pages_needed = (page_end - page_start) / pgsize;

    // 验证页面数量是否合理
    if (pages_needed == 0 || pages_needed > 1024) // 限制最大1024页
    {
        printf("Invalid page count: %zu\n", pages_needed);
        return -1;
    }

    // 为每个页面分配物理内存并映射
    for (size_t j = 0; j < pages_needed; j++)
    {
        void *page = pmm->alloc(pgsize);
        if (page == NULL)
        {
            return -1;
        }

        // 清零页面
        memset(page, 0, pgsize);

        // 计算当前页面的虚拟地址
        uintptr_t page_vaddr = page_start + j * pgsize;

        // 验证页面虚拟地址是否有效（应该已经通过前面的检查）
        if (page_vaddr >= UVMEND)
        {
            printf("Invalid page virtual address: 0x%lx\n", page_vaddr);
            pmm->free(page);
            return -1;
        }

        // 如果这个页面包含段数据，复制数据
        if (page_vaddr < vaddr_end && (page_vaddr + pgsize) > vaddr_start)
        {
            if (copy_segment_data(elf_data, file_size, phdr, page, page_vaddr, vaddr_start, vaddr_end, pgsize) < 0)
            {
                pmm->free(page);
                return -1;
            }
        }

        // 验证物理页面地址
        if (page == NULL)
        {
            printf("Invalid physical page address\n");
            return -1;
        }

        // 映射页面
        map(&task->pi->as, (void *)page_vaddr, page, MMAP_READ);
    }

    return 0;
}

/**
 * 复制段数据到页面
 */
static int copy_segment_data(const char *elf_data, size_t file_size, Elf64_Phdr *phdr,
                             void *page, uintptr_t page_vaddr, uintptr_t vaddr_start,
                             uintptr_t vaddr_end, size_t pgsize)
{
    // 计算在页面内的偏移和大小
    uintptr_t copy_start = page_vaddr > vaddr_start ? page_vaddr : vaddr_start;
    uintptr_t copy_end = (page_vaddr + pgsize) < vaddr_end ? (page_vaddr + pgsize) : vaddr_end;

    if (copy_start < copy_end && phdr->p_filesz > 0)
    {
        size_t file_offset = phdr->p_offset + (copy_start - vaddr_start);
        size_t page_offset = copy_start - page_vaddr;
        size_t copy_size = copy_end - copy_start;

        // 确保不超出文件大小
        if (file_offset + copy_size <= file_size)
        {
            memcpy((char *)page + page_offset, elf_data + file_offset, copy_size);
        }
        else
        {
            return -1;
        }
    }

    return 0;
}
static uint64_t syscall_read(task_t *task, int fd, char *buf, size_t count)
{
    uintptr_t *ptep = ptewalk(&task->pi->as, (uintptr_t)buf);
    buf = (char *)(PTE_ADDR(*ptep) | ((uintptr_t)buf & 0xFFF));
    panic_on(buf == NULL, "Invalid buffer address");
    return vfs->read(fd, buf, count);
}
static uint64_t syscall_write(task_t *task, int fd, const char *buf, size_t count)
{
    uintptr_t *ptep = ptewalk(&task->pi->as, (uintptr_t)buf);
    buf = (const char *)(PTE_ADDR(*ptep) | ((uintptr_t)buf & 0xFFF));
    panic_on(buf == NULL, "Invalid buffer address");
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
    .read = syscall_read,
    .write = syscall_write};