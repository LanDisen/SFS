/*
 * 描述SFS文件系统的检查和读写操作（包括bitmap、inode、数据块等）
 * 主要是涉及到SFS虚拟磁盘的读写操作
*/
#ifndef __SFS_RW_H__
#define __SFS_RW_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#include "sfs_ds.h"
#include "sfs_utils.h"

// 利用inode位图判断该inode号是否已使用
int inode_is_used(short int ino) {
    // inode位图
    uint8_t inode_bitmap[NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs); // 读取inode位图
    short int row = ino >> 3; // 位图的行
    short int col = ino % 8; // 位图的列（在0~7之间）
    uint8_t byte = inode_bitmap[row];
    // 根据bitmap判断该inode号是否已使用
    // 如: byte = 10100100, col = 4 = 00000100
    // byte & col = 00000100 != 0 可用于判断inode是否存在
    if (((byte >> (7 - col)) & 1) != 0) {
        // 该inode已使用
        return 1;
    }
    // 该inode未使用
    return 0;
}

// 利用数据块位图判断对应数据块是否已使用
int data_block_is_used(short int data_block_no) {
    // 数据块位图
    uint8_t data_block_bitmap[4 * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fread(data_block_bitmap, sizeof(data_block_bitmap), 1, fs); // 读取数据块位图
    short int row = data_block_no >> 3; // 位图的行
    short int col = data_block_no % 8; // 位图的列（在0~7之间）
    uint8_t byte = data_block_bitmap[row];
    // 根据bitmap判断该数据块是否已使用
    // 如: byte = 10100100, col = 4 = 00000100
    // byte & col = 00000100 != 0 可用于判断inode是否存在
    if (((byte >> (7 - col)) & 1) != 0) {
        // 该inode已使用
        return 1;
    }
    // 该inode未使用
    return 0;
}

// 设置inode号对应bitmap为1表示已使用该inode
int set_inode_bitmap_used(short int ino) {
    // 读取inode位图
    uint8_t inode_bitmap[NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    // 定位
    short int row = ino >> 3; // 位图的行
    short int col = ino % 8; // 位图的列（在0~7之间）
    // inode号对应位置设为1
    uint8_t byte = 1 << (7 - col);
    inode_bitmap[row] |= byte;
    // 写回磁盘
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    printf("[set_inode_bitmap_used] ino=%d\n", ino);
    return 0;
}

// 设置数据块号对应bitmap为1表示已使用该数据块
int set_datablock_bitmap_used(short int data_block_no) {
    // 读取数据块位图
    uint8_t data_block_bitmap[NUM_DATA_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fread(data_block_bitmap, sizeof(data_block_bitmap), 1, fs);
    // 定位
    short int row = data_block_no >> 3; // 位图的行
    short int col = data_block_no % 8; // 位图的列（在0~7之间）
    // 数据块号对应位置设为1
    uint8_t byte = 1 << (7 - col);
    data_block_bitmap[row] |= byte;
    // 写回磁盘
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fwrite(data_block_bitmap, sizeof(data_block_bitmap), 1, fs);
    printf("[set_datablock_bitmap_used] datablock_no=%d\n", data_block_no);
    return 0;
}

/**
 * 获取空闲的inode号
 * 若没有空闲inode则*ino=-1
*/
int get_free_ino(short int* ino) {
    // 读取bitmap
    uint8_t inode_bitmap[NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    // inode bitmap占1个block（512B）
    int num_inodes = NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE * 8;
    int rows = num_inodes / 8;
    for (int i=0; i<rows; i++) {
        uint8_t byte = inode_bitmap[i];
        if (byte != 0xFF) {
            // byte不是全1，该行有空闲inode
            for (int j=7; j>=0; j--) {
                if (((byte >> j) & 1) == 0) {
                    // 有空闲inode
                    *ino = i * 8 + (7 - j);
                    printf("[get_free_ino] alloc ino=%d\n", *ino);
                    return 0;
                }
            }
            
        }
    }
    // 未找到空闲inode
    *ino = -1;
    return -1;
}

/**
 * 获取空闲的数据块号
 * 未找到空闲数据块则*datablock_no=-1
*/
int get_free_datablock_no(short int* datablock_no) {
    // 读取bitmap
    uint8_t data_block_bitmap[NUM_DATA_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fread(data_block_bitmap, sizeof(data_block_bitmap), 1, fs);
    // 数据块bitmap占4个block（4*512B=2048B）
    int num_datablocks = NUM_DATA_BITMAP_BLOCK * BLOCK_SIZE * 8;
    int rows = num_datablocks / 8;
    for (int i=0; i<rows; i++) {
        uint8_t byte = data_block_bitmap[i];
        if (byte != 0xFF) {
            // byte不是全1，该行有空闲数据块
            for (int j=7; j>=0; j--) {
                if (((byte >> j) & 1) == 0) {
                    // 有空闲数据块
                    *datablock_no = i * 8 + (7 - j);
                    printf("[get_free_datablock_no] alloc datablock_no=%d\n", *datablock_no);
                    return 0;
                }
            }
        }
    }
    // 未找到空闲数据块
    *datablock_no = -1;
    return -1;
}

// 根据inode号读取inode
int read_inode(short int ino, struct inode* inode) {
    printf("[read_inode] ino=%d\n", ino);
    if (ino < 0) {
        return -1;
    }
    fseek(fs, (sb->first_inode + ino) * BLOCK_SIZE, SEEK_SET);
    fread(inode, sizeof(struct inode), 1, fs); // 读取inode数据
    // 读取成功
    return 0;
}

// 根据数据块号读取数据块
int read_data_block(short int data_block_no, struct data_block* data_block) {
    printf("[read_data_block] data_block_no=%d\n", data_block_no);
    if (data_block_no < 0) {
        return -1;
    }
    fseek(fs, (sb->first_blk + data_block_no) * BLOCK_SIZE, SEEK_SET);
    fread(data_block, sizeof(struct data_block), 1, fs);
    // 读取成功
    return 0;
}

// 根据inode号写入inode
int write_inode(short int ino, struct inode* inode) {
    printf("[write_inode] ino=%d\n", ino);
    fseek(fs, (sb->first_inode + ino) * BLOCK_SIZE, SEEK_SET);
    fwrite(inode, sizeof(struct inode), 1, fs);
    return 0;
}

// 根据数据块号写入数据块
int write_data_block(short int data_block_no, struct data_block* data_block) {
    printf("[write_data_block] datablock_no=%d\n", data_block_no);
    fseek(fs, (sb->first_blk + data_block_no) * BLOCK_SIZE, SEEK_SET);
    fwrite(data_block, sizeof(struct data_block), 1, fs);
    return 0;
}

/**
 * 为inode分配一个新的数据块
*/
int alloc_datablock(struct inode* inode, short int* datablock_no) {
    // TODO 实现多级alloc_datablock
    get_free_datablock_no(datablock_no);
    if (inode->st_size == 0) {
        inode->addr[0] = *datablock_no;
        set_datablock_bitmap_used(*datablock_no);
        return 0;
    }
    int index = 0; // 索引级别
    while (index <= 6) {
        if (index <= 3) {
            // 直接索引
            if (!data_block_is_used(inode->addr[index])) {
                inode->addr[index] = *datablock_no;
                set_datablock_bitmap_used(*datablock_no);
                return 0;
            }
        } else if (index == 4) {
            // 一级间接索引
        }
        index++;
    }

    return 0;
}

// 以上是bitmap、inode、数据块相关函数

/*------------------------------------------------*/
/* inode迭代器相关函数 */
int has_next(struct inode_iter* iter) {
    if (iter->read_size >= iter->inode->st_size || iter->index > 6) {
        // 超出间接索引级别，或者已读取完全部数据
        return 0;
    }
    return 1; // 有下一个数据块
}

// 一次取出一个可用的数据块（512B），包括其对应的数据块号（可能需要用来将该数据块写回磁盘）
void next(struct inode_iter* iter, struct data_block* data_block) {
    if (iter->index <= 3) {
        // 直接索引
        short int data_block_no = iter->inode->addr[iter->index];
        iter->index += 1;
        iter->datablock_no = data_block_no;
        if (!data_block_is_used(data_block_no)) {
            next(iter, data_block);
            return;
        }
        read_data_block(data_block_no, data_block);
        iter->read_size += sizeof(struct data_block);
        return;
    } else if (iter->index == 4) {
        // TODO 一级间接索引
    }
}

// 获取inode指向的最后一个还有空余空间的数据块，若最后一块满则分配一个新的数据块
// 分配新的数据块后，不会修改inode的文件大小
int get_last_datablock(struct inode* inode, struct data_block* data_block) {
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, inode);
    // struct data_block* data_block = (struct data_block*)malloc(sizeof(struct data_block));
    while (has_next(iter)) {
        next(iter, data_block);
    }
    if (iter->read_size == inode->st_size) {
        // 最后一个数据块已满，需要重新分配
        short int* datablock_no = (short int*)malloc(sizeof(short int));
        // 获取一个空闲数据块的快号
        if (get_free_datablock_no(datablock_no) == -1) {
            // 无空闲数据块
            printf("[alloc_datablock] Error: there is no free datablock");
            free(datablock_no);
            datablock_no = NULL;
            return -1;
        }
        // 获得了空闲的数据块
        // set_datablock_bitmap_used(*datablock_no); // 设置数据块bitmap
        read_data_block(*datablock_no, data_block);
        inode->addr[iter->index] = *datablock_no;
        free(datablock_no);
        datablock_no = NULL;
    }
    // 最后一个数据块未满，此时datablock已经是最后一个
    free(iter);
    iter = NULL;
    return 0;
}

void test() {
    // 手动为根目录添加目录项
    printf("[test]\n");

    short int* ino_a = (short int*)malloc(sizeof(short int));
    short int* ino_b = (short int*)malloc(sizeof(short int));
    // get_free_ino(ino_a); // 1
    // get_free_ino(ino_b); // 2
    *ino_a = 1;
    *ino_b = 2;
    struct inode* inode_a = (struct inode*)malloc(sizeof(struct inode));
    struct inode* inode_b = (struct inode*)malloc(sizeof(struct inode));

    inode_a->st_mode  = __S_IFDIR | 0755; // 目录文件
    inode_a->st_ino   = *ino_a; // 根目录的inode号为0（第一个）
    inode_a->st_nlink = 1; // 链接引用数
    inode_a->st_uid   = 0; // getuid(); // 拥有者的用户ID，0为超级用户
    inode_a->st_gid   = 0; // getgid(); // 拥有者的组ID，0为超级用户组
    inode_a->st_size  = 0; // 初始大小为空

    inode_b->st_mode  = __S_IFDIR | 0755; // 目录文件
    inode_b->st_ino   = *ino_b; // 根目录的inode号为0（第一个）
    inode_b->st_nlink = 1; // 链接引用数
    inode_b->st_uid   = 0; // getuid(); // 拥有者的用户ID，0为超级用户
    inode_b->st_gid   = 0; // getgid(); // 拥有者的组ID，0为超级用户组
    inode_b->st_size  = 0; // 初始大小为空

    struct entry* a = (struct entry*)malloc(sizeof(struct entry));
    strcpy(a->name, "a");
    strcpy(a->extension, "");
    a->inode = *ino_a;
    a->type = DIR_TYPE;

    struct entry* b = (struct entry*)malloc(sizeof(struct entry));
    strcpy(b->name, "b");
    strcpy(b->extension, "");
    b->inode = *ino_b;
    b->type = DIR_TYPE;

    write_inode(*ino_a, inode_a);
    write_inode(*ino_b, inode_b);

    // 目录inode存放的是entry（子目录或文件）的inode号
    short int* datablock_no = (short int*)malloc(sizeof(short int)); // root_entry的目录项存放的数据块
    // 初始根目录为空，新开一个数据块
    get_free_datablock_no(datablock_no); // 理论上应该是0
    struct data_block* data_block = (struct data_block*)malloc(sizeof(struct data_block));
    read_data_block(*datablock_no, data_block);
    // a和b对应entry存入根目录inode指向数据块中
    memcpy(data_block->data, a, sizeof(struct entry));
    memcpy(data_block->data + sizeof(struct entry), b, sizeof(struct entry));
    write_data_block(*datablock_no, data_block); // 写回磁盘

    // 更新根目录inode记录的文件大小（两个目录项a和b）
    struct inode* root_inode = (struct inode*)malloc(sizeof(struct inode));
    root_inode->addr[0] = *datablock_no;
    read_inode(0, root_inode);
    root_inode->st_size += 2 * sizeof(struct entry);
    write_inode(0, root_inode); // 写回磁盘

}

#endif