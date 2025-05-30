#include <stddef.h>
#include <stdint.h>
#include "../kernel/framework/syscall.h"
#include "../kernel/framework/user.h"

static inline long syscall(int num, long x1, long x2, long x3, long x4)
{
  register long a0 asm("rax") = num;
  register long a1 asm("rdi") = x1;
  register long a2 asm("rsi") = x2;
  register long a3 asm("rdx") = x3;
  register long a4 asm("rcx") = x4;
  asm volatile("int $0x80"
               : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4)
               : "memory");
  return a0;
}
static inline int gettimeofday(struct timespec *ts)
{
  return syscall(SYS_gettimeofday, (uint64_t)ts, 0, 0, 0);
}
static inline int open(const char *pathname, int flags)
{
  return syscall(SYS_open, (uint64_t)pathname, flags, 0, 0);
}
static inline int close(int fd)
{
  return syscall(SYS_close, fd, 0, 0, 0);
}
static inline int read(int fd, void *buf, size_t count)
{
  return syscall(SYS_read, fd, (uint64_t)buf, count, 0);
}
static inline int write(int fd, const void *buf, size_t count)
{
  return syscall(SYS_write, fd, (uint64_t)buf, count, 0);
}
static inline int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
  return syscall(SYS_linkat, olddirfd, (uint64_t)oldpath, newdirfd, (uint64_t)newpath);
}
static inline int unlinkat(int dirfd, const char *pathname, int flags)
{
  return syscall(SYS_unlinkat, dirfd, (uint64_t)pathname, flags, 0);
}
static inline int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
  return syscall(SYS_mkdirat, dirfd, (uint64_t)pathname, mode, 0);
}
static inline int umount2(const char *target, int flags)
{
  return syscall(SYS_umount2, (uint64_t)target, flags, 0, 0);
}
static inline int mount(const char *source, const char *target, const char *filesystemtype, unsigned long flags, const void *data)
{
  return syscall(SYS_mount, (uint64_t)source, (uint64_t)target, (uint64_t)filesystemtype, flags);
}
static inline int fstat(int fd, struct stat *statbuf)
{
  return syscall(SYS_fstat, fd, (uint64_t)statbuf, 0, 0);
}
static inline int clone(int flags, void *stack, int *ptid, int *ctid, unsigned long newtls)
{
  return syscall(SYS_clone, flags, (uint64_t)stack, (uint64_t)ptid, (uint64_t)ctid);
}
static inline int execve(const char *filename, char *const argv[], char *const envp[])
{
  return syscall(SYS_execve, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0);
}
static inline int wait4(int pid, int *status, int options)
{
  return syscall(SYS_wait4, pid, (uint64_t)status, options, 0);
}
static inline int getppid()
{
  return syscall(SYS_getppid, 0, 0, 0, 0);
}
static inline int brk(void *addr)
{
  return syscall(SYS_brk, (uint64_t)addr, 0, 0, 0);
}
static inline int munmap(void *addr, size_t length)
{
  return syscall(SYS_munmap, (uint64_t)addr, length, 0, 0);
}
static inline void *mmap(void *addr, size_t length, int prot, int flags)
{
  return (void *)syscall(SYS_mmap, (uint64_t)addr, length, prot, flags);
}

static inline int times(struct tms *buf)
{
  return syscall(SYS_times, (uint64_t)buf, 0, 0, 0);
}
static inline int uname(struct utsname *buf)
{
  return syscall(SYS_uname, (uint64_t)buf, 0, 0, 0);
}
static inline int sched_yield()
{
  return syscall(SYS_sched_yield, 0, 0, 0, 0);
}
static inline int getcwd(char *buf, size_t size)
{
  return syscall(SYS_getcwd, (uint64_t)buf, size, 0, 0);
}
static inline int pipe2(int pipefd[2], int flags)
{
  return syscall(SYS_pipe2, (uint64_t)pipefd, flags, 0, 0);
}
static inline int dup(int oldfd)
{
  return syscall(SYS_dup, oldfd, 0, 0, 0);
}
static inline int dup3(int oldfd, int newfd, int flags)
{
  return syscall(SYS_dup3, oldfd, newfd, flags, 0);
}
static inline int chdir(const char *path)
{
  return syscall(SYS_chdir, (uint64_t)path, 0, 0, 0);
}
static inline int openat(int dirfd, const char *pathname, int flags)
{
  return syscall(SYS_openat, dirfd, (uint64_t)pathname, flags, 0);
}
static inline int nanosleep(const struct timespec *req, struct timespec *rem)
{
  return syscall(SYS_nanosleep, (uint64_t)req, (uint64_t)rem, 0, 0);
}

static inline int kputc(char ch)
{
  return syscall(SYS_kputc, ch, 0, 0, 0);
}

static inline int fork()
{
  return syscall(SYS_fork, 0, 0, 0, 0);
}

static inline int exit(int status)
{
  return syscall(SYS_exit, status, 0, 0, 0);
}

static inline int kill(int pid)
{
  return syscall(SYS_kill, pid, 0, 0, 0);
}

static inline int getpid()
{
  return syscall(SYS_getpid, 0, 0, 0, 0);
}

static inline int sleep(int seconds)
{
  return syscall(SYS_sleep, seconds, 0, 0, 0);
}
