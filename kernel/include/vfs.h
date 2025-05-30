#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
// Type definitions
typedef uint32_t mode_t;
typedef long off_t;
typedef long ssize_t;

// 文件系统统计信息结构
typedef struct fs_stats {
	uint64_t f_blocks;  // 总块数
	uint64_t f_bfree;   // 空闲块数
	uint64_t f_bavail;  // 可用块数
	uint64_t f_files;   // 总inode数
	uint64_t f_ffree;   // 空闲inode数
	uint32_t f_bsize;   // 块大小
	uint32_t f_frsize;  // 片段大小
	uint32_t f_fsid;    // 文件系统ID
	uint32_t f_namelen; // 最大文件名长度
} fs_stats_t;

// VFS 错误码
#define VFS_SUCCESS 0
#define VFS_ERROR -1
#define VFS_ERROR_NOT_FOUND -2	      // 文件不存在
#define VFS_ERROR_ACCESS_DENIED -3    // 权限错误
#define VFS_ERROR_INVALID_ARGUMENT -4 // 无效参数
#define VFS_ERROR_NO_MEMORY -5	      // 内存不足
#define VFS_ERROR_NO_SPACE -6	      // 磁盘空间不足
#define VFS_ERROR_IO -7		      // I/O错误
#define VFS_ERROR_ALREADY_EXISTS -8   // 文件已存在
#define VFS_ERROR_NOT_DIRECTORY -9    // 不是目录
#define VFS_ERROR_IS_DIRECTORY -10    // 是目录
#define VFS_ERROR_READ_ONLY -11	      // 只读文件系统
#define VFS_ERROR_NOT_IMPLEMENTED -12 // 功能未实现
#define VFS_ERROR_UNKNOWN -13	      // 未知错误

// 向后兼容的错误码
#define VFS_ENOENT VFS_ERROR_NOT_FOUND
#define VFS_EACCES VFS_ERROR_ACCESS_DENIED
#define VFS_EINVAL VFS_ERROR_INVALID_ARGUMENT
#define VFS_ENOMEM VFS_ERROR_NO_MEMORY
#define VFS_ENOSPC VFS_ERROR_NO_SPACE
#define VFS_EIO VFS_ERROR_IO

// 高级VFS操作的错误码常量 (for lwext4 integration)
#define VFS_ERR_SUCCESS 0	 // 成功
#define VFS_ERR_INVALID_PARAM -4 // 无效参数 (映射到 VFS_ERROR_INVALID_ARGUMENT)
#define VFS_ERR_NOT_READY -14	 // 设备未就绪

// 文件类型
#define VFS_TYPE_REG 1 // 普通文件
#define VFS_TYPE_DIR 2 // 目录
#define VFS_TYPE_LNK 3 // 符号链接
#define VFS_TYPE_CHR 4 // 字符设备
#define VFS_TYPE_BLK 5 // 块设备

// 目录项类型
#define VFS_DT_UNKNOWN 0
#define VFS_DT_FIFO 1
#define VFS_DT_CHR 2
#define VFS_DT_DIR 4
#define VFS_DT_BLK 6
#define VFS_DT_REG 8
#define VFS_DT_LNK 10
#define VFS_DT_SOCK 12

// 文件模式位
#define VFS_S_IFMT 0170000   // 文件类型掩码
#define VFS_S_IFREG 0100000  // 普通文件
#define VFS_S_IFDIR 0040000  // 目录
#define VFS_S_IFLNK 0120000  // 符号链接
#define VFS_S_IFCHR 0020000  // 字符设备
#define VFS_S_IFBLK 0060000  // 块设备
#define VFS_S_IFIFO 0010000  // FIFO
#define VFS_S_IFSOCK 0140000 // 套接字

// 打开标志
#define VFS_O_RDONLY 0x00000000
#define VFS_O_WRONLY 0x00000001
#define VFS_O_RDWR 0x00000002
#define VFS_O_CREAT 0x00000040
#define VFS_O_EXCL 0x00000080
#define VFS_O_TRUNC 0x00000200
#define VFS_O_APPEND 0x00000400

