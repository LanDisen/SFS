#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h> // 用户态文件系统fuse
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "sfs_ds.h"    // SFS文件系统相关数据结构
#include "sfs_rw.h"    // SFS文件系统相关读写操作
#include "sfs_utils.h" // SFS文件系统相关辅助函数


// **************************************************************************************
// 以下是辅助函数

// 根据inode号获取目录（包括子目录和文件）
int read_dir(struct inode* inode, struct dir* dir) {
    printf("[read_dir] ino=%d\n", inode->st_ino);
    dir->num_entries = 0;
    int inode_size = inode->st_size;
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, inode);
    struct data_block* data_block = (struct data_block*)malloc(sizeof(struct data_block));
    while (has_next(iter) && inode_size > 0) {
        next(iter, data_block);
        int read_size = BLOCK_SIZE;
        int k = 0; // 下一个待读取的entry下标（包括UNUSED类型的entry）
        while (read_size > 0 && inode_size > 0) {
            struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
            memcpy(entry, data_block->data + k*sizeof(struct entry), sizeof(struct entry));
            k++;
            if (entry->type != UNUSED) {
                // 读取到的entry是已使用的才会加入dir（被删除的entry会被设置成UNUSED类型）
                dir->entries[dir->num_entries++] = entry;
                inode_size -= sizeof(struct entry);
            }
            read_size -= sizeof(struct entry);
            // free(entry);
            // entry = NULL;
        }
    }
    free(data_block);
    free(iter);
    data_block = NULL;
    iter = NULL;
    return 0;
}

/**
 * 根据路径获取entry（目录文件或普通文件）
 * @return: 返回0则找到entry指针，-1则未找到
 * @example:
 * path: /abc/ef
 *     (1) find /'s inode
 *     (2) find abc's entry
 *     (3) find abc's inode
 *     (4) find ef's entry
*/
int find_entry(const char* path, struct entry* entry) {
    printf("[find_entry] path=%s\n", path);
    if (path == NULL || strcmp(path, "") == 0) {
        printf("[find_entry] Error: the entry path should not be NULL or empty\n");
        return -1;
    }
    if (strcmp(path, "/") == 0) {
        // 根目录
        *entry = *root_entry;
        return 0;
    }
    char* path_copy = (char*)malloc(sizeof(path));
    strcpy(path_copy, path);
    strcpy(path_copy, path + 1);
    
    struct inode* cur_inode = (struct inode*)malloc(sizeof(struct inode));
    struct dir* cur_dir = (struct dir*)malloc(sizeof(struct dir));
    struct entry* cur_entry = (struct entry*)malloc(sizeof(struct entry));
    *cur_entry = *root_entry;
    // 路径解析
    char* cur_path = (char*)malloc(sizeof(path)); // 开始为根目录"/"
    char* head = (char*)malloc(sizeof(path)); // 分割路径的前部分（待匹配的子目录）
    char* tail = (char*)malloc(sizeof(path)); // 分割路径的后部分
    split_path(path_copy, head, tail);
    strcpy(cur_path, "/");
    int flag = 0; // 匹配成功标志
    int ret;
    char name[MAX_FILE_NAME + MAX_FILE_EXTENSION + 1];
    while (1) {
        read_inode(cur_entry->inode, cur_inode);
        read_dir(cur_inode, cur_dir);
        for (int i=0; i<cur_dir->num_entries; i++) {
            full_name(cur_dir->entries[i]->name, cur_dir->entries[i]->extension, name);
            if (strcmp(name, head) == 0) {
                // 子目录或文件路径匹配成功
                *cur_entry = *(cur_dir->entries[i]);
                flag = 1;
                break;
            }
        }
        if (flag) {
            // 路径匹配成功
            flag = 0;
            if (strcmp(tail, "") == 0) {
                *entry = *cur_entry;
                ret = 0;
                break;
            }
            strcpy(path_copy, tail);
            split_path(path_copy, head, tail);
            continue;
        } else {
            // 路径匹配失败
            entry = NULL;
            ret = -1;
            break;
        }
    }
    free(path_copy);
    free(head);
    free(tail);
    free(cur_entry);
    free(cur_inode);
    free(cur_dir);
    return ret;
}

