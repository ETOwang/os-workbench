#ifndef __FILE_H__
#define __FILE_H__
#include <common.h>
struct file
{
    enum
    {
        FD_NONE,
        FD_FILE,
        FD_DIR,
        FD_DEVICE,
        FD_PIPE
    } type;
    int ref; // reference count
    char readable;
    char writable;
    uint32_t off;
    char path[PATH_MAX]; // Add path to file struct
    void *ptr;           // Pointer to ext4_file, ext4_dir, etc.
};

// System-wide open file table
struct file_table
{
    spinlock_t lock;
    struct file file[NFILE];
};

#endif // __FILE_H__
