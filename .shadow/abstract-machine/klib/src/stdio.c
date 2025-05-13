#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// 添加一个简单的自旋锁实现，确保printf的线程安全
static int print_lock = 0;

static inline void print_lock_acquire()
{
  // 获取当前中断状态（如果在系统支持的情况下）
  bool intr_was_enabled = ienabled();

  // 先关闭中断，防止在多核环境下被其他中断处理程序打断
  if (intr_was_enabled)
    iset(false);

  // 原子操作：获取锁
  while (__sync_lock_test_and_set(&print_lock, 1));

  // 锁已获取，可以安全地重新启用中断
  if (intr_was_enabled)
    iset(true);
}

static inline void print_lock_release()
{
  // 原子操作：释放锁
  __sync_lock_release(&print_lock);
}

int printf(const char *fmt, ...)
{
  char buf[25600];
  va_list ap;

  // 获取锁，确保多核/多线程安全
  print_lock_acquire();

  va_start(ap, fmt);
  int ret = vsprintf(buf, fmt, ap);
  va_end(ap);

  assert(ret < sizeof(buf));
  putstr(buf);

  // 释放锁
  print_lock_release();

  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap)
{
  // vsprintf是线程安全的，与vsnprintf类似
  // 只操作局部变量和传入的参数，因此不需要额外的同步

  char *str = out;
  const char *s;
  char buf[256]; // 这个缓冲区是线程本地的，所以是安全的
  int i, len;
  uint32_t cur;
  for (; *fmt; fmt++)
  {
    if (*fmt != '%')
    {
      *str++ = *fmt;
      continue;
    }
    switch (*++fmt)
    {
    case 's':
      s = va_arg(ap, char *);
      while (*s)
      {
        *str++ = *s++;
      }
      break;
    case 'd':
      i = va_arg(ap, int);
      if (i < 0)
      {
        *str++ = '-';
        i = -i;
      }
      char *p = buf + sizeof(buf) - 1;
      *p = '\0';
      do
      {
        *--p = '0' + (i % 10);
        i /= 10;
      } while (i != 0);
      while (*p)
      {
        *str++ = *p++;
      }
      break;
    case 'x':
      cur = va_arg(ap, uint32_t);
      p = buf + sizeof(buf) - 1;
      *p = '\0';
      do
      {
        int digit = cur % 16;
        *--p = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
        cur /= 16;
      } while (cur != 0);
      while (*p)
      {
        *str++ = *p++;
      }
      break;
    case 'p':
      cur = va_arg(ap, uint32_t);
      len = sprintf(buf, "0x%x", cur);
      for (int j = 0; j < len; j++)
      {
        *str++ = buf[j];
      }
      break;
    case 'c':
      i = va_arg(ap, int);
      *str++ = i;
      break;
    default:
      break;
    }
  }
  *str = '\0';
  return str - out;
}

int sprintf(char *out, const char *fmt, ...)
{
  va_list ap;

  // sprintf不需要锁，因为输出到用户提供的缓冲区
  // 用户应负责确保对同一缓冲区的访问同步

  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...)
{
  va_list ap;

  // snprintf不需要锁，原因同上

  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap)
{
  // vsnprintf是线程安全的，因为它只操作局部变量和传入的参数
  // 不需要额外的同步机制，除非多线程共享同一个输出缓冲区

  char *str = out;
  const char *s;
  char buf[256];
  int i, len;
  uint32_t cur;
  size_t remaining = n;

  for (; *fmt && remaining > 1; fmt++)
  {
    if (*fmt != '%')
    {
      *str++ = *fmt;
      remaining--;
      continue;
    }
    switch (*++fmt)
    {
    case 's':
      s = va_arg(ap, char *);
      while (*s && remaining > 1)
      {
        *str++ = *s++;
        remaining--;
      }
      break;
    case 'd':
      i = va_arg(ap, int);
      if (i < 0)
      {
        if (remaining > 1)
        {
          *str++ = '-';
          remaining--;
        }
        i = -i;
      }
      char *p = buf + sizeof(buf) - 1;
      *p = '\0';
      do
      {
        *--p = '0' + (i % 10);
        i /= 10;
      } while (i != 0);
      while (*p && remaining > 1)
      {
        *str++ = *p++;
        remaining--;
      }
      break;
    case 'x':
      cur = va_arg(ap, uint32_t);
      p = buf + sizeof(buf) - 1;
      *p = '\0';
      do
      {
        int digit = cur % 16;
        *--p = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
        cur /= 16;
      } while (cur != 0);
      while (*p && remaining > 1)
      {
        *str++ = *p++;
        remaining--;
      }
      break;
    case 'p':
      cur = va_arg(ap, uint32_t);
      len = snprintf(buf, sizeof(buf), "0x%x", cur);
      for (int j = 0; j < len && remaining > 1; j++)
      {
        *str++ = buf[j];
        remaining--;
      }
      break;
    case 'c':
      i = va_arg(ap, int);
      if (remaining > 1)
      {
        *str++ = i;
        remaining--;
      }
      break;
    default:
      break;
    }
  }
  if (remaining > 0)
  {
    *str = '\0';
  }
  else if (n > 0)
  {
    out[n - 1] = '\0';
  }
  return str - out;
}

#endif
