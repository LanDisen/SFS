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
 * 为inode分配一个新的数据块（会自动寻找空闲数据块）
 * @param inode        需要添加新数据块的inode指针
 * @param datablock_no 返回的空闲数据块号
*/
int alloc_datablock(struct inode* inode, short int* datablock_no) {
    get_free_datablock_no(datablock_no); // 获取空闲数据块进行返回
    // 将该数据块加入到inode
    if (inode->st_size == 0) {
        // inode大小为空，在第一个addr指向新的数据块号
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
            if (inode->addr[index] < 0) {
                // 一级间接未使用，分配存储数据块号的数据块
                short int* no = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no); // 获取空闲数据块
                inode->addr[index] = *no;
                set_datablock_bitmap_used(*no); // 更新bitmap
                // 读取存放数据块号的数据块
                struct data_block* db = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no, db);
                memset(db, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                memcpy(db, datablock_no, sizeof(short int)); // 第一个块号为分配的新数据块号
                // 数据块写回磁盘
                write_data_block(*no, db);
                set_datablock_bitmap_used(*datablock_no); // 更新bitmap
                return 0;
            } else {
                // 读取存放数据块号的数据块
                struct data_block* db = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(inode->addr[index], db);
                int k = 0;
                while (k*sizeof(short int) < BLOCK_SIZE) {
                    short int read_no;
                    memcpy(&read_no, db + k*sizeof(short int), sizeof(short int));
                    if (read_no < 0) {
                        // 这一个数据块未使用，进行替换
                        memcpy(db + k*sizeof(short int), datablock_no, sizeof(short int));
                        set_datablock_bitmap_used(*datablock_no); // 更新bitmap已使用数据块
                        // 写回数据块
                        write_data_block(inode->addr[index], db);
                        return 0;
                    }
                    k++;
                }
            }
            // struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
        } else if (index == 5) {
            // 二级间接索引
            if (inode->addr[index] < 0) {
                // 二级间接未使用，分配存储数据块号的数据块
                short int* no1 = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no1); // 获取空闲数据块
                inode->addr[index] = *no1;
                set_datablock_bitmap_used(*no1); // 更新bitmap
                // 读取存放数据块号的数据块
                struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no1, db1);
                memset(db1, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                short int* no2 = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no2); // 获取空闲数据块
                struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no2, db2);
                memset(db2, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                memcpy(db2, datablock_no, sizeof(short int)); // 第一个块号为分配的新数据块号
                set_datablock_bitmap_used(*no2); // 更新bitmap
                // 数据块写回磁盘
                write_data_block(*no1, db1);
                write_data_block(*no2, db2);
                set_datablock_bitmap_used(*datablock_no); // 更新bitmap
                return 0;
            } else {
                struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(inode->addr[index], db1);
                int k1 = 0;
                while (k1*sizeof(short int) < BLOCK_SIZE) {
                    short int no2;
                    memcpy(&no2, db1 + k1*sizeof(short int), sizeof(short int));
                    struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
                    read_data_block(no2, db2);
                    int k2 = 0;
                    while (k2*sizeof(short int) < BLOCK_SIZE) {
                        short int read_no;
                        memcpy(&read_no, db2 + k2*sizeof(short int), sizeof(short int));
                        if (read_no < 0) {
                            // 这一个数据块未使用，进行替换
                            memcpy(db2 + k2*sizeof(short int), datablock_no, sizeof(short int));
                            set_datablock_bitmap_used(*datablock_no); // 更新bitmap已使用数据块
                            // 写回数据块
                            write_data_block(no2, db2);
                            write_data_block(inode->addr[index], db1);
                            return 0;
                        }
                        k2++;
                    }
                }
            }
        } else if (index == 6) {
            // 三级间接索引
            if (inode->addr[index] < 0) {
                // 三级间接未使用，分配存储数据块号的数据块
                short int* no1 = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no1); // 获取空闲数据块
                inode->addr[index] = *no1;
                set_datablock_bitmap_used(*no1); // 更新bitmap
                // 读取存放数据块号的数据块
                struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no1, db1);
                memset(db1, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                short int* no2 = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no2); // 获取空闲数据块
                struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no2, db2);
                memset(db2, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                memcpy(db2, datablock_no, sizeof(short int)); // 第一个块号为分配的新数据块号
                set_datablock_bitmap_used(*no2); // 更新bitmap
                short int* no3 = (short int*)malloc(sizeof(short int));
                get_free_datablock_no(no3); // 获取空闲数据块
                struct data_block* db3 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(*no3, db3);
                memset(db3, -1, sizeof(struct data_block)); // 设置数据块为全是负数
                memcpy(db3, datablock_no, sizeof(short int)); // 第一个块号为分配的新数据块号
                set_datablock_bitmap_used(*no3); // 更新bitmap
                // 数据块写回磁盘
                write_data_block(*no1, db1);
                write_data_block(*no2, db2);
                write_data_block(*no3, db3);
                set_datablock_bitmap_used(*datablock_no); // 更新bitmap
                return 0;
            } else {
                struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
                read_data_block(inode->addr[index], db1);
                int k1 = 0;
                while (k1*sizeof(short int) < BLOCK_SIZE) {
                    short int no2;
                    memcpy(&no2, db1 + k1*sizeof(short int), sizeof(short int));
                    struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
                    read_data_block(no2, db2);
                    int k2 = 0;
                    while (k2*sizeof(short int) < BLOCK_SIZE) {
                        short int no3;
                        memcpy(&no3, db2 + k2*sizeof(short int), sizeof(short int));
                        struct data_block* db3 = (struct data_block*)malloc(sizeof(struct data_block));
                        read_data_block(no3, db3);
                        int k3 = 0;
                        while (k3*sizeof(short int) < BLOCK_SIZE) {
                            short int read_no;
                            memcpy(&read_no, db2 + k3*sizeof(short int), sizeof(short int));
                            if (read_no < 0) {
                                // 这一个数据块未使用，进行替换
                                memcpy(db3 + k3*sizeof(short int), datablock_no, sizeof(short int));
                                set_datablock_bitmap_used(*datablock_no); // 更新bitmap已使用数据块
                                // 写回数据块
                                write_data_block(no3, db3);
                                write_data_block(no2, db2);
                                write_data_block(inode->addr[index], db1);
                                return 0;
                            }
                            k2++;
                        }
                    }
                }
            }
        }
        index++;
    }
    printf("[alloc_datablock] datablock_no=%d\n", *datablock_no);

    return 0;
}

