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

// int read_entry(struct inode* inode, struct entry* entry) {
//     off_t read_size = 0; // 已读取的数据块大小
//     int index = 0; // 索引级别
//     while (read_size < inode->st_size) {
//         if (index <= 3) { 
//             直接索引
//             short int data_block_no = inode->addr[index++];
//             if (!data_block_is_used(data_block_no)) {
//                 continue;
//             }
//             struct data_block* data_block = malloc(sizeof(data_block));
//             read_data_block(data_block_no, data_block); // 读取对应数据块
//             将数据块中存放的entry内容拷贝出来
//             memcpy(entry, data_block->data, sizeof(entry));
//             free(data_block);
//             break;
//         }
//     }
//     return 0;
// }

/**
 * 在父目录parent_entry下添加新的entry
*/
int add_entry(struct entry* parent_entry, struct entry* new_entry) {
    if (parent_entry->inode == -1) {
        // 空目录，分配新的inode存放entry
        short int* ino = (short int*)malloc(sizeof(short int));
        get_free_ino(ino);
        if (*ino == -1) {
            // 没有空闲inode
            printf("[add_entry] Error: there is no free inode");
            free(ino);
            return -1;
        }
        parent_entry->inode = *ino;
        free(ino);
    }
    // 读取父目录inode，将new_entry添加进去
    struct inode* inode = (struct inode*)malloc(sizeof(inode));
    read_inode(parent_entry->inode, inode);
    struct data_block* last_datablock = (struct data_block*)malloc(sizeof(struct data_block));
    get_last_datablock(inode, last_datablock);
    // 将new_entry写入该数据块
    off_t used_size = inode->st_size % BLOCK_SIZE; // 该数据块前面已使用的空间大小
    memcpy(last_datablock + used_size, new_entry, sizeof(struct entry));
    inode->st_size += sizeof(struct entry); // 修改目录大小

    free(inode);
    inode = NULL;
    free(last_datablock);
    last_datablock = NULL;
    return 0;
}

// 根据inode号获取目录（包括子目录和文件）
int read_dir(struct inode* inode, struct dir* dir) {
    // TODO 实现read_dir
    dir->num_entries = 0;
    off_t file_size = inode->st_size;

    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    init_inode_iter(iter, inode);
    struct data_block* data_block = (struct data_block*)malloc(sizeof(struct data_block));
    while (has_next(iter)) {
        next(iter, data_block);
        file_size -= sizeof(data_block);
        size_t read_size = MIN(file_size, sizeof(data_block));
        while (read_size > 0) {
            struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
            memcpy(entry, data_block->data, sizeof(struct entry));
            dir->entries[dir->num_entries++] = entry;
            read_size -= sizeof(struct entry);
        }        
    }
    free(iter);
    
    // off_t read_size = 0; // 已读取的数据块大小
    // int index = 0; // 索引级别
    
    // while (read_size < inode->st_size) {
    //     if (index <= 3) {       
    //         // 直接索引（index=0, 1, 2, 3）
    //         // 读取数据块号并判断位图有无使用该数据块
    //         short int data_block_no = inode->addr[index++];
    //         if (!data_block_is_used(data_block_no)) {
    //             continue;
    //         }
    //         struct data_block* data_block = malloc(sizeof(data_block));
    //         read_data_block(data_block_no, data_block); // 读取对应数据块
    //         size_t datablock_size = 0;
    //         while (datablock_size < data_block->size) {
    //             // 从数据块中取出目录项entry集合放到dir中
    //             struct entry* entry = malloc(sizeof(struct entry));
    //             memcpy(entry, data_block->data, sizeof(struct entry));
    //             dir->entries[dir->num_entries++] = entry;
    //             datablock_size += sizeof(struct entry);
    //         }
    //         read_size += data_block->size;
    //         free(data_block);
    //     } else if (index == 4) {
    //         // 一级索引
    //         // TODO inode多级索引实现
    //         break;
    //     }
    // }
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
        entry = root_entry;
        return 0;
    }

    struct inode* cur_inode = (struct inode*)malloc(sizeof(struct inode));
    struct entry* cur_entry = (struct entry*)malloc(sizeof(struct entry));
    struct dir* cur_dir = (struct dir*)malloc(sizeof(struct dir));

    // 不能操作const的path参数，只能操作path_copy
    char* path_copy = (char*)malloc(sizeof(path));
    strcpy(path_copy, path);
    // 路径解析
    if (path_copy[0] == '/') {
        // 路径第一个字符为"/"则是从根目录开始遍历
        cur_entry = root_entry;
        strcpy(path_copy, path_copy + 1); // 去除path的第一个"/"字符
    } else {
        cur_entry = work_entry; // 工作目录开始遍历
    }
    
    char* head = (char*)malloc(sizeof(path_copy));
    char* tail = (char*)malloc(sizeof(path_copy));
    split_path(path_copy, head, tail);

    short int cur_inode_no = cur_entry->inode;
    read_inode(cur_inode_no, cur_inode); // 获取inode
    read_dir(cur_inode, cur_dir);        // 根据inode获取对应dir
    // 遍历entry匹配文件名
    for (int i=0; i<cur_dir->num_entries; i++) {
        if (strcmp(cur_dir->entries[i]->name, head) == 0) {
            // 存在该目录，继续解析
            int ret = find_entry(tail, entry);
            free(head);
            free(tail);
            free(path_copy);
            return ret;
        }
    }
    printf("[find_entry] Error: the entry %s does not exist\n", path);
    free(head);
    free(tail);
    free(path_copy);
    return -1;
}

