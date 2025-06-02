#include <am.h>
#include <syscall.h>
#define MODULE(mod)                           \
  typedef struct mod_##mod##_t mod_##mod##_t; \
  extern mod_##mod##_t *mod;                  \
  struct mod_##mod##_t

#define MODULE_DEF(mod)                \
  extern mod_##mod##_t __##mod##_obj;  \
  mod_##mod##_t *mod = &__##mod##_obj; \
  mod_##mod##_t __##mod##_obj

typedef Context *(*handler_t)(Event, Context *);
MODULE(os)
{
  void (*init)();
  void (*run)();
  Context *(*trap)(Event ev, Context *context);
  void (*on_irq)(int seq, int event, handler_t handler);
};

MODULE(pmm)
{
  void (*init)();
  void *(*alloc)(size_t size);
  void (*free)(void *ptr);
};

typedef struct task task_t;
typedef struct spinlock spinlock_t;
typedef struct semaphore sem_t;
MODULE(kmt)
{
  void (*init)();
  int (*create)(task_t *task, const char *name, void (*entry)(void *arg), void *arg);
  void (*teardown)(task_t *task);
  void (*spin_init)(spinlock_t *lk, const char *name);
  void (*spin_lock)(spinlock_t *lk);
  void (*spin_unlock)(spinlock_t *lk);
  void (*sem_init)(sem_t *sem, const char *name, int value);
  void (*sem_wait)(sem_t *sem);
  void (*sem_signal)(sem_t *sem);
};

typedef struct device device_t;
MODULE(dev)
{
  void (*init)();
  device_t *(*lookup)(const char *name);
};
typedef struct procinfo procinfo_t;
MODULE(uproc)
{
  void (*init)();
  int (*kputc)(task_t *task, char ch);
  int (*fork)(task_t *task);
  int (*wait)(task_t *task, int pid, int *status, int options);
  int (*exit)(task_t *task, int status);
  void *(*mmap)(task_t *task, void *addr, int length, int prot, int flags);
  int (*getpid)(task_t *task);
  int (*getppid)(task_t *task);
  int (*sleep)(task_t *task, int seconds);
  int64_t (*uptime)(task_t *task, struct timespec *tv);
};

typedef long off_t;
typedef long ssize_t;
MODULE(vfs)
{

  void (*init)(void);
  int (*mount)(const char *dev_name, const char *mount_point,
               const char *fs_type, int flags, void *data);
  int (*umount)(const char *mount_point);
  int (*open)(const char *pathname, int flags);
  int (*close)(int fd);
  ssize_t (*read)(int fd, void *buf, size_t count);
  ssize_t (*write)(int fd, const void *buf, size_t count);
  off_t (*seek)(int fd, off_t offset, int whence);
  int (*mkdir)(const char *pathname);
  int (*rmdir)(const char *pathname);
  int (*link)(const char *oldpath, const char *newpath);
  int (*unlink)(const char *pathname);
  int (*rename)(const char *oldpath, const char *newpath);
  int (*opendir)(const char *pathname);
  int (*readdir)(int fd, struct dirent *entry);
  int (*closedir)(int fd);
  int (*stat)(int fd,  struct kstat *stat);
  int (*dup)(int oldfd);
  int (*dup3)(int oldfd, int newfd, int flags);
  const char* (*getdirpath)(int fd);
};

MODULE(syscall)
{
  uint64_t (*chdir)(task_t *task, const char *path);
  uint64_t (*getcwd)(task_t *task, char *buf, size_t size);
  uint64_t (*openat)(task_t *task, int fd, const char *filename, int flags, mode_t mode);
  uint64_t (*pipe2)(task_t *task, int pipefd[2], int flags);
  uint64_t (*dup)(task_t *task, int oldfd);
  uint64_t (*dup3)(task_t *task, int oldfd, int newfd, int flags);
  uint64_t (*getdents64)(task_t *task, int fd, struct dirent *buf, size_t len);
  uint64_t (*linkat)(task_t *task, int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
  uint64_t (*unlinkat)(task_t *task, int dirfd, const char *path, int flags);
  uint64_t (*mkdirat)(task_t *task, int dirfd, const char *path, mode_t mode);
  uint64_t (*umount2)(task_t *task, const char *target, int flags);
  uint64_t (*mount)(task_t *task, const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);
  uint64_t (*fstat)(task_t *task, int fd, struct kstat *statbuf);
  uint64_t (*brk)(task_t *task, void *addr);
  uint64_t (*munmap)(task_t *task, void *addr, size_t length);
  uint64_t (*mmap)(task_t *task, void *addr, size_t length, int prot, int flags, int fd, off_t offset);
  uint64_t (*times)(task_t *task, struct tms *buf);
  uint64_t (*uname)(task_t *task, struct utsname *buf);
  uint64_t (*sched_yield)(task_t *task);
  uint64_t (*nanosleep)(task_t *task, const struct timespec *req, struct timespec *rem);
  uint64_t (*gettimeofday)(task_t *task, struct timespec *ts, void *tz);
  uint64_t (*clone)(task_t *task, int flags, void *stack, int *ptid, int *ctid, unsigned long newtls);
  uint64_t (*execve)(task_t *task, const char *pathname, char *const argv[], char *const envp[]);
};