// 以上是bitmap、inode、数据块相关函数

/**********************/
/* inode迭代器相关函数 */
int has_next(struct inode_iter* iter) {
    if (iter->inode->st_size == 0 || iter->read_size >= iter->inode->st_size || iter->index > 6) {
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
        // 一级间接索引
        short int no1 = iter->inode->addr[iter->index];
        if (!data_block_is_used(no1)) {
            next(iter, data_block);
            return;
        }
        struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no1, db1);
        short int datablock_no;
        memcpy(&datablock_no, db1->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(datablock_no)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        iter->datablock_no = datablock_no;
        read_data_block(datablock_no, data_block);
        iter->n++;
        // 该级索引的块号已遍历完毕
        if (iter->n >= BLOCK_SIZE / sizeof(short int)) {
            iter->index += 1;
            iter->n = 0;
        }
        return;
    } else if (iter->index == 5) {
        // 二级间接索引
        short int no1 = iter->inode->addr[iter->index];
        if (!data_block_is_used(no1)) {
            next(iter, data_block);
            return;
        }
        struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no1, db1);
        short int no2;
        memcpy(&no2, db1->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(no2)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no2, db2);
        short int datablock_no;
        memcpy(&datablock_no, db2->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(datablock_no)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        iter->datablock_no = datablock_no;
        read_data_block(datablock_no, data_block);
        iter->n++;
        // 该级索引的块号已遍历完毕
        if (iter->n >= pow((BLOCK_SIZE / sizeof(short int)), 2)) {
            iter->index += 1;
            iter->n = 0;
        }
        return;

    } else if (iter->index == 6) {
        // 三级间接索引
        short int no1 = iter->inode->addr[iter->index];
        if (!data_block_is_used(no1)) {
            next(iter, data_block);
            return;
        }
        struct data_block* db1 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no1, db1);
        short int no2;
        memcpy(&no2, db1->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(no2)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        struct data_block* db2 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no2, db2);
        short int no3;
        memcpy(&no3, db2->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(no3)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        struct data_block* db3 = (struct data_block*)malloc(sizeof(struct data_block));
        read_data_block(no3, db3);
        short int datablock_no; // 最终获取的数据块号
        memcpy(&datablock_no, db3->data + iter->n*sizeof(short int), sizeof(short int));
        if (!data_block_is_used(datablock_no)) {
            iter->n++;
            next(iter, data_block);
            return;
        }
        iter->datablock_no = datablock_no;
        read_data_block(datablock_no, data_block);
        iter->n++;

        // 该级索引的块号已遍历完毕
        if (iter->n >= pow((BLOCK_SIZE / sizeof(short int)), 3)) {
            iter->index += 1;
            iter->n = 0;
        }
        return;

    }
    printf("[next] out of index\n");
}