/**
 * 获得path的上一级目录entry
 * example: path="abc/ef/g" -> 返回ef对应entry指针
*/
// int find_last_entry(const char* path, struct entry* entry) {
//     printf("[find_last_entry] path=%s\n", path);
//     if (path == NULL || strcmp(path, "") == 0) {
//         printf("[find_last_entry] Error: the entry path should not be NULL or empty\n");
//         return -1;
//     }

//     if (strcmp(path, "/") == 0) {
//         // 根目录
//         entry = root_entry;
//         return 0;
//     }

//     struct inode* cur_inode = (struct inode*)malloc(sizeof(struct inode));
//     struct entry* cur_entry = (struct entry*)malloc(sizeof(struct entry));
//     struct entry* last_entry = (struct entry*)malloc(sizeof(struct entry)); // 上一级entry
//     struct dir* cur_dir = (struct dir*)malloc(sizeof(struct dir));

//     // 不能操作const的path参数，只能操作path_copy
//     char* path_copy = (char*)malloc(sizeof(path));
//     strcpy(path_copy, path);
//     // 路径解析
//     if (path_copy[0] == '/') {
//         // 路径第一个字符为"/"则是从根目录开始遍历
//         cur_entry = root_entry;
//         strcpy(path_copy, path_copy + 1); // 去除path的第一个"/"字符
//     } else {
//         cur_entry = work_entry; // 工作目录开始遍历
//     }
    
//     char* head = (char*)malloc(sizeof(path_copy));
//     char* tail = (char*)malloc(sizeof(path_copy));
//     split_path(path_copy, head, tail);

//     short int cur_inode_no = cur_entry->inode;
//     read_inode(cur_inode_no, cur_inode); // 获取inode
//     read_dir(cur_inode, cur_dir);        // 根据inode获取对应dir
//     // 遍历entry匹配文件名
//     for (int i=0; i<cur_dir->num_entries; i++) {
//         if (strcmp(cur_dir->entries[i]->name, head) == 0) {
//             // 存在该目录，继续解析
//             int ret = find_last_entry(tail, entry);
//             free(head);
//             free(tail);
//             free(path_copy);
//             return ret;
//         }

//     }
//     printf("[find_entry] Error: the entry %s does not exist\n", path);
//     free(head);
//     free(tail);
//     free(path_copy);
//     return -1;


//     return 0;
// }

// 根据文件路径获取其对应的inode
int find_inode(const char* path, struct inode* inode) {
    printf("[find_inode] path=%s\n", path);
    struct entry* entry = malloc(sizeof(struct entry));
    int ret = -1;
    if (find_entry(path, entry) == 0) {
        // 存在该路径
        ret = read_inode(entry->inode, inode);
    }
    free(entry);
    return ret;
}

