#ifndef __MYULIB_H__
#define __MYULIB_H__

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
// 避免包含系统头文件，直接定义需要的常量和结构
#define SYS_kputc 1
#define SYS_fork 2
#define SYS_sleep 14
#define SYS_getcwd 17
#define SYS_pipe2 59
#define SYS_dup 23
#define SYS_dup3 24
#define SYS_chdir 49
#define SYS_openat 56
#define SYS_close 57
#define SYS_getdents64 61
#define SYS_read 63
#define SYS_write 64
#define SYS_linkat 37
#define SYS_unlinkat 35
#define SYS_mkdirat 34
#define SYS_umount2 39
#define SYS_mount 40
#define SYS_fstat 80
#define SYS_clone 220
#define SYS_execve 221
#define SYS_wait4 260
#define SYS_exit 93
#define SYS_getppid 173
#define SYS_getpid 172
#define SYS_brk 214
#define SYS_munmap 215
#define SYS_mmap 222
#define SYS_times 153
#define SYS_uname 160
#define SYS_sched_yield 124
#define SYS_gettimeofday 169
#define SYS_nanosleep 101

// 常量定义
#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2

// 简单的时间结构
struct my_timespec
{
  long tv_sec;
  long tv_nsec;
};

static inline long syscall(int num, ...)
{
  va_list ap;
  va_start(ap, num);
  register long a0 asm("rax") = num;
  register long a1 asm("rdi") = va_arg(ap, long);
  register long a2 asm("rsi") = va_arg(ap, long);
  register long a3 asm("rdx") = va_arg(ap, long);
  register long a4 asm("r10") = va_arg(ap, long);
  va_end(ap);
  asm volatile("int $0x80"
               : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4)
               : "memory", "rcx", "r8", "r9", "r11");
  return a0;
}

static inline int my_kputc(char ch)
{
  return syscall(SYS_kputc, ch, 0, 0, 0);
}

static inline int my_fork()
{
  return syscall(SYS_fork, 0, 0, 0, 0);
}

static inline int my_exit(int status)
{
  return syscall(SYS_exit, status, 0, 0, 0);
}

static inline int my_getpid()
{
  return syscall(SYS_getpid, 0, 0, 0, 0);
}

static inline int my_sleep(int seconds)
{
  return syscall(SYS_sleep, seconds, 0, 0, 0);
}

static inline int my_openat(int dirfd, const char *pathname, int flags)
{
  return syscall(SYS_openat, dirfd, (uint64_t)pathname, flags, 0);
}

static inline int my_close(int fd)
{
  return syscall(SYS_close, fd, 0, 0, 0);
}

static inline int my_read(int fd, void *buf, size_t count)
{
  return syscall(SYS_read, fd, (uint64_t)buf, count, 0);
}

static inline int my_write(int fd, const void *buf, size_t count)
{
  return syscall(SYS_write, fd, (uint64_t)buf, count, 0);
}

static inline int my_chdir(const char *path)
{
  return syscall(SYS_chdir, (uint64_t)path, 0, 0, 0);
}

static inline int my_mount(const char *source, const char *target, const char *filesystemtype, unsigned long flags, const void *data)
{
  return syscall(SYS_mount, (uint64_t)source, (uint64_t)target, (uint64_t)filesystemtype, flags);
}

static inline int my_execve(const char *filename, char *const argv[], char *const envp[])
{
  return syscall(SYS_execve, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0);
}

static inline size_t strlen(const char *s)
{
  size_t len = 0;
  for (; *s; s++)
    len++;
  return len;
}

static inline char *strcpy(char *d, const char *s)
{
  char *r = d;
  while (*s)
  {
    *d++ = *s++;
  }
  *d = '\0';
  return r;
}

static inline char *strchr(const char *s, int c)
{
  for (; *s; s++)
  {
    if (*s == c)
      return (char *)s;
  }
  return NULL;
}

static inline void print(const char *s, ...)
{
  va_list ap;
  va_start(ap, s);
  while (s != NULL)
  {
    syscall(SYS_write, 2, s, strlen(s));
    s = va_arg(ap, const char *);
  }
  va_end(ap);
}

static inline void print_int(int num)
{
  char buf[32];
  char *p = buf + sizeof(buf) - 1;
  *p = '\0';

  int is_negative = 0;
  if (num < 0)
  {
    is_negative = 1;
    num = -num;
  }

  if (num == 0)
  {
    *(--p) = '0';
  }
  else
  {
    while (num > 0)
    {
      *(--p) = '0' + (num % 10);
      num /= 10;
    }
  }

  if (is_negative)
  {
    *(--p) = '-';
  }

  syscall(SYS_write, 2, p, strlen(p));
}

#define assert(cond)              \
  do                              \
  {                               \
    if (!(cond))                  \
    {                             \
      print("Panicked.\n", NULL); \
      syscall(SYS_exit, 1);       \
    }                             \
  } while (0)

static char mem[4096], *freem = mem;

static inline void *zalloc(size_t sz)
{
  // This is memory leak!
  assert(freem + sz < mem + sizeof(mem));
  void *ret = freem;
  freem += sz;
  return ret;
}

#endif // __MYULIB_H__
