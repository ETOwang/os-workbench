#include <common.h>
#include <vfs.h>
#include <ext4.h>
static struct file_table ftable;

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

static struct file *filedup(struct file *f)
{
	kmt->spin_lock(&ftable.lock);
	if (f->ref < 1)
		panic("filedup");
	f->ref++;
	kmt->spin_unlock(&ftable.lock);
	return f;
}

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
		st->st_ino = d->f.inode;
		st->st_size = d->f.fsize;
		st->st_nlink = 2; // Directories typically have a link count of at least 2 (., ..)
		return 0;
	}
	return -1;
}

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

int pipealloc(struct file **f0, struct file **f1)
{
	struct pipe *pi;

	pi = 0;
	*f0 = *f1 = 0;
	if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
		goto bad;
	if ((pi = (struct pipe *)pmm->alloc(4096)) == 0)
		goto bad;
	pi->readopen = 1;
	pi->writeopen = 1;
	pi->nwrite = 0;
	pi->nread = 0;
	kmt->spin_init(&pi->lock, "pipe");
	(*f0)->type = FD_PIPE;
	(*f0)->readable = 1;
	(*f0)->writable = 0;
	(*f0)->ptr = pi;
	(*f1)->type = FD_PIPE;
	(*f1)->readable = 0;
	(*f1)->writable = 1;
	(*f1)->ptr = pi;
	return 0;

bad:
	if (pi)
		pmm->free((void *)pi);
	if (*f0)
		fileclose(*f0);
	if (*f1)
		fileclose(*f1);
	return -1;
}

void pipeclose(struct pipe *pi, int writable)
{
	kmt->spin_lock(&pi->lock);
	if (writable)
	{
		pi->writeopen = 0;
		kmt->wakeup(&pi->nread);
	}
	else
	{
		pi->readopen = 0;
		kmt->wakeup(&pi->nwrite);
	}
	if (pi->readopen == 0 && pi->writeopen == 0)
	{
		kmt->spin_unlock(&pi->lock);
		pmm->free((void *)pi);
	}
	else
		kmt->spin_unlock(&pi->lock);
}

int pipewrite(struct pipe *pi, const void *buf, int n)
{
	int i = 0;
	kmt->spin_lock(&pi->lock);
	while (i < n)
	{
		if (pi->readopen == 0)
		{
			kmt->spin_unlock(&pi->lock);
			return -1;
		}
		if (pi->nwrite == pi->nread + PIPESIZE)
		{ // DOC: pipewrite-full
			kmt->wakeup(&pi->nread);
			kmt->sleep(&pi->nwrite, &pi->lock);
		}
		else
		{
			pi->data[pi->nwrite++ % PIPESIZE] = ((char *)buf)[i];
			i++;
		}
	}
	kmt->wakeup(&pi->nread);
	kmt->spin_unlock(&pi->lock);
	return i;
}

int piperead(struct pipe *pi, const void *buf, int n)
{
	int i;
	char ch;
	kmt->spin_lock(&pi->lock);
	while (pi->nread == pi->nwrite && pi->writeopen)
	{									   // DOC: pipe-empty
		kmt->sleep(&pi->nread, &pi->lock); // DOC: piperead-sleep
	}
	for (i = 0; i < n; i++)
	{ // DOC: piperead-copy
		if (pi->nread == pi->nwrite)
			break;
		ch = pi->data[pi->nread++ % PIPESIZE];
		((char *)buf)[i] = ch;
	}
	kmt->wakeup(&pi->nwrite); // DOC: piperead-wakeup
	kmt->spin_unlock(&pi->lock);
	return i;
}