/**
 * 在父目录下添加新的子entry（目录或文件）
 * @param parent_inode 父目录的inode指针
 * @param entry        待添加的entry指针
*/
void add_entry(struct inode* parent_inode, struct entry* entry) {
    char name[MAX_FILE_NAME + 1 + MAX_FILE_EXTENSION];
    full_name(entry->name, entry->extension, name);
    if (name[0] == '.') {
        return; // 隐藏文件
    }
    printf("[add_entry] entry name=%s\n", name);
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, parent_inode);
    struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
    while (has_next(iter)) {
        next(iter, datablock);
    }

    // 此时datablock是parent_inode的最后一个数据块
    off_t used_size = parent_inode->st_size % BLOCK_SIZE;
    if (used_size == 0) {
        printf("[add_entry] data block is full\n");
        short int* datablock_no = (short int*)malloc(sizeof(short int));
        alloc_datablock(parent_inode, datablock_no); // 分配一个未使用的数据块
        read_data_block(*datablock_no, datablock);
        memcpy(datablock, entry, sizeof(struct entry));
        // 写回磁盘
        write_data_block(*datablock_no, datablock);
        set_datablock_bitmap_used(*datablock_no);
        free(datablock_no);
        datablock_no = NULL;
    } else {
        memcpy(datablock->data + used_size, entry, sizeof(struct entry));
        // 写回磁盘
        write_data_block(iter->datablock_no, datablock);
    }
    parent_inode->st_size += sizeof(struct entry); // 更新inode大小
    write_inode(parent_inode->st_ino, parent_inode);
    free(iter);
    free(datablock);
    iter = NULL;
    datablock = NULL;
}

/**
 * 在父目录下删除entry（目录或文件）
 * @param parent_inode 父目录的inode指针
 * @param entry        待删除的entry指针
*/
int remove_entry(struct inode* parent_inode, struct entry* entry) {
    // 这里暂时保证待删除的参数entry一定存在
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, parent_inode);
    
    struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
    int inode_size = parent_inode->st_size;
    struct entry* e = (struct entry*)malloc(sizeof(struct entry));
    // 遍历parent_inode数据块
    while (has_next(iter) && inode_size > 0) {
        next(iter, datablock);
        // 匹配待删除的entry
        int read_size = BLOCK_SIZE; 
        int k = 0; // 从数据块中下一个待读取第k个entry（包括UNUSED类型的entry）
        while (read_size > 0 && inode_size > 0) {
            memcpy(e, datablock->data + k*sizeof(struct entry), sizeof(struct entry));
            read_size -= sizeof(struct entry);
            if (e->type == UNUSED) {
                k++;
                continue;
            }
            inode_size -= sizeof(struct entry);
            if (strcmp(entry->name, e->name) == 0 && strcmp(entry->extension, e->extension) == 0) {
                // 匹配成功，进行删除
                if (e->type == DIR_TYPE) {
                    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
                    read_inode(e->inode, inode);
                    struct dir* dir = (struct dir*)malloc(sizeof(struct dir));
                    read_dir(inode, dir);
                    for (int i=0; i<dir->num_entries; i++) {
                        remove_entry(inode, dir->entries[i]);
                    }
                }
                e->type = UNUSED;
                // 写回数据块
                memcpy(datablock->data + k*sizeof(struct entry), e, sizeof(struct entry));
                write_data_block(iter->datablock_no, datablock);
                // 如果数据块内没有可用entry则需要释放
                if (!datablock_has_entry(iter->datablock_no)) {
                    // 数据块无可用entry，进行释放（设置bitmap）
                    set_free_datablock_bitmap(iter->datablock_no);
                }
                // 释放inode
                set_free_inode_bitmap(e->inode);

                // 更新inode大小
                parent_inode->st_size -= sizeof(struct entry);
                // 写回磁盘
                write_inode(parent_inode->st_ino, parent_inode);
                return 0;
            }
            k++;
        }
        // 该数据块无待删除entry，继续读取下一个数据块
    }
    free(iter);
    free(e);
    free(datablock);
    iter = NULL;
    e = NULL;
    datablock = NULL;
    return 0;
}


// ************************************************************************************
// 以下为fuse_operations需要实现的SFS回调函数

