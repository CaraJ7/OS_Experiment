#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

// #define NEWFS_MAGIC        1214          /* TODO: Define by yourself */
// #define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */
/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
struct custom_options newfs_options;			 /* 全局选项 */
struct newfs_super super; 

/******************************************************************************
* SECTION: newfs_utils.c
*******************************************************************************/
char* 			   get_fname(const char* path);
int 			   calc_lvl(const char * path);
int 			   driver_read(int offset, uint8_t *out_content, int size);
int 			   driver_write(int offset, uint8_t *in_content, int size);


int 			   newfs_mount(struct custom_options options);
int 			   newfs_umount();

int 			   alloc_dentry(struct newfs_inode * inode, struct newfs_dentry * dentry);
int 			   alloc_datablk(int blk_num,struct newfs_inode* inode);
int 			   incre_inode_of_dentry(struct newfs_inode * inode);
// int 			   sfs_drop_dentry(struct sfs_inode * inode, struct sfs_dentry * dentry);
struct newfs_inode*  alloc_inode(struct newfs_dentry * dentry);
int 			   sync_inode(struct newfs_inode * inode);
// int 			   sfs_drop_inode(struct sfs_inode * inode);
struct newfs_inode*  read_inode(struct newfs_dentry * dentry, int ino);
struct newfs_dentry* get_dentry(struct newfs_inode * inode, int dir);

struct newfs_dentry*  lookup(const char * path, boolean * is_find, boolean* is_root);

/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);

#endif  /* _newfs_H_ */