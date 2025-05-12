#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int printf(const char *fmt, ...)
{
  char buf[25600];
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(buf, fmt, ap);
  va_end(ap);
  assert(ret < sizeof(buf));
  putstr(buf);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap)
{
  char *str = out;
  const char *s;
  char buf[256];
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
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap)
{
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