/*
 * 基于inode组织磁盘：
 * | super block | inode bitmap | data bitmap | inode area | data area |
 * 一个数据块可以存放 512/64 = 8 个inode
 * 最大文件大小为：(4*512 + 8*512 + 8*8*512 + 8*8*8*512) Byte = 301056 Byte = 294 kB
 * 
 * 初始化文件系统：
 * 1. 打开文件系统的载体文件（sfs.img）
 * 2. 检查文件系统是否已经被初始化，如果已经初始化，可以跳过初始化步骤
 * 3. 如果文件系统尚未初始化，进行初始化操作，包括超级块的填充以及inode位图和数据块位图的初始化
*/
static void* SFS_init(struct fuse_conn_info* conn, struct fuse_config *cfg) {
    // 8M大小的磁盘文件映像路径，该文件作为SFS文件系统的载体
    fs = fopen(fs_img, "wb+");
    if (fs == NULL) {
        // 检查映像文件路径
        perror("[SFS_init] Error: failed to open the file system image");
        printf("[SFS_init] Error: the file system image's path: %s\n", fs_img);
        return NULL;
    }

    // 检查文件系统是否已经初始化，可以通过检查超级块的fs_size来实现
    sb = malloc(sizeof(struct sb));
    fseek(fs, 0, SEEK_SET);              // 定位超级块位置
    fread(sb, sizeof(struct sb), 1, fs); // 读取超级块数据

    // 初始化根目录属性
    root_entry = (struct entry*)malloc(sizeof(struct entry));
    strcpy(root_entry->name, "/");     // 根目录为"/"
    strcpy(root_entry->extension, ""); // 目录扩展名为空字符串
    root_entry->type = DIR_TYPE;       // 目录文件类型
    root_entry->inode = 0;             // 根目录的inode号为0
    work_entry = root_entry;           // 当前工作目录为根目录

    if (sb->fs_size > 0) {
        // 文件系统已初始化，无需再次初始化虚拟磁盘文件sfs.img
        printf("[SFS_init] SFS has been initialized\n");
        // 读取根目录entry
        struct data_block* root_datablock = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(0, root_datablock);
        memcpy(root_entry, root_datablock, sizeof(struct entry));
        work_entry = root_entry;
        free(root_datablock);
        root_datablock = NULL;
    } else {
        // 进行虚拟磁盘初始化
        printf("[SFS_init] Start initializing SFS\n");
        // 文件系统虚拟磁盘尚未初始化，进行初始化操作（格式化）
        // 首先填充超级块（超级块位于第0块）
        sb->fs_size = FS_SIZE / BLOCK_SIZE; // 文件系统大小，以块为单位，共16*1024块
        sb->first_blk_of_inodebitmap = 1; // inode位图的第一块块号
        sb->inodebitmap_size = NUM_INODE_BITMAP_BLOCK; // inode位图大小为1块（512B）
        sb->first_blk_of_databitmap = sb->first_blk_of_inodebitmap + sb->inodebitmap_size; // 数据块位图的第一块块号（第2块）
        sb->databitmap_size = NUM_DATA_BITMAP_BLOCK; // 数据块位图大小为4块（4 * 512 = 2048 Byte）
        sb->first_inode = sb->first_blk_of_databitmap + sb->databitmap_size; // inode区的第一块块号（第6块）
        sb->inode_area_size = sb->inodebitmap_size * BLOCK_SIZE * 8; // inode区大小为512*8块，该文件系统最多有4k个文件
        sb->first_blk = sb->first_inode + sb->inode_area_size; // 数据区的第一块块号（6 + 4096 = 4102）
        sb->datasize = sb->databitmap_size * BLOCK_SIZE * 8; // 数据区大小为4*512*8块

        // 将超级块数据写到到文件系统载体文件
        fseek(fs, 0, SEEK_SET);
        fwrite(sb, sizeof(struct sb), 1, fs);

        // 将根目录的相关信息填写到inode区的第一个inode
        struct inode* root_inode = (struct inode*)malloc(sizeof(struct inode));
        root_inode->st_mode  = __S_IFDIR | 0755; // 目录文件
        // root_inode->st_mode = 0755;
        root_inode->st_ino   = 0; // 根目录的inode号为0（第一个）
        root_inode->st_nlink = 2; // 链接引用数（根目录为2，其它目录为1）
        root_inode->st_uid   = 0; // getuid(); // 拥有者的用户ID，0为超级用户
        root_inode->st_gid   = 0; // getgid(); // 拥有者的组ID，0为超级用户组
        root_inode->st_size  = 0; // 初始大小为空
        // root_inode->st_atim  = 0;

        // 初始化根目录inode
        write_inode(0, root_inode); 
        set_inode_bitmap_used(0); // 第一个inode已分配（ino=0）

        // 完成文件系统初始化，关闭文件系统载体文件 
        free(root_inode);
        root_inode = NULL;

        // test(); // 测试用
    }

    // 检查超级块属性
    printf("\tsuper block: first inode bitmap=%ld\n", sb->first_blk_of_inodebitmap);
    printf("\tsuper block: inode bitmap size=%ld\n", sb->inodebitmap_size);
    printf("\tsuper block: first datablock bitmap=%ld\n", sb->first_blk_of_databitmap);
    printf("\tsuper block: datablock bitmap size=%ld\n", sb->databitmap_size);
    printf("\tsuper block: first inode=%ld\n", sb->first_inode);
    printf("\tsuper block: first datablock=%ld\n", sb->first_blk);
    printf("\tsuper block: file system size=%ld\n", sb->fs_size);
    // 检查root_entry
    char* type = root_entry->type == DIR_TYPE ? "DIR": "FILE";
    printf("\troot entry: name=%s\n", root_entry->name);
    printf("\troot entry: type=%s\n", type);
    printf("\troot entry: inode=%d\n", root_entry->inode);

    // fclose(fs);
    return NULL;
}