/***
// 根据inode号读取对应目录（包括子目录和文件集合）
struct dir* read_dir(struct inode* inode) {
    // 需要提前确保该inode对应的文件类型是目录项
    struct dir* dir = malloc(sizeof(struct dir));
    dir->num_entries = 0;
    // 读取该目录项inode的数据块，获得子目录和文件
    off_t read_size = 0; // 已读取的数据块大小
    int index = 0; // 读取到的addr索引
    while (read_size < inode->st_size) {
        if (index >= 7) break; // 仅支持三级索引（addr最大索引为6）
        if (index == 4) {
            // 一级间接索引块号
            short int first_data_block_no = inode->addr[index++];
            if (!data_block_is_used(first_data_block_no)) {
                continue;
            }
            // 一级索引块
            struct data_block* first_data_block = read_data_block(first_data_block_no); // 读取对应数据块
            short int* data_block_no_arr = (short int*) first_data_block->data;
            size_t first_offset = 0;
            // 遍历一级间接索引块中的数据块号，访问数据块
            while (first_offset < first_data_block->size) {
                short int* data_block_no = malloc(sizeof(short int));
                memcpy(data_block_no, data_block_no_arr, sizeof(short int));
                data_block_no_arr += sizeof(short int);
                first_offset += sizeof(short int);
                if (!data_block_is_used(*data_block_no)) {
                    continue;
                }
                struct data_block* data_block = read_data_block(*data_block_no);
                // 一个块装的都是目录项
                struct entry* entries = (struct entry*)data_block->data;
                size_t offset = 0;
                while (offset < data_block->size) {
                    memcpy(dir->entries[dir->num_entries++], entries, sizeof(struct entry));
                    entries += sizeof(struct entry);
                    offset += sizeof(struct entry);
                }
                read_size += data_block->size;
            }
        } else if (index == 5) {
            // 二级间接索引
            short int second_data_block_no = inode->addr[index++];
            if (!data_block_is_used(second_data_block_no)) {
                continue;
            }
            // 二级索引块
            struct data_block* second_data_block = read_data_block(second_data_block_no); // 读取对应数据块
            // 二级间接块存放的是一级间接块号
            short int* first_data_block_no_arr = (short int*) second_data_block->data;
            size_t second_offset = 0;
            while (second_offset < second_data_block->size) {
                short int* first_data_block_no = malloc(sizeof(short int));
                memcpy(first_data_block_no, first_data_block_no_arr, sizeof(short int));
                first_data_block_no_arr += sizeof(short int);
                second_offset += sizeof(short int);
                if (!data_block_is_used(*first_data_block_no)) {
                    continue;
                }
                struct data_block* first_data_block = read_data_block(*first_data_block_no); // 读取一级间接块
                short int* data_block_no_arr = (short int*) first_data_block->data;
                size_t first_offset = 0;
                // 遍历一级间接索引块中的数据块号，访问数据块
                while (first_offset < first_data_block->size) {
                    short int* data_block_no = malloc(sizeof(short int));
                    memcpy(data_block_no, data_block_no_arr, sizeof(short int));
                    data_block_no_arr += sizeof(short int);
                    first_offset += sizeof(short int);
                    if (!data_block_is_used(*data_block_no)) {
                        continue;
                    }
                    struct data_block* data_block = read_data_block(*data_block_no);
                    // 一个块装的都是目录项
                    struct entry* entries = (struct entry*)data_block->data;
                    size_t offset = 0;
                    while (offset < data_block->size) {
                        memcpy(dir->entries[dir->num_entries++], entries, sizeof(struct entry));
                        entries += sizeof(struct entry);
                        offset += sizeof(struct entry);
                    }
                    read_size += data_block->size;
                }
            }
        } else if (index == 6) {
            // 三级间接索引
            short int third_data_block_no = inode->addr[index++];
            if (!data_block_is_used(third_data_block_no)) {
                continue;
            }
            // 三级索引块
            struct data_block* third_data_block = read_data_block(third_data_block_no); // 读取对应数据块
            // 三级间接块存放的是二级间接块号
            short int* second_data_block_no_arr = (short int*) third_data_block->data;
            size_t third_offset = 0;
            while (third_offset < third_data_block->size) {
                // 二级间接索引
                short int* second_data_block_no = malloc(sizeof(short int));
                memcpy(second_data_block_no, second_data_block_no_arr, sizeof(short int));
                second_data_block_no_arr += sizeof(short int);
                third_offset += sizeof(short int);
                if (!data_block_is_used(*second_data_block_no)) {
                    continue;
                }
                // 二级索引块
                struct data_block* second_data_block = read_data_block(*second_data_block_no); // 读取对应数据块
                // 二级间接块存放的是一级间接块号
                short int* first_data_block_no_arr = (short int*) second_data_block->data;
                size_t second_offset = 0;
                while (second_offset < second_data_block->size) {
                    short int* first_data_block_no = malloc(sizeof(short int));
                    memcpy(first_data_block_no, first_data_block_no_arr, sizeof(short int));
                    first_data_block_no_arr += sizeof(short int);
                    second_offset += sizeof(short int);
                    if (!data_block_is_used(*first_data_block_no)) {
                        continue;
                    }
                    struct data_block* first_data_block = read_data_block(*first_data_block_no); // 读取一级间接块
                    short int* data_block_no_arr = (short int*) first_data_block->data;
                    size_t first_offset = 0;
                    // 遍历一级间接索引块中的数据块号，访问数据块
                    while (first_offset < first_data_block->size) {
                        short int* data_block_no = malloc(sizeof(short int));
                        memcpy(data_block_no, data_block_no_arr, sizeof(short int));
                        data_block_no_arr += sizeof(short int);
                        first_offset += sizeof(short int);
                        if (!data_block_is_used(*data_block_no)) {
                            continue;
                        }
                        struct data_block* data_block = read_data_block(*data_block_no);
                        // 一个块装的都是目录项
                        struct entry* entries = (struct entry*)data_block->data;
                        size_t offset = 0;
                        while (offset < data_block->size) {
                            memcpy(dir->entries[dir->num_entries++], entries, sizeof(struct entry));
                            entries += sizeof(struct entry);
                            offset += sizeof(struct entry);
                        }
                        read_size += data_block->size;
                    }
                }
            }
        } else {
            // 直接索引（index=0, 1, 2, 3）
            // 读取数据块号并判断位图有无使用该数据块
            short int data_block_no = inode->addr[index++];
            if (!data_block_is_used(data_block_no)) {
                continue;
            }
            struct data_block* data_block = read_data_block(data_block_no); // 读取对应数据块
            size_t datablock_size = 0;
            while (datablock_size < data_block->size) {
                // 从数据块中取出目录项entry集合放到dir中
                struct entry* entry = malloc(sizeof(struct entry));
                memcpy(entry, data_block->data, sizeof(struct entry));
                dir->entries[dir->num_entries++] = entry;
                datablock_size += sizeof(struct entry);
            }
            read_size += sizeof(data_block->size);
        }
    }
    return dir;
}

// 根据文件路径获取其对应的inode
struct inode* find_inode(const char* path) {
    // 一个进程要打开“/etc/passwd”文件，内核解析该路径名时，发现“/”，这样就将根目录作为工作目录，
    // 然后从该目录中（inode所包好的数据块中）查找etc，找到了匹配后得到了etc的inode号，
    // 然后读取etc的内容，在其中查找passwd，找到之后就可以返回对应的inode
    
    // 文件路径解析
    if (path == NULL || strcmp(path, "") == 0) {
        return NULL;
    }
    struct dir* dir = malloc(sizeof(struct dir)); // 需要进行遍历的目录
    // char file_name[MAX_FILE_NAME]; // 文件名（可能为目录项文件名）
    // char file_extension[MAX_FILE_EXTENSION]; // 文件扩展名 
    char path_copy[MAX_PATH_LEN]; // path参数的副本（需要操作路径参数，原参数为const）
    strcpy(path_copy, path);
    if (path_copy[0] == '/') {
        // 路径第一个字符为"/"则是从根目录开始遍历
        dir = root_dir;
        strcpy(path_copy, path_copy + 1); // 去除path的第一个"/"字符
    } else {
        dir = work_dir;
    }
    // 逐级分解目录，获取最终文件
    while (strstr(path, "/")) {
        char temp[MAX_PATH_LEN];
        // 由于strtok会修改原字符串，这里保存修改前的字符串
        strcpy(temp, path_copy); 
        strtok(path_copy, "/"); // 获得第一个"/"前的目录项名
        // 截取第一个"/"后的字符串到path_copy
        strncpy(path_copy, temp + strlen(path_copy) + 1, strlen(temp) - strlen(path_copy));
        // 若path_copy不为空，此时temp为一个目录项
        // 遍历每个目录项，若匹配文件名则获取相应的inode
        for (int i=0; i<dir->num_entries; i++) {
            if (strcmp(dir->entries[i]->name, temp) == 0) {
                // 找到匹配的目录项名
                if (dir->entries[i]->type != DIR_TYPE) {
                    // dir需要是目录项
                    return NULL;
                }
                short int ino = dir->entries[i]->inode; // 获取inode号
                // 需要利用inode位图判断该inode是否使用
                if (!inode_is_used(ino)) {
                    // inode未使用则不存在该文件或目录
                    return NULL;
                }
                // 找到该inode对应的位置，即该目录文件存在，继续访问下一级目录
                struct inode* inode = read_inode(ino);
                // TODO
                dir = read_dir(inode); // 根据该目录项的inode读取完整目录

                // // 读取该目录项inode的数据块，获得子目录和文件
                // off_t read_size = 0; // 已读取的数据块大小
                // int index = 0; // 读取到的addr索引
                // dir->num_entries = 0;

                // while (read_size < inode->st_size) {
                //     if (index >= 7) break; // 仅支持三级索引
                //     // 读取数据块号并判断位图有无使用该数据块
                //     short int data_block_no = inode->addr[index++];
                //     if (!data_block_is_used(data_block_no)) {
                //         continue;
                //     }
                //     struct data_block* data_block = read_data_block(data_block_no); // 读取对应数据块
                //     // 从数据块中取出目录项entry
                //     // struct entry* entry = malloc(sizeof(struct entry));

                //     read_size += sizeof(struct entry);
                // }
                break; // 已找到该目录项

            }
        }
        // 
    }

    // 未找到文件路径对应的inode
    return NULL;
}

// 根据inode号将对应数据读到block指针中
// int read_block(uint16_t inode, struct block* block) {
//     FILE* fp = NULL;
//     fp = fopen(fs_img, "r+");
//     if (fp == NULL) {
//         打开文件系统载体文件失败
//         printf("Failed to open the file system image\n");
//         return -1;
//     }
//     成功打开文件系统载体文件，读取inode对应数据块
//     return 0;
// }

***/

