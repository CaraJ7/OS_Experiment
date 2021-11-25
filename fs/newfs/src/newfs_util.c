#include "newfs.h"

/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * IO_SZ = BLK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int newfs_mount(struct custom_options options){
    int                 ret = 0;
    int                 driver_fd;
    struct newfs_super_d  newfs_super_d; 
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int                 inode_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    // int                 map_data_offset;
    
    int                 super_blks;
    boolean             is_init = 0;

    super.is_mounted = 0;

    // driver_fd = open(options.device, O_RDWR);
    // 打开设备，存到内存结构中
    driver_fd = ddriver_open(options.device);
    if (driver_fd < 0) {
        return driver_fd;
    }
    super.fd = driver_fd;

    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE,  &super.sz_disk);
    // ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &sfs_super.sz_io);
    
    root_dentry = new_dentry("/", NEWFS_DIR);

    // 读超级块
    if (driver_read(SUPER_OFFSET, (uint8_t *)(&newfs_super_d), 
                        sizeof(struct newfs_super_d)) != 0) {
        return -EIO;
    }   
    
    //super|inode map|data map|inode|data                                                  /* 读取super */
    if (newfs_super_d.magic_num != NEWFS_MAGIC) {     /* 幻数无 */
        // 直接规定大小                              /* 估算各部分大小 */
        super_blks = SUPERBLOCK_BLKSIZE;
        inode_num  =  INO_NUM;
        map_inode_blks = INODE_MAP_BLKSIZE; // 都是1
        map_data_blks = DATA_MAP_BLKSIZE;


        super.max_ino = inode_num; 
        newfs_super_d.max_ino = inode_num;
        // offset是基于Byte的大小
        newfs_super_d.map_inode_offset = BLKS_SZ(super_blks);
        newfs_super_d.map_inode_blks  = map_inode_blks;
        newfs_super_d.map_data_blks  = map_data_blks;

        newfs_super_d.map_data_offset = BLKS_SZ((super_blks+map_inode_blks));

        newfs_super_d.sz_usage    = 0;
        is_init = 1;
    }

    super.sz_usage   = newfs_super_d.sz_usage;    
    super.max_ino =   newfs_super_d.max_ino;
    
    /* 建立 in-memory 结构 ,预先分配空间*/
    super.map_inode = (uint8_t *)malloc(BLKS_SZ(newfs_super_d.map_inode_blks));
    super.map_inode_blks =   newfs_super_d.map_inode_blks;
    super.map_data_blks =   newfs_super_d.map_data_blks;
    // super.map_inode_offset = newfs_super_d.map_inode_offset;
    super.map_data = (uint8_t *)malloc(BLKS_SZ(newfs_super_d.map_data_blks));

    // sfs_super.data_offset = sfs_super_d.data_offset;
    
    // 把inode map读进来 
    if (driver_read(newfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                        BLKS_SZ(newfs_super_d.map_inode_blks)) != 0) {
        return -EIO;
    }
    // 把data map读进来 
    if (driver_read(newfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                        BLKS_SZ(newfs_super_d.map_data_blks)) != 0) {
        return -EIO;
    }
    printf("inode: %d\n",super.map_inode);
    printf("map: %d\n",super.map_data);
    // 初始情况，为根节点分配inode(root_dentry指向这个inode)
    if (is_init) {             
        // 新建一个inode,并且把在位图中登记，传入的dentry指向它                       
        root_inode = alloc_inode(root_dentry);
        sync_inode(root_inode);//把分配的inode写回磁盘
    }
    
    root_inode            = read_inode(root_dentry, ROOT_INO);
    root_dentry->inode    = root_inode;
    super.root_dentry = root_dentry;
    super.is_mounted  = 1;

    // dump_map();
    return ret;
}

/**
 * @brief 
 * 
 * @return int 
 */
