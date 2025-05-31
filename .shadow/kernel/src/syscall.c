#include<common.h>


static uint64_t syscall_chdir(task_t *task, const char *path) {
    if (path == NULL || strlen(path) >= PATH_MAX)
    {
        return -1;
    }
    int ret = vfs->opendir(path);
    if (ret < 0) {
        return -1; 
    }
    vfs->closedir(ret);
    strncpy(task->pi->cwd, path, strlen(path) + 1);
    return 0;
}
MODULE_DEF(syscall) = {
    .chdir = syscall_chdir,
};