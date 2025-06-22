#include <common.h>
#include <file.h>
#include <ext4.h>
static struct file_table ftable;

/**
 * @brief 初始化全局文件表
 */
static void fileinit(void)
{
    kmt->spin_init(&ftable.lock, "ftable");
}

/**
 * @brief 从全局文件表中分配一个struct file
 * @return 成功则返回指向struct file的指针，否则返回NULL
 */
static struct file *filealloc(void)
{
    struct file *f;

    kmt->spin_lock(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if (f->ref == 0)
        {
            f->ref = 1;
            kmt->spin_unlock(&ftable.lock);
            return f;
        }
    }
    kmt->spin_unlock(&ftable.lock);
    return NULL;
}

/**
 * @brief 增加文件引用计数
 * @param f 要增加引用的文件
 * @return 返回传入的文件指针
 */
static struct file *filedup(struct file *f)
{
    kmt->spin_lock(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    kmt->spin_unlock(&ftable.lock);
    return f;
}

/**
 * @brief 关闭文件，减少引用计数。如果引用计数为0，则真正关闭文件。
 * @param f 要关闭的文件
 */
static void fileclose(struct file *f)
{
    struct file ff;

    kmt->spin_lock(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0)
    {
        kmt->spin_unlock(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    kmt->spin_unlock(&ftable.lock);

    if (ff.type == FD_FILE)
    {
        ext4_fclose(ff.ptr);
        pmm->free(ff.ptr);
    }
    else if (ff.type == FD_DIR)
    {
        ext4_dir_close(ff.ptr);
        pmm->free(ff.ptr);
    }
}

/**
 * @brief 获取文件状态信息
 * @param f 文件
 * @param st 用于存储状态信息的kstat结构体指针
 * @return 成功返回0，失败返回-1
 */
static int filestat(struct file *f, struct stat *st)
{
    if (f->type == FD_FILE)
    {

        ext4_file *ef = (ext4_file *)f->ptr;
        st->st_mode = S_IFREG;
        st->st_ino = ef->inode;
        st->st_size = ef->fsize;
        st->st_nlink = ef->refctr;
        return 0;
    }
    else if (f->type == FD_DIR)
    {
        ext4_dir *d = (ext4_dir *)f->ptr;
        st->st_mode = S_IFDIR;
        st->st_ino = d->de.inode;
        st->st_size = d->f.fsize;
        st->st_nlink = 2; // Directories typically have a link count of at least 2 (., ..)
        return 0;
    }
    return -1;
}

/**
 * @brief 从文件读取数据
 * @param f 文件
 * @param buf 缓冲区地址
 * @param n 要读取的字节数
 * @return 成功则返回读取的字节数，失败返回-1
 */
static ssize_t fileread(struct file *f, void *buf, size_t n)
{
    ssize_t r = -1;
    if (!f->readable)
        return -1;
    if (f->type == FD_FILE)
    {
        size_t bytes_read;
        if (ext4_fread((ext4_file *)f->ptr, buf, n, &bytes_read) == EOK)
        {
            r = bytes_read;
        }
    }
    else if (f->type == FD_DIR)
    {
        // Directory reading logic
        struct dirent *de = (struct dirent *)buf;
        if (n < sizeof(*de))
            return -1; // Buffer too small

        ext4_dir *dir = (ext4_dir *)f->ptr;
        const ext4_direntry *entry = ext4_dir_entry_next(dir);

        if (!entry)
            return 0; // End of directory

        de->d_ino = entry->inode;
        de->d_off = dir->next_off;
        de->d_reclen = sizeof(struct dirent);
        de->d_type = entry->inode_type;
        strncpy(de->d_name, (const char *)entry->name, sizeof(de->d_name) - 1);
        de->d_name[sizeof(de->d_name) - 1] = '\0';

        r = sizeof(struct dirent);
    }
    else if (f->type == FD_DEVICE)
    {
        device_t *device = (device_t *)f->ptr;
        int value = 0;
        while (atomic_xchg(&value, ((tty_t *)(device->ptr))->cooked.value) == 0)
            ;
        r = device->ops->read(device, 0, buf, n);
    }
    else
    {
        panic("fileread");
    }

    return r;
}

/**
 * @brief 向文件写入数据
 * @param f 文件
 * @param buf 包含要写入数据的缓冲区地址
 * @param n 要写入的字节数
 * @return 成功则返回写入的字节数，失败返回-1
 */
static ssize_t filewrite(struct file *f, const void *buf, size_t n)
{
    ssize_t r = -1;

    if (!f->writable)
        return -1;

    if (f->type == FD_FILE)
    {
        size_t bytes_written;
        if (ext4_fwrite((ext4_file *)f->ptr, buf, n, &bytes_written) == EOK)
        {
            r = bytes_written;
        }
    }
    else if (f->type == FD_DEVICE)
    {
        device_t *device = f->ptr;
        r = device->ops->write(device, 0, buf, n);
    }
    else
    {
        panic("filewrite");
    }

    return r;
}
MODULE_DEF(file) = {
    .init = fileinit,
    .alloc = filealloc,
    .dup = filedup,
    .close = fileclose,
    .stat = filestat,
    .read = fileread,
    .write = filewrite,
};