// 读取文件属性
static int SFS_getattr(const char *path, 
                       struct stat *stbuf, 
                       struct fuse_file_info *fi) {
    (void) fi;
    printf("[SFS_getattr] path=%s\n", path);

    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));

    if (find_entry(path, entry) == -1) {
        printf("[SFS_getattr] Error: path %s is not existed\n", path);
        return -ENOENT; // 没有该目录或文件
    }
    
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(entry->inode, inode);

    // 根据inode赋值stbuf(struct stat)
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = inode->st_mode;
    stbuf->st_ino = inode->st_ino;
	stbuf->st_nlink = inode->st_nlink;
	stbuf->st_uid = inode->st_uid;
	stbuf->st_gid = inode->st_gid;
	stbuf->st_size = inode->st_size;
    stbuf->st_blksize = BLOCK_SIZE;
    stbuf->st_blocks = 1; // FIXME st_blocks

    free(entry);
    free(inode);
    entry = NULL;
    inode = NULL;

    return 0;
}

/**
 * 读取目录，readdir在ls过程中每次仅会返回一个目录项
 * 其中offset参数记录着当前一个返回的目录项
*/
static int SFS_readdir(const char* path, void* buf, 
                       fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi,
                       enum fuse_readdir_flags flags) {
    (void) fi;
    off_t cur = offset;
    printf("[SFS_readir] path=%s\n", path);
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
    if (find_entry(path, entry) == -1) {
        // 该目录没有对应entry
        printf("[SFS_readir] Error: this path %s does not exist\n", path);
        return -1;
    }

    // 存在该路径对应entry
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(entry->inode, inode);
    struct dir* dir = (struct dir*)malloc(sizeof(struct dir));
    read_dir(inode, dir);

    if (cur < dir->num_entries) {
        char fname[MAX_FILE_NAME + 1 + MAX_FILE_EXTENSION];
        full_name(dir->entries[cur]->name, dir->entries[cur]->extension, fname);
        // strcpy(fname, dir->entries[cur]->name);
        // printf("[SFS_readdir] name=%s\n", fname);
        filler(buf, fname, NULL, ++cur, 0);
    }

    return 0;
}

// 创建目录
static int SFS_mkdir(const char* path, mode_t mode) {
    printf("[SFS_mkdir] path=%s\n", path);
    (void) mode;
    struct entry* parent_entry = (struct entry*)malloc(sizeof(struct entry));
    char* parent_path = (char*)malloc(sizeof(path));
    // 获取path的上一级目录
    get_parent_path(path, parent_path); // 获得上一级路径
    find_entry(parent_path, parent_entry); // 获得上一级entry（需要保证是目录文件类型）
    if (parent_entry->type != DIR_TYPE) {
        // 需要保证上一级是目录文件类型
        printf("[SFS_mkdir] Error: parent entry is not DIR type\n");
        free(parent_entry);
        free(parent_path);
        parent_entry = NULL;
        parent_path = NULL;
        return -1;
    }

    // 寻找空闲inode
    short int* ino = (short int*)malloc(sizeof(short int));
    get_free_ino(ino);
    if (*ino == -1) {
        // 没有空闲inode
        printf("[SFS_mkdir] Error: there is no free inode for new entry\n");
        free(parent_entry);
        free(parent_path);
        free(ino);
        return -1;
    }

    // 创建新inode指向new_entry
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    new_inode(inode, *ino, DIR_TYPE);
    // 将该inode写入虚拟磁盘
    write_inode(*ino, inode); 
    set_inode_bitmap_used(*ino);

    // 创建新的entry作为目录文件
    char file_name[MAX_FILE_NAME + MAX_FILE_EXTENSION + 1];
    get_file_name(path, file_name);
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
    new_entry(entry, file_name, "", DIR_TYPE, *ino);

    struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(parent_entry->inode, parent_inode);
    //printf("[SFS_mkdir] add entry name=%s\n", entry->name);
    add_entry(parent_inode, entry);

    free(ino);
    free(inode);
    free(entry);
    ino = NULL;
    inode = NULL;
    entry = NULL;
    free(parent_path);
    free(parent_entry);
    free(parent_inode);
    parent_path = NULL;
    parent_entry = NULL;
    parent_inode = NULL;
    return 0;
}

