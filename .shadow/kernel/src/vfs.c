#include <common.h>
#include <vfs.h>
#include <ext4.h>
#include <file.h>

// Simple block device interface implementation - uses AM disk interface directly
static struct ext4_blockdev_iface bi;
static struct ext4_blockdev bd;
static uint8_t block_buffer[4096];

static int blockdev_open(struct ext4_blockdev *bdev) { return EOK; }
static int blockdev_close(struct ext4_blockdev *bdev) { return EOK; }

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
	panic_on(!buf, "no buf");
	size_t offset = blk_id * bdev->bdif->ph_bsize;
	int count = blk_cnt * bdev->bdif->ph_bsize;
	device_t *dev = (device_t *)bdev->bdif->p_user;
	dev->ops->read(dev, offset, buf, count);
	return EOK;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
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
}

int vfs_mount(const char *dev_name, const char *mount_point, const char *fs_type, int flags, void *data)
{
	int ret = ext4_device_register(&bd, dev_name);
	panic_on(ret != EOK, "Failed to register ext4 device");
	ret = ext4_mount(dev_name, mount_point, false);
	panic_on(ret != EOK, "Failed to mount ext4 filesystem");
	return VFS_SUCCESS;
}

struct file *vfs_open(const char *pathname, int flags)
{
	struct file *f;
	ext4_file *ef;
	ext4_dir *d;

	if ((f = file->alloc()) == NULL)
	{
		return NULL;
	}

	if (flags & O_DIRECTORY)
	{
		d = pmm->alloc(sizeof(ext4_dir));
		if (!d)
		{
			file->close(f);
			return NULL;
		}
		if (ext4_dir_open(d, pathname) != EOK)
		{
			pmm->free(d);
			file->close(f);
			return NULL;
		}
		f->type = FD_DIR;
		f->ptr = d;
		f->readable = 1;
		f->writable = 0;
		f->off = 0;
		return f;
	}

	const char *mode = "r";
	if (flags & O_WRONLY)
	{
		mode = (flags & O_APPEND) ? "a" : "w";
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

	ef = pmm->alloc(sizeof(ext4_file));
	if (!ef)
	{
		file->close(f);
		return NULL;
	}

	if (ext4_fopen(ef, pathname, mode) != EOK)
	{
		pmm->free(ef);
		file->close(f);
		return NULL;
	}

	f->type = FD_FILE;
	f->ptr = ef;
	f->readable = !(flags & O_WRONLY);
	f->writable = (flags & O_WRONLY) || (flags & O_RDWR);
	f->off = 0;
	return f;
}

void vfs_close(struct file *f)
{
	file->close(f);
}

ssize_t vfs_read(struct file *f, void *buf, size_t count)
{
	return file->read(f, buf, count);
}

ssize_t vfs_write(struct file *f, const void *buf, size_t count)
{
	return file->write(f, buf, count);
}

off_t vfs_seek(struct file *f, off_t offset, int whence)
{
	if (f->type != FD_FILE)
	{
		return VFS_ERROR;
	}
	ext4_file *ef = (ext4_file *)f->ptr;
	if (ext4_fseek(ef, offset, whence) != EOK)
	{
		return VFS_ERROR;
	}
	return ext4_ftell(ef);
}

int vfs_stat(struct file *f, struct stat *stat)
{
	return file->stat(f, stat);
}

int vfs_umount(const char *mount_point)
{
	// WARNING: This is not safe. It does not check for open files.
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

static int vfs_link(const char *oldpath, const char *newpath)
{
	int ret = ext4_flink(oldpath, newpath);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
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
	.stat = vfs_stat,
};
