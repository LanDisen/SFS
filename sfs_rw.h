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

/**
 * 设置bitmap中inode号为空闲（释放inode）
*/
int set_free_inode_bitmap(short int ino) {
    // 读取inode位图
    uint8_t inode_bitmap[NUM_INODE_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    // bitmap定位
    int row = ino >> 3;
    int col = ino % 8;
    uint8_t byte = inode_bitmap[row];
    uint8_t mask = 0b11111111 - (1 << (7 - col));
    inode_bitmap[row] = byte & mask;
    // 写回磁盘
    fseek(fs, sb->first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    printf("[set_free_inode_bitmap] ino=%d\n", ino);
    return 0;
}

/**
 * 设置bitmap中数据块号为空闲（释放数据块）
*/
int set_free_datablock_bitmap(short int datablock_no) {
    // 读取数据位图
    uint8_t data_block_bitmap[NUM_DATA_BITMAP_BLOCK * BLOCK_SIZE] = {0};
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fread(data_block_bitmap, sizeof(data_block_bitmap), 1, fs);
    // bitmap定位
    int row = datablock_no >> 3;
    int col = datablock_no % 8;
    uint8_t byte = data_block_bitmap[row];
    uint8_t mask = 0b11111111 - (1 << (7 - col));
    data_block_bitmap[row] = byte & mask;
    // 写回磁盘
    fseek(fs, sb->first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    fwrite(data_block_bitmap, sizeof(data_block_bitmap), 1, fs);
    printf("[set_free_datablock_bitmap] datablock_no=%d\n", datablock_no);
    return 0;
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
 * 判断一个数据块有无可用entry，有则返回1，没有则返回0
*/
int datablock_has_entry(short int datablock_no) {
    struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
    read_data_block(datablock_no, datablock);
    int read_size = BLOCK_SIZE;
    struct entry* entry = (struct entry*)malloc(sizeof(struct entry));
    int k = 0; // 待读取的entry下标
    while (read_size > 0) {
        memcpy(entry, datablock->data + k*sizeof(struct entry), sizeof(struct entry));
        if (entry->type != UNUSED) {
            // 存在可用entry
            return 1;
        }
        k++;
        read_size -= sizeof(struct entry);
    }
    // 不存在可用entry
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

#endif