// 删除目录
static int SFS_rmdir(const char* path) {
    printf("[SFS_rmdir] path=%s\n", path);
    if (strcmp(path, "/") == 0) {
        // 根目录无法删除
        printf("[SFS_rmdir] fail to remove the root dir\n");
        return -1;
    }
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry)); // 待删除的entry
    if (find_entry(path, entry) != 0) {
        // 不存在该路径对应的entry
        printf("[SFS_rmdir] the path %s does not exist\n", path);
        return -1;
    }
    // 找到了路径对应的entry
    struct entry* parent_entry = (struct entry*)malloc(sizeof(struct entry));
    char* parent_path = (char*)malloc(sizeof(path));
    // 获取path的上一级目录
    get_parent_path(path, parent_path); // 获得上一级路径
    find_entry(parent_path, parent_entry); // 获得上一级entry
    // 遍历parent_inode的数据块进行匹配删除
    struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(parent_entry->inode, parent_inode);
    remove_entry(parent_inode, entry);

    free(entry);
    free(parent_entry);
    free(parent_inode);
    return 0;
}

// 创建文件
// touch要求不仅仅是创建文件，还要求可以修改文件的访问时间
static int SFS_mknod(const char* path, mode_t mode, dev_t dev) {
    printf("[SFS_mknod] path=%s\n", path);
    (void) mode;
    (void) dev;
    struct entry* parent_entry = (struct entry*)malloc(sizeof(struct entry));
    char* parent_path = (char*)malloc(sizeof(path));
    // 获取path的上一级目录
    get_parent_path(path, parent_path); // 获得上一级路径
    find_entry(parent_path, parent_entry); // 获得上一级entry（需要保证是目录文件类型）
    if (parent_entry->type != DIR_TYPE) {
        // 需要保证上一级是目录文件类型
        printf("[SFS_mknod] Error: parent entry is not DIR type\n");
        free(parent_entry);
        free(parent_path);
        parent_entry = NULL;
        parent_path = NULL;
        return -1;
    }

    // 寻找空闲inode
    short int* ino = (short int*)malloc(sizeof(short int));
    get_free_ino(ino);
    if (*ino == -1) {
        // 没有空闲inode
        printf("[SFS_mknod] Error: there is no free inode for new entry\n");
        free(parent_entry);
        free(parent_path);
        free(ino);
        return -1;
    }

    // 创建新inode指向new_entry
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    new_inode(inode, *ino, FILE_TYPE);
    // 将该inode写入虚拟磁盘
    write_inode(*ino, inode); 
    set_inode_bitmap_used(*ino);

    // 创建新的entry作为文件
    char file_name[MAX_FILE_NAME + MAX_FILE_EXTENSION + 1];
    get_file_name(path, file_name);
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));

    fname_ext(file_name, entry->name, entry->extension);
    new_entry(entry, entry->name, entry->extension, FILE_TYPE, *ino);

    struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(parent_entry->inode, parent_inode);
    //printf("[SFS_mknod] add entry name=%s\n", entry->name);
    add_entry(parent_inode, entry);

    free(ino);
    free(inode);
    free(entry);
    ino = NULL;
    inode = NULL;
    entry = NULL;
    free(parent_path);
    free(parent_entry);
    free(parent_inode);
    parent_path = NULL;
    parent_entry = NULL;
    parent_inode = NULL;

    return 0;
}

