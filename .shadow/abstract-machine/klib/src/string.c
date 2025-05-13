#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s)
{
  size_t len = 0;
  while (s[len] != '\0')
  {
    len++;
  }
  return len;
}

char *strcpy(char *dst, const char *src)
{
  size_t index = 0;
  while (src[index] != '\0')
  {
    dst[index] = src[index];
    index++;
  }
  dst[index] = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  for (; i < n; i++)
  {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src)
{
  size_t dest_len = strlen(dst);
  size_t src_len = strlen(src);
  for (size_t i = 0; i < src_len; i++)
  {
    dst[dest_len + i] = src[i];
  }
  dst[dest_len + src_len] = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2)
{
  size_t s1_len = strlen(s1);
  size_t s2_len = strlen(s2);
  for (size_t i = 0; i < s1_len && i < s2_len; i++)
  {
    if ((__u_char)s1[i] > (__u_char)s2[i] || (__u_char)s1[i] < (__u_char)s2[i])
    {
      return (int)(__u_char)s1[i] - (int)(__u_char)s2[i];
    }
  }

  if (s1_len == s2_len)
  {
    return 0;
  }
  else if (s1_len > s2_len)
  {
    return (int)(__u_char)s1[s2_len];
  }
  else
  {
    return -(int)(__u_char)s2[s1_len];
  }
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  size_t s1_len = strlen(s1);
  size_t s2_len = strlen(s2);
  for (size_t i = 0; i < s1_len && i < s2_len && i < n; i++)
  {
    if ((__u_char)s1[i] > (__u_char)s2[i] || (__u_char)s1[i] < (__u_char)s2[i])
    {
      return(int)(__u_char)s1[i] - (int)(__u_char)s2[i];
    }
  }
  if (s1_len >= n && s2_len >= n)
  {
    return 0;
  }
  else if (s1_len > s2_len)
  {
    return (int)(__u_char)s1[s2_len];
  }
  else
  {
    return -(int)(__u_char)s2[s1_len];
  }
}
void *memset(void *s, int c, size_t n)
{
  char *dest = (char *)s;
  for (size_t i = 0; i < n; i++)
  {
    dest[i] = (char)c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n)
{
  char temp[1000000];
  assert(n < 1000000);
  char *d = (char *)dst;
  char *s = (char *)src;
  for (size_t i = 0; i < n; i++)
  {
    temp[i] = s[i];
  }
  for (size_t i = 0; i < n; i++)
  {
    d[i] = temp[i];
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n)
{
  char *dst = (char *)out;
  char *src = (char *)in;
  for (size_t i = 0; i < n; i++)
  {
    dst[i] = src[i];
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  if (n == 0)
  {
    return 0;
  }
  return strncmp((char*)s1,(char*)s2,n);
}

#endif
