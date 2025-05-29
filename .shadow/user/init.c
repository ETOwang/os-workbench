#include "ulib.h"
void putstr(char *str, int len)
{
  for (int i = 0; i < len; i++)
  {
    kputc(str[i]);
  }
}
int main()
{
  int pid = fork();
  if (pid == 0)
  {
    pid = fork();
    if (pid == 0)
    {
      putstr("Hello World from grandchild process\n", 36);
    }
    else
    {
      int status;
      if (status == 0)
      {
        putstr("Hello World from child process\n", 31);
      }
    }
  }
  else
  {
    pid=fork();
    if (pid == 0)
    {
      putstr("Hello World from child process\n", 31);
    }
    else
    {
      int status;
      if (status == 0)
      {
        putstr("Hello World from child process\n", 31);
      }
    }
  }
  return 0;
}