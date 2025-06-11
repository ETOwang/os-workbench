#include <common.h>
#include <vfs.h>
#include <ext4.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
// 简单的块设备接口实现 - 直接使用AM磁盘接口
static struct ext4_blockdev_iface bi;
static struct ext4_blockdev bd;
static uint8_t block_buffer[4096];
#define MAX_OPEN_FILES 256
static struct
{
	bool in_use;
	ext4_file *file;
} open_files[MAX_OPEN_FILES];

#define MAX_OPEN_DIRS 64
static struct
{
	bool in_use;
	ext4_dir *dir;
} open_dirs[MAX_OPEN_DIRS];

static int blockdev_open(struct ext4_blockdev *bdev)
{
	return EOK;
}

static int blockdev_close(struct ext4_blockdev *bdev)
{
	return EOK;
}
static int blockdev_bread(struct ext4_blockdev *bdev, void *buf,
						  uint64_t blk_id, uint32_t blk_cnt)
{
	panic_on(!buf, "no buf");
	size_t offset = blk_id * bdev->bdif->ph_bsize;
	int count = blk_cnt * bdev->bdif->ph_bsize;
	device_t *dev = (device_t *)bdev->bdif->p_user;
	dev->ops->read(dev, offset, buf, count);
	return EOK;
}
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
						   uint64_t blk_id, uint32_t blk_cnt)
{
	panic_on(!buf, "no buf");
	size_t offset = blk_id * bdev->bdif->ph_bsize;
	int count = blk_cnt * bdev->bdif->ph_bsize;
	device_t *dev = (device_t *)bdev->bdif->p_user;
	dev->ops->write(dev, offset, buf, count);
	return EOK;
}
static int blockdev_lock(struct ext4_blockdev *bdev) { return EOK; }
static int blockdev_unlock(struct ext4_blockdev *bdev) { return EOK; }

void vfs_init(void)
{
	device_t *sda = dev->lookup("sda");
	bi.open = blockdev_open;
	bi.close = blockdev_close;
	bi.bread = blockdev_bread;
	bi.bwrite = blockdev_bwrite;
	bi.lock = blockdev_lock;
	bi.unlock = blockdev_unlock;
	bi.ph_bsize = 4096;
	bi.ph_bbuf = block_buffer;
	bi.ph_refctr = 1;
	bi.p_user = sda;
	bi.ph_bcnt = ((sd_t *)sda->ptr)->blkcnt / bi.ph_bsize * ((sd_t *)sda->ptr)->blksz;
	bd.bdif = &bi;
	bd.part_size = bd.bdif->ph_bcnt * (uint64_t)bd.bdif->ph_bsize;
	vfs->mount("disk", "/", "ext4", 0, NULL);
	open_files[STDIN_FILENO].in_use = true;
	open_files[STDOUT_FILENO].in_use = true;
	open_files[STDERR_FILENO].in_use = true;
}

int vfs_mount(const char *dev_name, const char *mount_point,
			  const char *fs_type, int flags, void *data)
{
	int ret = ext4_device_register(&bd, dev_name);
	panic_on(ret != EOK, "Failed to register ext4 device");
	ret = ext4_mount(dev_name, mount_point, false);
	panic_on(ret != EOK, "Failed to mount ext4 filesystem");
	return VFS_SUCCESS;
}

int vfs_open(const char *pathname, int flags)
{
	int fd = -1;
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (!open_files[i].in_use)
		{
			fd = i;
			break;
		}
	}
	panic_on(fd == -1, "No free file descriptor available");
	const char *mode = "r";
	if (flags & O_RDONLY)
		mode = "r";
	else if (flags & O_WRONLY)
	{
		if (flags & O_APPEND)
			mode = "a";
		else
			mode = "w";
	}
	else if (flags & O_RDWR)
	{
		if (flags & O_APPEND)
			mode = "a+";
		else if (flags & O_TRUNC)
			mode = "w+";
		else
			mode = "r+";
	}
	open_files[fd].file = pmm->alloc(sizeof(ext4_file));
	int ret = ext4_fopen(open_files[fd].file, pathname, mode);
	if (ret != EOK)
	{
		pmm->free(open_files[fd].file);
		return VFS_ERROR;
	}
	open_files[fd].file->refctr = 1;
	open_files[fd].in_use = true;
	return fd;
}

int vfs_close(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
	{
		return VFS_ERROR;
	}

	int ret = ext4_fclose(open_files[fd].file);
	open_files[fd].in_use = false;
	open_files[fd].file->refctr--;
	if (open_files[fd].file->refctr == 0)
	{
		pmm->free(open_files[fd].file);
	}
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

ssize_t vfs_read(int fd, void *buf, size_t count)
{

	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use || !buf)
	{
		return VFS_ERROR;
	}
	if (open_files[fd].file == NULL)
	{
		device_t *tty = dev->lookup("tty1");
		// tty_t *tty1 = tty->ptr;
		// printf("VFS: Read from tty1\n");
		// while (tty1->cooked.value == 0);
		// printf("VFS: Read from tty1\n");
		return tty->ops->read(tty, 0, buf, count);
	}
	size_t bytes_read;
	int ret = ext4_fread(open_files[fd].file, buf, count, &bytes_read);
	if (ret != EOK)
	{
		return VFS_ERROR;
	}
	return (ssize_t)bytes_read;
}

ssize_t vfs_write(int fd, const void *buf, size_t count)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use || !buf)
	{
		return VFS_ERROR;
	}
	if (open_files[fd].file == NULL)
	{
		device_t *tty = dev->lookup("tty1");
		return tty->ops->write(tty, 0, buf, count);
	}
	size_t bytes_written;
	int ret = ext4_fwrite(open_files[fd].file, buf, count, &bytes_written);
	if (ret != EOK)
	{
		printf("VFS: Write failed: %d\n", ret);
		return VFS_ERROR;
	}

	return (ssize_t)bytes_written;
}