// 删除文件
static int SFS_unlink(const char* path) {
        printf("[SFS_unlink] path=%s\n", path);
    if (strcmp(path, "/") == 0) {
        // 根目录无法删除
        printf("[SFS_unlink] fail to remove the root dir\n");
        return -1;
    }
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry)); // 待删除的entry
    if (find_entry(path, entry) != 0) {
        // 不存在该路径对应的entry
        printf("[SFS_unlink] the path %s does not exist\n", path);
        return -1;
    }
    // 找到了路径对应的entry
    struct entry* parent_entry = (struct entry*)malloc(sizeof(struct entry));
    char* parent_path = (char*)malloc(sizeof(path));
    // 获取path的上一级目录
    get_parent_path(path, parent_path); // 获得上一级路径
    find_entry(parent_path, parent_entry); // 获得上一级entry
    // 遍历parent_inode的数据块进行匹配删除
    struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(parent_entry->inode, parent_inode);
    remove_entry(parent_inode, entry);

    free(entry);
    free(parent_entry);
    free(parent_inode);
    return 0;
}

// 打开文件
static int SFS_open(const char* path, struct fuse_file_info* fi) {
	(void)fi;
	(void)path;
	return 0;

}

// 关闭文件
static int SFS_release(const char* path, struct fuse_file_info* fi) {
    (void)fi;
	(void)path;
    return 0;
}

// 读文件
static int SFS_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    printf("[SFS_read] path=%s\n", path);
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
    find_entry(path, entry);
    if (entry->type != FILE_TYPE) {
        printf("[SFS_read] the path %s is not a file\n", path);
        return -EISDIR; // 无法读取目录
    }
    // 获取读取文件的inode
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(entry->inode, inode);
    if (inode->st_size < offset) {
        free(entry);
        free(inode);
        // 要保证文件大小大于所要读取的偏移量
        return -ESPIPE;
    }
    printf("[SFS_read] inode size=%ld\n", inode->st_size);

    char* data = malloc(inode->st_size);
    read_file(inode, data, size); // 将inode存储的数据读取到data
    // 将数据读到buf内存
    memcpy(buf, data + offset, size);
    free(entry);
    free(inode);
    return size; // 返回实际读取的字节数，如果读取失败，返回负数表示错误
}

// 写文件
static int SFS_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    printf("[SFS_write] path=%s\n", path);
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
    find_entry(path, entry);
    if (entry->type != FILE_TYPE) {
        printf("[SFS_read] the path %s is not a file\n", path);
        return -EISDIR; // 无法读取目录
    }
    // 获取待写文件的inode
    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    read_inode(entry->inode, inode);
    if (inode->st_size < offset) {
        // 要保证文件大小大于所要读取的偏移量
        free(entry);
        free(inode);
        return -ESPIPE;
    }

    int new_size = MAX(offset + size, inode->st_size);
    char* data = malloc(new_size);
    read_file(inode, data, inode->st_size);
    memcpy(data + offset, buf, size);
    write_file(inode, data, new_size);
    inode->st_size = new_size; // 更新inode大小
    // 写回inode到磁盘
    write_inode(inode->st_ino, inode);
    return size;
}

// 修改时间
int SFS_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info *fi) {
	(void) path;
    (void) fi;
	return 0;
}


// 定义文件系统支持的操作函数，并添加到该结构体中
// fuse会在执行linux相关操作时执行我们所定义的文件操作函数
static struct fuse_operations SFS_operations = {
    .init    = SFS_init,    // 初始化文件系统
    .getattr = SFS_getattr, // 获取文件或目录的属性
    .readdir = SFS_readdir, // 读取目录
    .mkdir   = SFS_mkdir,   // 创建目录
    .rmdir   = SFS_rmdir,   // 删除目录
    .mknod   = SFS_mknod,   // 创建文件
    .unlink  = SFS_unlink,  // 删除文件
    .open    = SFS_open,    // 打开文件
    .release = SFS_release, // 关闭文件
    .read    = SFS_read,    // 读文件
    .write   = SFS_write,   // 写文件
    .utimens = SFS_utimens, // 修改时间（创建文件要求实现）
};

int main(int argc, char *argv[]) {
    // printf("%lu\n", sizeof(struct file));
    // 权限掩码，umask(0)为0取反再创建文件时权限（mode）相与
    // 即(~0) & mode，在八进制中为：0777 & mode
    // 为后面的代码调用函数mkdir给出最大的权限，避免了创建目录或文件的权限不确定性
    umask(0);
    // fuse库的入口起点，通过SFS_operation包含的回调函数来执行文件系统操作
    int ret = 0;
    ret = fuse_main(argc, argv, &SFS_operations, NULL);
    fclose(fs);
    return ret;
}