// ************************************************************************************
// 以下为fuse_operations需要实现的SFS回调函数

/*
 * 初始化文件系统
 * 1. 打开文件系统的载体文件（sfs.img）
 * 2. 检查文件系统是否已经被初始化，如果已经初始化，可以跳过初始化步骤
 * 3. 如果文件系统尚未初始化，进行初始化操作，包括超级块的填充以及inode位图和数据块位图的初始化
*/
static void* SFS_init(struct fuse_conn_info* conn, struct fuse_config *cfg) {
    // 8M大小的磁盘文件映像路径，该文件作为SFS文件系统的载体
    fs = fopen(fs_img, "rb+");
    if (fs == NULL) {
        // 检查映像文件路径
        perror("[SFS_init] Error: failed to open the file system image");
        printf("[SFS_init] Error: the file system image's path: %s\n", fs_img);
        return NULL;
    }

    // 检查文件系统是否已经初始化，可以通过检查超级块的fs_size来实现
    sb = malloc(sizeof(struct sb));
    root_entry = (struct entry*)malloc(sizeof(struct entry));
    fseek(fs, 0, SEEK_SET);              // 定位超级块位置
    fread(sb, sizeof(struct sb), 1, fs); // 读取超级块数据

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
        sb->fisrt_blk_of_inodebitmap = 1; // inode位图的第一块块号
        sb->inodebitmap_size = NUM_INODE_BITMAP_BLOCK; // inode位图大小为1块（512B）
        sb->first_blk_of_databitmap = sb->fisrt_blk_of_inodebitmap + sb->inodebitmap_size; // 数据块位图的第一块块号（第2块）
        sb->databitmap_size = NUM_DATA_BITMAP_BLOCK; // 数据块位图大小为4块（4 * 512 = 2048 Byte）
        sb->first_inode = sb->first_blk_of_databitmap + sb->databitmap_size; // inode区的第一块块号（第6块）
        sb->inode_area_size = sb->inodebitmap_size * BLOCK_SIZE * 8; // inode区大小为512*8块，该文件系统最多有4k个文件
        sb->first_blk = sb->first_inode + sb->inode_area_size; // 数据区的第一块块号（6 + 4096 = 4102），img: 0x200C00
        sb->datasize = sb->databitmap_size * BLOCK_SIZE * 8; // 数据区大小为4*512*8块

        // 将超级块数据写到到文件系统载体文件
        fseek(fs, 0, SEEK_SET);
        fwrite(sb, sizeof(struct sb), 1, fs);

        // 初始化inode位图和数据块位图
        uint8_t inode_bitmap[NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE] = {0}; // 1块inode位图，表示最多4k个文件
        uint8_t data_bitmap[NUM_DATA_BITMAP_BLOCK * BLOCK_SIZE] = {0}; // 4块数据块位图

        // 初始化根目录属性
        strcpy(root_entry->name, "/");     // 根目录为"/"
        strcpy(root_entry->extension, ""); // 目录扩展名为空字符串
        root_entry->type = DIR_TYPE;       // 目录文件类型
        root_entry->inode = -1;            // 根目录的inode号为0
        work_entry = root_entry;           // 当前工作目录为根目录

        // 将根目录的相关信息填写到inode区的第一个inode
        struct inode* root_inode = (struct inode*)malloc(sizeof(struct inode));
        root_inode->st_mode  = __S_IFDIR | 0755; // 目录文件
        // root_inode->st_mode = 0755;
        root_inode->st_ino   = 0; // 根目录的inode号为0（第一个）
        root_inode->st_nlink = 2; // 链接引用数
        root_inode->st_uid   = 0; // getuid(); // 拥有者的用户ID，0为超级用户
        root_inode->st_gid   = 0; // getgid(); // 拥有者的组ID，0为超级用户组
        root_inode->st_size  = 0; // 初始大小为空

        // 将根目录entry写入第一个数据块
        struct data_block* root_datablock = (struct data_block*)malloc(sizeof(struct data_block));
        memcpy(root_datablock, root_entry, sizeof(struct entry));

        // 设置根目录inode位图的第一个字节的第一位为1，表示第一个inode已分配（根目录）
        inode_bitmap[0] |= 0x80;

        fseek(fs, sb->fisrt_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET); // 定位到inode位图区
        fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs); // 写入inode位图数据

        fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET); // 定位到数据块位图区
        fwrite(data_bitmap, sizeof(data_bitmap), 1, fs); // 写入数据块位图数据

        // 初始化根目录inode和数据块
        write_inode(0, root_inode);
        write_data_block(0, root_datablock);

        // 完成文件系统初始化，关闭文件系统载体文件 
        free(root_inode);
        free(root_datablock);
        root_inode = NULL;
        root_datablock = NULL;
    }

    // 检查超级块属性
    printf("\tsuper block: first inode bitmap=%ld\n", sb->fisrt_blk_of_inodebitmap);
    printf("\tsuper block: inode bitmap size=%ld\n", sb->inodebitmap_size);
    printf("\tsuper block: first datablock bitmap=%ld\n", sb->first_blk_of_databitmap);
    printf("\tsuper block: datablock bitmap size=%ld\n", sb->databitmap_size);
    printf("\tsuper block: first inode=%ld\n", sb->first_inode);
    printf("\tsuper block: first datablock=%ld\n", sb->first_blk);
    printf("\tsuper block: file system size=%ld\n", sb->fs_size);

    char* type = root_entry->type == DIR_TYPE ? "DIR": "FILE";
    printf("\troot entry: name=%s\n", root_entry->name);
    printf("\troot entry: type=%s\n", type);
    printf("\troot entry: int type=%d\n", root_entry->type);
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

    struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
    find_inode(path, inode);

    if (inode == NULL) {
        printf("[SFS_getattr] Error: inode is NULL\n");
        return -1;
    }

    // 根据inode赋值stbuf(struct stat)
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = inode->st_mode;
    stbuf->st_ino = inode->st_ino;
	stbuf->st_nlink = inode->st_nlink;
	stbuf->st_uid = inode->st_uid;
	stbuf->st_gid = inode->st_gid;
	stbuf->st_size = inode->st_size;
    stbuf->st_blksize = BLOCK_SIZE;
    stbuf->st_blocks = 0; // FIXME

    free(inode);

    return 0;
}

