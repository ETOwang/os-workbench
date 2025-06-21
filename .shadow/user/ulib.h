#ifndef __ULIB_H__
#define __ULIB_H__
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "../kernel/framework/syscall.h"
#define DIRSIZ 255
// Basic type definitions
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32_t int32;
typedef int64_t int64;
typedef uint32_t mode_t;

static inline long syscall(int num, long arg1, long arg2, long arg3, long arg4)
{
  /**
   * problems when use variable arguments in inline assembly
   */
  register long a0 asm("rax") = num;
  register long a1 asm("rdi") = arg1;
  register long a2 asm("rsi") = arg2;
  register long a3 asm("rdx") = arg3;
  register long a4 asm("r10") = arg4;
  asm volatile("int $0x80"
               : "+r"(a0)
               : "r"(a1), "r"(a2), "r"(a3), "r"(a4)
               : "memory", "rcx", "r8", "r9", "r11");
  return a0;
}

static inline int gettimeofday(struct timespec *ts)
{
  return syscall(SYS_gettimeofday, (uint64_t)ts, 0, 0, 0);
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

static inline pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage)
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
static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  return (void *)syscall(SYS_mmap, (uint64_t)addr, length, prot, flags);
}

static inline clock_t times(struct tms *buf)
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
static inline int nanosleep(const struct timespec *req, struct timespec *rem)
{
  return syscall(SYS_nanosleep, (uint64_t)req, (uint64_t)rem, 0, 0);
}

static inline int kputc(char ch)
{
  return syscall(SYS_kputc, ch, 0, 0, 0);
}

static inline int openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
  return syscall(SYS_openat, dirfd, (uint64_t)pathname, flags, mode);
}
static inline int fork()
{
  return syscall(SYS_fork, 0, 0, 0, 0);
}

static inline int exit(int status)
{
  return syscall(SYS_exit, status, 0, 0, 0);
}

static inline int getpid()
{
  return syscall(SYS_getpid, 0, 0, 0, 0);
}

static inline int sleep(int seconds)
{
  return syscall(SYS_sleep, seconds, 0, 0, 0);
}

// String functions
static inline size_t strlen(const char *s)
{
  size_t n;
  for (n = 0; s[n]; n++)
    ;
  return n;
}

static inline char *strcpy(char *s, const char *t)
{
  char *os = s;
  while ((*s++ = *t++) != 0)
    ;
  return os;
}

static inline int strcmp(const char *p, const char *q)
{
  while (*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

static inline int strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

static inline char *strcat(char *dest, const char *src)
{
  char *d = dest;
  while (*d)
    d++;
  while ((*d++ = *src++))
    ;
  return dest;
}

static inline char *strchr(const char *s, int c)
{
  for (; *s; s++)
    if (*s == c)
      return (char *)s;
  return 0;
}

// Memory functions
static inline void *memset(void *dst, int c, uint n)
{
  char *cdst = (char *)dst;
  int i;
  for (i = 0; i < n; i++)
  {
    cdst[i] = c;
  }
  return dst;
}

static inline void *memcpy(void *dst, const void *src, uint n)
{
  const char *s = src;
  char *d = dst;
  while (n-- > 0)
    *d++ = *s++;
  return dst;
}

static inline void *memmove(void *vdst, const void *vsrc, int n)
{
  char *dst = vdst;
  const char *src = vsrc;
  if (src > dst)
  {
    while (n-- > 0)
      *dst++ = *src++;
  }
  else
  {
    dst += n;
    src += n;
    while (n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

static inline int memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0)
  {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++, p2++;
  }
  return 0;
}

// Character functions
static inline int isdigit(int c)
{
  return c >= '0' && c <= '9';
}

static inline int isspace(int c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// Number conversion
static inline int atoi(const char *s)
{
  int n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}

// Standard I/O
extern void printf(const char *fmt, ...);
extern void fprintf(int fd, const char *fmt, ...);

// Memory allocation
extern void *malloc(uint nbytes);
extern void free(void *ptr);

static inline char *
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;)
  {
    cc = read(0, &c, 1);
    if (cc < 1)
      break;
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

// Utility functions
static inline int min(int a, int b)
{
  return a < b ? a : b;
}

static inline int max(int a, int b)
{
  return a > b ? a : b;
}
#endif