int newfs_umount() {
    struct newfs_super_d  newfs_super_d; 

    if (!super.is_mounted) {
        return 0;
    }

    sync_inode(super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    newfs_super_d.magic_num           = NEWFS_MAGIC;
    newfs_super_d.map_inode_blks      = super.map_inode_blks;
    newfs_super_d.map_data_blks       = super.map_data_blks;
    newfs_super_d.map_data_offset    = BLKS_SZ((SUPERBLOCK_BLKSIZE+super.map_inode_blks));

    newfs_super_d.map_inode_offset     = BLKS_SZ(SUPERBLOCK_BLKSIZE);
    newfs_super_d.max_ino             = super.max_ino;
    newfs_super_d.sz_usage            = super.sz_usage;

    if (driver_write(SUPER_OFFSET, (uint8_t *)&newfs_super_d, 
                     sizeof(struct newfs_super_d)) != 0) {
        return -EIO;
    }

    if (driver_write(newfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                         BLKS_SZ(super.map_inode_blks)) != 0) {
        return -EIO;
    }

    if (driver_write(newfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                         BLKS_SZ(super.map_data_blks)) != 0) {
        return -EIO;
    }

    free(super.map_inode);
    free(super.map_data);
    ddriver_close(super.fd);

    return 0;
}

/**
 * @brief 驱动读，读成功返回0
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = ROUND_DOWN(offset, IO_SZ());
    int      bias           = offset - offset_aligned;
    // 总大小，向上取整
    int      size_aligned   = ROUND_UP((size + bias), IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(super.fd, offset_aligned, SEEK_SET);
    // 这里有点坑，读取完了以后，磁盘头是自动移动的，所以不需要重新seek（ddriver中可实验知）
    while (size_aligned != 0)
    {
        // read(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_read(super.fd, cur, IO_SZ());
        cur          += IO_SZ();
        size_aligned -= IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return 0;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = ROUND_DOWN(offset, IO_SZ());
    int      bias           = offset - offset_aligned;
    //  向上取整的总的大小
    int      size_aligned   = ROUND_UP((size + bias), IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // 先全给读了出来，然后再在里面去改对应的位置，因为有可能不是aligned的
    // 下面那一步的memcpy很妙
    driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(super.fd, offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_write(super.fd, cur, IO_SZ());
        cur          += IO_SZ();
        size_aligned -= IO_SZ();   
    }

    free(temp_content);
    return 0;
}

int alloc_datablk(int blk_num,struct newfs_inode* inode){
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int blk_cursor  = 0;
    boolean is_find_free_blk = 0;
    int max_blk = super.max_ino*6;

    for (byte_cursor = 0; byte_cursor < BLKS_SZ(super.map_data_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前datablk_cursor位置空闲 */
                super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_blk = 1;           
                break;
            }
            blk_cursor++;
        }
        if (is_find_free_blk) {
            break;
        }
    }

    // 没有空闲，或者超出了实际的inode范围
    if (!is_find_free_blk || blk_cursor == max_blk)
        return -ENOSPC;
    
    inode->block_pointer[blk_num] = blk_cursor;
    return 0;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return sfs_inode
 */
struct newfs_inode* alloc_inode(struct newfs_dentry * dentry) {
    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = 0;