off_t vfs_seek(int fd, off_t offset, int whence)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
	{
		return VFS_ERROR;
	}

	int ret = ext4_fseek(open_files[fd].file, offset, whence);
	if (ret != EOK)
	{
		return VFS_ERROR;
	}

	return ext4_ftell(open_files[fd].file);
}

int vfs_umount(const char *mount_point)
{
	// 关闭所有打开的文件
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (open_files[i].in_use)
		{
			vfs->close(i);
		}
	}

	// 关闭所有打开的目录
	for (int i = 0; i < MAX_OPEN_DIRS; i++)
	{
		if (open_dirs[i].in_use)
		{
			vfs->closedir(i);
		}
	}

	int ret = ext4_umount(mount_point);
	if (ret != EOK)
	{
		printf("VFS: Failed to umount: %d\n", ret);
		return VFS_ERROR;
	}

	ext4_device_unregister("disk");
	printf("VFS: Successfully umounted filesystem\n");
	return VFS_SUCCESS;
}

int vfs_mkdir(const char *pathname)
{
	int ret = ext4_dir_mk(pathname);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_rmdir(const char *pathname)
{
	int ret = ext4_dir_rm(pathname);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_unlink(const char *pathname)
{
	int ret = ext4_fremove(pathname);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_rename(const char *oldpath, const char *newpath)
{
	int ret = ext4_frename(oldpath, newpath);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_opendir(const char *pathname)
{
	int dirfd = -1;
	for (int i = 0; i < MAX_OPEN_DIRS; i++)
	{
		if (!open_dirs[i].in_use)
		{
			dirfd = i;
			break;
		}
	}

	if (dirfd == -1)
		return VFS_ERROR;

	open_dirs[dirfd].dir = pmm->alloc(sizeof(ext4_dir));
	int ret = ext4_dir_open(open_dirs[dirfd].dir, pathname);
	if (ret != EOK)
	{
		pmm->free(open_dirs[dirfd].dir);
		return VFS_ERROR;
	}
	open_dirs[dirfd].dir->f.refctr = 1;
	open_dirs[dirfd].in_use = true;
	return dirfd;
}

int vfs_readdir(int fd, struct dirent *entry)
{
	if (fd < 0 || fd >= MAX_OPEN_DIRS || !open_dirs[fd].in_use || !entry)
	{
		return VFS_ERROR;
	}
	ext4_dir *dir = open_dirs[fd].dir;
	if (dir->de.name_length > strlen(entry->d_name))
	{
		return VFS_ERROR;
	}
	entry->d_ino = dir->de.inode;
	entry->d_off = dir->next_off;
	entry->d_reclen = dir->de.entry_length;
	entry->d_type = dir->de.inode_type;
	strncpy(entry->d_name, (const char *)dir->de.name, dir->de.name_length);
	entry->d_name[dir->de.name_length] = '\0';
	return VFS_SUCCESS;
}

int vfs_closedir(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_DIRS || !open_dirs[fd].in_use)
	{
		return VFS_ERROR;
	}

	int ret = ext4_dir_close(open_dirs[fd].dir);
	open_dirs[fd].in_use = false;
	open_dirs[fd].dir->f.refctr--;
	if (open_dirs[fd].dir->f.refctr == 0)
	{
		pmm->free(open_dirs[fd].dir);
	}
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_stat(int fd, struct kstat *stat)
{

	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use)
	{
		return VFS_ERROR;
	}
	ext4_file *file = open_files[fd].file;
	stat->st_size = file->fsize;
	return VFS_SUCCESS;
}

static const char *vfs_getdirpath(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_DIRS || !open_dirs[fd].in_use)
	{
		return NULL;
	}

	return (const char *)open_dirs[fd].dir->de.name;
}
static int vfs_link(const char *oldpath, const char *newpath)
{
	int ret = ext4_flink(oldpath, newpath);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}
static int vfs_dup(int oldfd)
{
	if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || !open_files[oldfd].in_use)
	{
		return VFS_ERROR;
	}
	int newfd = -1;
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (!open_files[i].in_use)
		{
			newfd = i;
			open_files[i].in_use = true;
			open_files[i].file = open_files[oldfd].file;
			open_files[i].file->refctr++;
			return newfd;
		}
	}
	return VFS_ERROR;
}
static int vfs_dup3(int oldfd, int newfd, int flags)
{
	if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || oldfd == newfd)
	{
		return VFS_ERROR;
	}
	if (open_files[newfd].in_use)
	{
		// TODO:change to correct implement
		if (flags & O_CLOEXEC)
		{
			return VFS_ERROR;
		}
		else
		{
			vfs_close(newfd);
		}
	}
	open_files[newfd].in_use = true;
	open_files[newfd].file = open_files[oldfd].file;
	open_files[newfd].file->refctr++;
	return newfd;
}
MODULE_DEF(vfs) = {
	.init = vfs_init,
	.mount = vfs_mount,
	.umount = vfs_umount,
	.open = vfs_open,
	.close = vfs_close,
	.read = vfs_read,
	.write = vfs_write,
	.seek = vfs_seek,
	.mkdir = vfs_mkdir,
	.rmdir = vfs_rmdir,
	.unlink = vfs_unlink,
	.link = vfs_link,
	.rename = vfs_rename,
	.opendir = vfs_opendir,
	.readdir = vfs_readdir,
	.closedir = vfs_closedir,
	.stat = vfs_stat,
	.getdirpath = vfs_getdirpath,
	.dup = vfs_dup,
	.dup3 = vfs_dup3,
};
