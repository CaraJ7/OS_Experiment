#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128 
#define NEWFS_MAGIC        1214          /* TODO: Define by yourself */
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */  
// meta-data块的大小
#define SUPERBLOCK_BLKSIZE 1
#define INODE_MAP_BLKSIZE 1
#define DATA_MAP_BLKSIZE 1
#define INODE_BLKSIZE 1
#define INO_NUM 512
//
#define ROOT_INO 0
#define SUPER_OFFSET 0
#define DATABLK_BLKSIZE 2 // EXT2的数据块是2个512B

/******************************************************************************
* SECTION: Type def
*******************************************************************************/

typedef enum file_type {
    NEWFS_FILE,
    NEWFS_DIR
} FILE_TYPE;

typedef int boolean;
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define IO_SZ()                     512
#define SFS_DISK_SZ()                   (sfs_super.sz_disk)
#define SFS_DRIVER()                    (sfs_super.driver_fd)

#define ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)

#define BLKS_SZ(blks)               (blks * IO_SZ())
#define ASSIGN_FNAME(psfs_dentry, _fname)\ 
                                        memcpy(psfs_dentry->fname, _fname, strlen(_fname))
#define INO_OFS(ino)                (BLKS_SZ((super.map_inode_blks+super.map_data_blks+SUPERBLOCK_BLKSIZE))\
                                     + ino * BLKS_SZ(INODE_BLKSIZE))
#define DATA_OFS(data_blknum)               (INO_OFS(INO_NUM) + DATABLK_BLKSIZE*IO_SZ()*data_blknum)

#define IS_DIR(pinode)              (pinode->dentry->ftype == NEWFS_DIR)
#define IS_REG(pinode)              (pinode->dentry->ftype == NEWFS_FILE)

#define CEIL(value,round)            (value % round == 0 ? (int)(value/round) : ((int)(value / round) + 1))
#define FLOOR(value,round)            (int)(value/round) 

#define DENTRY_PER_DATABLK           ((int)(IO_SZ()*DATABLK_BLKSIZE/(sizeof(struct newfs_dentry_d))))
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct newfs_dentry;
struct newfs_inode;
struct newfs_super;


struct newfs_inode
{
    int                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    int                dir_cnt;
    struct newfs_dentry* dentry;                        /* 指向该inode的dentry */
    struct newfs_dentry* dentrys;                       /* 所有目录项 */
    int                 block_pointer[6];             // 数据块指针          
};  

struct newfs_dentry
{
    char               fname[MAX_NAME_LEN];
    struct newfs_dentry* parent;                        /* 父亲Inode的dentry */
    struct newfs_dentry* brother;                       /* 兄弟 */
    int                ino;
    struct newfs_inode*  inode;                         /* 指向inode */
    FILE_TYPE          ftype;
};

struct newfs_super
{
    uint32_t magic;
    int      fd;

    int                sz_disk;
    int                sz_usage;
   
    int                max_ino; // 最多支持的文件数

    uint8_t*           map_inode;       // inode位图位置
    uint8_t*           map_data;        // data位图位置

    int                map_inode_blks;              // inode位图占用的块数
    int                map_data_blks;               // data位图占用的块数

    boolean            is_mounted;

    struct newfs_dentry* root_dentry; // 这个dentry指向根目录的inode
};

static inline struct newfs_dentry* new_dentry(char * fname, FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;                                            
}
/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/
struct newfs_super_d
{
    // int                sz_usage;
    uint32_t           magic_num;                   // 幻数

    int                sz_usage;

    int                max_ino;                     // 最多支持的文件数

    int                map_inode_blks;              // inode位图占用的块数
    int                map_inode_offset;            // inode位图在磁盘上的偏移

    int                map_data_blks;               // data位图占用的块数
    int                map_data_offset;             // data位图在磁盘上的偏移
};

struct newfs_inode_d
{
    int                ino;                // 在inode位图中的下标
    int                size;               // 文件已占用空间
    // int                link;               // 链接
    FILE_TYPE          ftype;              // 文件类型（目录类型、普通文件类型）
    int                dir_cnt;            // 如果是目录类型文件，下面有几个目录项
    int                block_pointer[6];   // 数据块指针（可固定分配）
};  

struct newfs_dentry_d
{
    char               fname[MAX_NAME_LEN];
    FILE_TYPE          ftype;                         // 指向的ino文件类型
    int                ino;                           // 指向的ino号
    int                valid;                         // 该目录项是否有效
};  



struct custom_options {
	const char*        device;
};

// struct newfs_super {
//     uint32_t magic;
//     int      fd;
//     /* TODO: Define yourself */
// };

// struct newfs_inode {
//     uint32_t ino;
//     /* TODO: Define yourself */
// };

// struct newfs_dentry {
//     char     name[MAX_NAME_LEN];
//     uint32_t ino;
//     /* TODO: Define yourself */
// };

#endif /* _TYPES_H_ */