/* 以上是inode迭代器相关函数 */

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


/*********************************************/
/* 以下是文件读写相关（read/write）函数 */

// 将inode数据读取到data中
int read_file(struct inode* inode, char* data, size_t size) {
    printf("[read_file] ino=%d\n", inode->st_ino);
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, inode);
    // 遍历inode中的数据块，拷贝到data中
    struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
    int n = 0; // 已读取的数据块数
    int read_size = size;
    read_size = MIN(read_size, inode->st_size); // 由于参数size按块读取（read_size=4096?），设置为不能超过原inode大小
    while (has_next(iter)) {
        int copy_size = MIN(read_size, sizeof(struct data_block));
        read_size -= copy_size;
        next(iter, datablock);
        memcpy(data + n*sizeof(struct data_block), datablock, copy_size);;
        n += 1;
    }
    free(datablock);
    datablock = NULL;
    return 0;
}

// 将data写到inode数据中（不在这里更新inode大小）
int write_file(struct inode* inode, char* data, size_t size) {
    printf("[write_file] ino=%d\n", inode->st_ino);
    printf("[write_file] size=%ld\n", size);
    struct inode_iter* iter = (struct inode_iter*)malloc(sizeof(struct inode_iter));
    new_inode_iter(iter, inode);
    // 遍历inode数据块写入（如果已满需要分配，如果没写完则需要释放）
    struct data_block* datablock = (struct data_block*)malloc(sizeof(struct data_block));
    int n = 0; // 读取的数据块数量
    int write_size = size;
    while (has_next(iter)) {
        next(iter, datablock);
        int copy_size = MIN(write_size, sizeof(struct data_block));
        if (write_size > 0) {
            memcpy(datablock->data, data + n*sizeof(struct data_block), copy_size);
            // 写回磁盘
            write_data_block(iter->datablock_no, datablock);
            write_size -= copy_size;
        } else if (write_size <= 0) {
            // inode其它多余的没写的数据块需要释放
            set_free_datablock_bitmap(iter->datablock_no);
        }
        n++;
    }
    // 遍历完所有数据块也还有需要写的数据，需要分配数据块
    while (write_size > 0)  {
        int short* datablock_no = (int short*)malloc(sizeof(int short));
        alloc_datablock(inode, datablock_no);
        read_data_block(*datablock_no, datablock); // 读取刚分配的数据块
        int copy_size = MIN(write_size, sizeof(struct data_block));
        // 写入数据块
        memcpy(datablock->data, data + n*sizeof(struct data_block), copy_size);
        n++;
        // 写回磁盘
        write_data_block(*datablock_no, datablock);
        write_size -= copy_size;
    }

    return 0;
}

#endif