    for (byte_cursor = 0; byte_cursor < BLKS_SZ(super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            if((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = 1;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    // 没有空闲，或者超出了实际的inode范围
    if (!is_find_free_entry || ino_cursor == super.max_ino)
        return -ENOSPC;

    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    // 判断是否是文件，如果是文件进行初始化。不一定用得上。 
    // if (SFS_IS_REG(inode)) {
    //     inode->data = (uint8_t *)malloc(SFS_BLKS_SZ(SFS_DATA_PER_FILE));
    // }

    return inode;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d  inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    memcpy(inode_d.block_pointer,inode->block_pointer,6*sizeof(int));

    // 先把inode拷贝回去
    if (driver_write(INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct newfs_inode_d)) != 0) {
        printf("[%s] io error\n", __func__);
        return -EIO;
    }
                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    int offset;
    int cur_datablk;
    // 如果是文件夹，他里面的dentrys可能是改变了，要重新一条条写回去
    if (IS_DIR(inode)) {                          
        dentry_cursor = inode->dentrys; // 链表的头
        offset        = 0; // 第几个dentruy
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.fname, dentry_cursor->fname, MAX_NAME_LEN);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            dentry_d.valid = 1;

            cur_datablk = FLOOR((offset+1),DENTRY_PER_DATABLK);
            printf("data offset %d\n",DATA_OFS((inode->block_pointer[cur_datablk]))+(offset-cur_datablk*DENTRY_PER_DATABLK) * sizeof(struct newfs_dentry_d));
            // 计算出实际的偏移，读取
            if (driver_write(DATA_OFS((inode->block_pointer[cur_datablk])) + 
                            (offset-cur_datablk*DENTRY_PER_DATABLK) * sizeof(struct newfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct newfs_dentry_d)) != 0) {
                printf("[%s] io error\n", __func__);
                return -EIO;                     
            }
            
            // 相当于对这个树上的每个节点都写进去，但是真的会读到那么深吗？
            // 存疑，注释一下试试还好不好使
            if (dentry_cursor->inode != NULL) {
                sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset ++;
        }
    }
    else if (IS_REG(inode)) {
        // 如果就是一个文件，他也没改不了什么东西，他的数据也没被读进来，所以上面做的就够了 

        // if (sfs_driver_write(SFS_DATA_OFS(ino), inode->data, 
        //                      SFS_BLKS_SZ(SFS_DATA_PER_FILE)) != SFS_ERROR_NONE) {
        //     SFS_DBG("[%s] io error\n", __func__);
        //     return -SFS_ERROR_IO;
        // }
    }
    return 0;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode。并且填充满inode结构体下面的东西。相当于把文件夹的数据都读了，文件的数据不读.
 * @param ino inode唯一编号
 * @return struct sfs_inode* 
 */
struct newfs_inode* read_inode(struct newfs_dentry * dentry, int ino) {
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry;
    struct newfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    if (driver_read(INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != 0) {
        printf("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    // inode对应的数据的位置
    memcpy(inode->block_pointer,inode_d.block_pointer,6*sizeof(int));
    int cur_datablk;
    // 如果是一个ino对应一个目录,就是把这个inode结构体下面的每一个东西都填满，然后再就不管了
    // 所以需要把dentry也读出来，组成一个链表
    if (IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        // 每次读一个dentry,然后在内存里面生成一个映像放在inode的dentrys(链表)里面
        for (i = 0; i < dir_cnt; i++)
        {
            //  确定当前属于的数据块
            cur_datablk = FLOOR((i+1),DENTRY_PER_DATABLK);
            // 计算出实际的偏移，读取
            if (driver_read(DATA_OFS((inode->block_pointer[cur_datablk])) + 
                            (i-cur_datablk*DENTRY_PER_DATABLK) * sizeof(struct newfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct newfs_dentry_d)) != 0) {
                printf("[%s] io error\n", __func__);
                return NULL;                    
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry; //这个东西emmm，它是指的这个dentry对应的inode是被哪个dentry指向的 parent->inode->sub_dentry
            sub_dentry->ino    = dentry_d.ino; 
            // 把新读进来的dentry插入到inode的dentrys里面,在inode里面对dir_cnt加
            alloc_dentry(inode, sub_dentry); 
        }
    }
    // 如果是一个ino对应一个文件,上面做的就够了
    else if (IS_REG(inode)) {
        // 如果是一个文件，只需要存对应的数组就可以了
        // memcpy(inode->block_pointer,inode_d.block_pointer,6*sizeof(int));

        // if (sfs_driver_read(SFS_DATA_OFS(ino), (uint8_t *)inode->data, 
        //                     SFS_BLKS_SZ(SFS_DATA_PER_FILE)) != SFS_ERROR_NONE) {
        //     printf("[%s] io error\n", __func__);
        //     return NULL;                    
        // }
    }
    return inode;
}

/**
 * @brief 在一个inode的dentrys列表中插入，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

int incre_inode_of_dentry(struct newfs_inode * inode){
    int originial_dircnt = inode->dir_cnt-1;
    inode->size+=sizeof(struct newfs_dentry_d);
    if((CEIL(inode->dir_cnt,DENTRY_PER_DATABLK))!=(CEIL(originial_dircnt,DENTRY_PER_DATABLK)))
    {
        alloc_datablk((CEIL(inode->dir_cnt,DENTRY_PER_DATABLK))-1,inode);
    }

    return inode->size;
    
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct sfs_inode* 
 */
struct newfs_dentry* lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry* dentry_cursor = super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode*  inode; 
    int   total_lvl = calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = 0;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = 1;
        *is_root = 1;
        dentry_ret = super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (IS_REG(inode) && lvl < total_lvl) {
            printf("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = 0;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = 1;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = 0;
                printf("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = 1;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct sfs_dentry* 
 */
struct newfs_dentry* get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}