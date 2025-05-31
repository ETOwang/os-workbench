#include <common.h>

static uint64_t syscall_chdir(task_t *task, const char *path)
{
    if (path == NULL || strlen(path) >= PATH_MAX)
    {
        return -1;
    }
    int ret = vfs->opendir(path);
    if (ret < 0)
    {
        return -1;
    }
    vfs->closedir(ret);
    strncpy(task->pi->cwd, path, strlen(path) + 1);
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
MODULE_DEF(syscall) = {
    .chdir = syscall_chdir,
    .getcwd = syscall_getcwd,
};