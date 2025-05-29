#include <common.h>
#include <vfs.h>
#include <ext4.h>
// 简单的块设备接口实现 - 直接使用AM磁盘接口
static struct ext4_blockdev_iface bi;
static struct ext4_blockdev bd;
static uint8_t block_buffer[4096];
#define MAX_OPEN_FILES 256
static struct {
	bool in_use;
	ext4_file file;
} open_files[MAX_OPEN_FILES];

#define MAX_OPEN_DIRS 64
static struct {
	bool in_use;
	ext4_dir dir;
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
	int count =blk_cnt*bdev->bdif->ph_bsize;
	device_t* dev=(device_t*)bdev->bdif->p_user;
	dev->ops->read(dev,offset,buf,count);
	return EOK;
}
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
				uint64_t blk_id, uint32_t blk_cnt)
{
	panic_on(!buf, "no buf");
	size_t offset = blk_id * bdev->bdif->ph_bsize;
	int count = blk_cnt * bdev->bdif->ph_bsize;
	device_t* dev=(device_t*)bdev->bdif->p_user;
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
	bi.ph_refctr=1;
    bi.p_user = sda;
	bi.ph_bcnt = ((sd_t*)sda->ptr)->blkcnt/ bi.ph_bsize* ((sd_t*)sda->ptr)->blksz;
	bd.bdif = &bi;
	bd.part_size=bd.bdif->ph_bcnt*(uint64_t)bd.bdif->ph_bsize;
	vfs->mount("disk", "/", "ext4", 0, NULL);
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
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (!open_files[i].in_use) {
			fd = i;
			open_files[i].in_use = true;
			break;
		}
	}
    panic_on(fd == -1, "No free file descriptor available");
	const char *mode = "r";
	if (flags & VFS_O_WRONLY)
		mode = "w";
	else if (flags & VFS_O_RDWR)
		mode = "r+";
	if (flags & VFS_O_CREAT)
		mode = (flags & VFS_O_WRONLY) ? "w" : "w+";
	int ret = ext4_fopen(&open_files[fd].file, pathname, mode);
	if (ret != EOK) {
		printf("VFS: Failed to open %s: %d\n", pathname, ret);
		return VFS_ERROR;
	}
	return fd;
}

int vfs_close(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use) {
		return VFS_ERROR;
	}

	int ret = ext4_fclose(&open_files[fd].file);
	open_files[fd].in_use = false;
	printf("VFS: Closed fd=%d\n", fd);
	return (ret == EOK) ? VFS_SUCCESS : VFS_ERROR;
}

ssize_t vfs_read(int fd, void *buf, size_t count)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use || !buf) {
		return VFS_ERROR;
	}

	size_t bytes_read;
	int ret = ext4_fread(&open_files[fd].file, buf, count, &bytes_read);
	if (ret != EOK) {
		printf("VFS: Read failed: %d\n", ret);
		return VFS_ERROR;
	}

	return (ssize_t)bytes_read;
}

ssize_t vfs_write(int fd, const void *buf, size_t count)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use || !buf) {
		return VFS_ERROR;
	}

	size_t bytes_written;
	int ret = ext4_fwrite(&open_files[fd].file, buf, count, &bytes_written);
	if (ret != EOK) {
		printf("VFS: Write failed: %d\n", ret);
		return VFS_ERROR;
	}

	return (ssize_t)bytes_written;
}

off_t vfs_seek(int fd, off_t offset, int whence)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].in_use) {
		return VFS_ERROR;
	}

	int ret = ext4_fseek(&open_files[fd].file, offset, whence);
	if (ret != EOK) {
		return VFS_ERROR;
	}

	return ext4_ftell(&open_files[fd].file);
}

int vfs_umount(const char *mount_point)
{
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i].in_use) {
			ext4_fclose(&open_files[i].file);
			open_files[i].in_use = false;
		}
	}

	int ret = ext4_umount(mount_point);
	if (ret != EOK) {
		printf("VFS: Failed to umount: %d\n", ret);
		return VFS_ERROR;
	}

	ext4_device_unregister("disk");
	printf("VFS: Successfully umounted filesystem\n");
	return VFS_SUCCESS;
}