int vfs_link(const char *oldpath, const char *newpath)
{
	for (size_t i = 0; i < NFILE; i++)
	{
		if (strcmp(ftable.file[i].path, oldpath) == 0)
		{
			ftable.file[i].ref++;
			break;
		}
	}
	int ret = ext4_flink(oldpath, newpath);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_unlink(const char *path)
{
	for (size_t i = 0; i < NFILE; i++)
	{
		if (strcmp(ftable.file[i].path, path) == 0)
		{
			ftable.file[i].ref--;
			break;
		}
	}
	int ret = ext4_fremove(path);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

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
	kmt->spin_init(&ftable.lock, "ftable");
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

int vfs_mkdir(const char *pathname)
{
	int ret = ext4_dir_mk(pathname);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
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
	if ((f = filealloc()) == NULL)
	{
		return NULL;
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
		fileclose(f);
		return NULL;
	}
	if (ext4_fopen(ef, pathname, mode) != EOK)
	{
		pmm->free(ef);
		d = pmm->alloc(sizeof(ext4_dir));
		if (!d)
		{
			fileclose(f);
			return NULL;
		}
		if (ext4_dir_open(d, pathname) != EOK)
		{
			pmm->free(d);
			fileclose(f);
			return NULL;
		}
		strcpy(f->path, pathname);
		f->type = FD_DIR;
		f->ptr = d;
		f->readable = !(flags & O_WRONLY);
		f->writable = (flags & O_WRONLY) || (flags & O_RDWR);
		f->off = 0;
		return f;
	}
	strcpy(f->path, pathname);
	f->ref = ef->refctr + 1;
	f->type = FD_FILE;
	f->ptr = ef;
	f->readable = !(flags & O_WRONLY);
	f->writable = (flags & O_WRONLY) || (flags & O_RDWR);
	f->off = 0;
	return f;
}

void vfs_close(struct file *f)
{
	switch (f->type)
	{
	case FD_DEVICE:
	case FD_DIR:
	case FD_FILE:
		fileclose(f);
		break;
	case FD_PIPE:
		f->ref--;
		if (f->ref == 0)
		{
			pipeclose((struct pipe *)f->ptr, f->writable);
		}
		break;
	default:
		panic("vfs close unknown type");
		break;
	}
}

ssize_t vfs_read(struct file *f, void *buf, size_t count)
{
	int nread;
	if (f->type == FD_FILE || f->type == FD_DIR || f->type == FD_DEVICE)
	{
		nread = fileread(f, buf, count);
	}
	else if (f->type == FD_PIPE)
	{
		nread = piperead((struct pipe *)f->ptr, buf, count);
	}
	else
	{
		panic("vfs read unknown type");
	}
	return nread;
}

ssize_t vfs_write(struct file *f, const void *buf, size_t count)
{
	int nwrite;
	if (f->type == FD_FILE || f->type == FD_DIR || f->type == FD_DEVICE)
	{
		nwrite = filewrite(f, buf, count);
	}
	else if (f->type == FD_PIPE)
	{
		nwrite = pipewrite((struct pipe *)f->ptr, buf, count);
	}
	else
	{
		printf("vfs type %d\n", f->type);
		panic("vfs write unknown type");
	}
	return nwrite;
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
	return filestat(f, stat);
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

int vfs_rmdir(const char *pathname)
{
	int ret = ext4_dir_rm(pathname);
	ext4_flink(pathname, pathname);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

int vfs_rename(const char *oldpath, const char *newpath)
{
	for (size_t i = 0; i < NFILE; i++)
	{
		if (strcmp(ftable.file[i].path, oldpath) == 0)
		{
			strcpy(ftable.file[i].path, newpath);
			break;
		}
	}
	int ret = ext4_frename(oldpath, newpath);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

static struct file *vfs_dup(struct file *f)
{
	return filedup(f);
}

static int vfs_pipe(struct file *fd[2])
{
	struct file *f0, *f1;
	if (pipealloc(&f0, &f1) < 0)
	{
		return -1;
	}
	fd[0] = f0;
	fd[1] = f1;
	return 0;
}
MODULE_DEF(vfs) = {
	.init = vfs_init,
	.dup = vfs_dup,
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
	.alloc = filealloc,
	.pipe = vfs_pipe};