// 读取目录
static int SFS_readdir(const char* path, void* buf, 
                       fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi,
                       enum fuse_readdir_flags flags) {
    // TODO SFS_readdir 读取目录
    (void) offset;
    (void) fi;
   
    if (strcmp(path, "/") != 0) {
        return -ENOENT; // 目录不存在
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    return 0;
}

// 创建目录
static int SFS_mkdir(const char* path, mode_t mode) {
    // TODO SFS_mkdir 创建目录
    (void) mode;
    struct entry* parent_entry = (struct entry*)malloc(sizeof(struct entry));
    char* parent_path = (char*)malloc(sizeof(path));
    get_parent_path(path, parent_path); // 获得上一级路径
    find_entry(parent_path, parent_entry); // 获得上一级entry（需要保证是目录文件类型）
    if (parent_entry->type != DIR_TYPE) {
        // 需要保证上一级是目录文件类型
        printf("[SFS_mkdir] Error: parent entry is not DIR type\n");
        free(parent_entry);
        free(parent_path);
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

    // 创建新的entry作为目录文件
    struct entry* new_entry = (struct entry*)malloc(sizeof(struct entry));
    char file_name[MAX_FILE_NAME];
    get_file_name(path, file_name);
    strcpy(new_entry->name, file_name);
    strcpy(new_entry->extension, "");
    new_entry->type = DIR_TYPE;
    new_entry->inode = -1; // 目录下还没有内容
    




    return 0;
}

// 删除目录
static int SFS_rmdir(const char* path) {
    // TODO SFS_rmdir 删除目录
    return 0;
}

// 创建文件
static int SFS_mknod(const char* path, mode_t mode, dev_t dev) {
    // TODO SFS_mknod 创建文件
    return 0;
}

// 删除文件
static int SFS_unlink(const char* path) {
    // TODO SFS_unlink 删除文件
    return 0;
}

// 打开文件
static int SFS_open(const char* path, struct fuse_file_info* fi) {
    
    // struct inode* inode;
    // // 根据路径查找相应的inode，以确定要打开的文件


    // // 检查文件是否存在
    // if (inode == NULL) {
    //     return -ENOENT; // 未找到该文件
    // }
    // // 暂时认为该文件系统为单用户，不检查权限
    

    // // 已找到inode，将其存储在fi->fh中，以便后续操作使用
    // fi->fh = (uintptr_t)inode;
    // TODO SFS_open 打开文件

    return 0;
}

// 关闭文件
static int SFS_release(const char* path, struct fuse_file_info* fi) {
    // 在这里关闭文件
    // TODO SFS_release 关闭文件
    return 0;
}

// 读文件
static int SFS_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // 读文件内容到buf
    // struct inode* inode = (struct inode*)(uintptr_t)fi->fh;
    // // 根据inode中的数据块索引和offset计算要读取的数据块和位置
    // short int ino = inode->st_ino; // 获取inode号


    // 从数据块中读取数据到buf中，根据size和offset
    // 注意：这里的示例代码假设数据块存储在磁盘上，需要根据文件系统的实际设计来实现数据的读取
    //struct data_block* block = malloc(sizeof(struct data_block));

    // 更新文件的偏移量，这里未实际更新，您需要根据读取的数据大小来更新

    // 返回读取的数据给调用者


    return -1; // 返回实际读取的字节数，如果读取失败，返回负数表示错误
}

// 写文件
static int SFS_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // TODO
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