int vfs_register_filesystem(vfs_filesystem_type_t *fs_type)
{
	return VFS_SUCCESS;
}
int vfs_unregister_filesystem(const char *name) { return VFS_SUCCESS; }

int vfs_mkdir(const char *pathname, mode_t mode)
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
	for (int i = 0; i < MAX_OPEN_DIRS; i++) {
		if (!open_dirs[i].in_use) {
			dirfd = i;
			break;
		}
	}

	if (dirfd == -1)
		return VFS_ERROR;

	int ret = ext4_dir_open(&open_dirs[dirfd].dir, pathname);
	if (ret != EOK) {
		printf("VFS: Failed to open directory %s: %d\n", pathname, ret);
		return VFS_ERROR;
	}

	open_dirs[dirfd].in_use = true;
	printf("VFS: Opened directory %s (dirfd=%d)\n", pathname, dirfd);
	return dirfd;
}

int vfs_readdir(int fd, vfs_dentry_t *entry)
{
	if (fd < 0 || fd >= MAX_OPEN_DIRS || !open_dirs[fd].in_use || !entry) {
		return VFS_ERROR;
	}

	const ext4_direntry *direntry = ext4_dir_entry_next(&open_dirs[fd].dir);
	if (!direntry) {
		return VFS_ERROR; 
	}

	strncpy(entry->d_name, (const char *)direntry->name,
		direntry->name_length);
	entry->d_name[direntry->name_length] = '\0';


	if (!entry->d_inode) {
	
		entry->d_inode =
		    (struct vfs_inode *)pmm->alloc(sizeof(struct vfs_inode));
		if (entry->d_inode) {
			memset(entry->d_inode, 0, sizeof(struct vfs_inode));
		}
	}

	if (entry->d_inode) {
		entry->d_inode->i_ino = direntry->inode;
		switch (direntry->inode_type) {
		case EXT4_DE_REG_FILE:
			entry->d_inode->i_mode = VFS_S_IFREG;
			break;
		case EXT4_DE_DIR:
			entry->d_inode->i_mode = VFS_S_IFDIR;
			break;
		case EXT4_DE_CHRDEV:
			entry->d_inode->i_mode = VFS_S_IFCHR;
			break;
		case EXT4_DE_BLKDEV:
			entry->d_inode->i_mode = VFS_S_IFBLK;
			break;
		case EXT4_DE_FIFO:
			entry->d_inode->i_mode = VFS_S_IFIFO;
			break;
		case EXT4_DE_SOCK:
			entry->d_inode->i_mode = VFS_S_IFSOCK;
			break;
		case EXT4_DE_SYMLINK:
			entry->d_inode->i_mode = VFS_S_IFLNK;
			break;
		default:
			entry->d_inode->i_mode = 0;
			break;
		}
	}

	return VFS_SUCCESS;
}

int vfs_stat(const char *pathname, vfs_inode_t *stat)
{

	ext4_file file;
	int ret = ext4_fopen(&file, pathname, "r");
	if (ret != EOK) {
		printf("VFS: Failed to stat %s: %d\n", pathname, ret);
		return VFS_ERROR;
	}

	stat->i_size = ext4_fsize(&file);
	stat->i_ino = 0;   
	stat->i_mode = 0644; 
	stat->i_nlink = 1;
	stat->i_uid = 0;
	stat->i_gid = 0;
	stat->i_atime = 0; 
	stat->i_mtime = 0;
	stat->i_ctime = 0;

	ext4_dir dir;
	if (ext4_dir_open(&dir, pathname) == EOK) {
		stat->i_mode |= VFS_S_IFDIR;
		ext4_dir_close(&dir);
	} else {
		stat->i_mode |= VFS_S_IFREG;
	}
	ext4_fclose(&file);
	return VFS_SUCCESS;
}
MODULE_DEF(vfs) = {
    .init = vfs_init,
    .register_filesystem = vfs_register_filesystem,
    .unregister_filesystem = vfs_unregister_filesystem,
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
    .rename = vfs_rename,
    .opendir = vfs_opendir,
    .readdir = vfs_readdir,
    .stat = vfs_stat,
};