// 前向声明
struct vfs_file;
struct vfs_inode;
struct vfs_super_block;
struct vfs_dentry;

// 文件操作结构
struct vfs_file_operations {
	int (*open)(struct vfs_inode *inode, struct vfs_file *file);
	int (*close)(struct vfs_file *file);
	ssize_t (*read)(struct vfs_file *file, char *buf, size_t count,
			off_t *pos);
	ssize_t (*write)(struct vfs_file *file, const char *buf, size_t count,
			 off_t *pos);
	off_t (*seek)(struct vfs_file *file, off_t offset, int whence);
	int (*ioctl)(struct vfs_file *file, unsigned int cmd,
		     unsigned long arg);
};

// inode操作结构
struct vfs_inode_operations {
	int (*lookup)(struct vfs_inode *dir, struct vfs_dentry *dentry);
	int (*create)(struct vfs_inode *dir, struct vfs_dentry *dentry,
		      int mode);
	int (*mkdir)(struct vfs_inode *dir, struct vfs_dentry *dentry,
		     int mode);
	int (*rmdir)(struct vfs_inode *dir, struct vfs_dentry *dentry);
	int (*unlink)(struct vfs_inode *dir, struct vfs_dentry *dentry);
	int (*rename)(struct vfs_inode *old_dir, struct vfs_dentry *old_dentry,
		      struct vfs_inode *new_dir, struct vfs_dentry *new_dentry);
};

// 超级块操作结构
struct vfs_super_operations {
	int (*mount)(const char *dev_name, const char *mount_point);
	int (*umount)(const char *mount_point);
	int (*sync)(struct vfs_super_block *sb);
	struct vfs_inode *(*alloc_inode)(struct vfs_super_block *sb);
	void (*destroy_inode)(struct vfs_inode *inode);
};

// inode结构
struct vfs_inode {
	uint32_t i_ino;	   // inode号
	uint32_t i_mode;   // 文件模式
	uint32_t i_uid;	   // 用户ID
	uint32_t i_gid;	   // 组ID
	uint64_t i_size;   // 文件大小
	uint32_t i_blocks; // 块数
	uint32_t i_nlink;  // 硬链接数
	uint64_t i_atime;  // 访问时间
	uint64_t i_mtime;  // 修改时间
	uint64_t i_ctime;  // 创建时间

	struct vfs_super_block *i_sb;	   // 超级块
	struct vfs_inode_operations *i_op; // inode操作
	struct vfs_file_operations *i_fop; // 文件操作
	void *i_private;		   // 文件系统私有数据
};

// 文件结构
struct vfs_file {
	struct vfs_inode *f_inode;	  // 关联的inode
	struct vfs_file_operations *f_op; // 文件操作
	uint32_t f_flags;		  // 打开标志
	off_t f_pos;			  // 文件位置
	void *f_private;		  // 文件系统私有数据
};

// 目录项结构
struct vfs_dentry {
	char d_name[256];	      // 文件名
	struct vfs_inode *d_inode;    // 关联的inode
	struct vfs_dentry *d_parent;  // 父目录项
	struct vfs_super_block *d_sb; // 超级块
};

// 超级块结构
struct vfs_super_block {
	uint32_t s_blocksize;	// 块大小
	uint64_t s_blocks;	// 总块数
	uint64_t s_free_blocks; // 空闲块数
	uint32_t s_inodes;	// 总inode数
	uint32_t s_free_inodes; // 空闲inode数
	uint32_t s_magic;	// 魔数

	struct vfs_super_operations *s_op; // 超级块操作
	struct vfs_inode *s_root;	   // 根inode
	void *s_fs_info;		   // 文件系统私有信息

	const char *s_type;	   // 文件系统类型
	const char *s_dev_name;	   // 设备名
	const char *s_mount_point; // 挂载点
};

// 文件系统类型结构
struct vfs_file_system_type {
	const char *name; // 文件系统名称
	struct vfs_super_block *(*mount)(const char *dev_name,
					 const char *mount_point);
	int (*umount)(struct vfs_super_block *sb);
	struct vfs_file_system_type *next; // 链表指针
};

#endif // VFS_H
