/*
 * 描述SFS文件系统的相关数据结构（data structures）
*/
#ifndef __SFS_DS_H__
#define __SFS_DS_H__

#include <sys/types.h>
#include <sys/stat.h>

#define FS_SIZE 8*1024*1024  // 文件系统载体文件大小为8MB
#define BLOCK_SIZE 512       // 文件系统使用的块的大小为512字节
#define MAX_PATH_LEN 256     // 路径最大字节长度为256字节
#define MAX_FILE_NAME 8      // 文件名为8个字节
#define MAX_FILE_EXTENSION 3 // 文件扩展名为3个字节

#define NUM_INODE_BITMAP_BLOCK 1 // inode位图大小为1块（512B）
#define NUM_DATA_BITMAP_BLOCK 4  // 数据块位图大小为4块（4 * 512 = 2048 Byte）

// SFS全局变量
// 文件系统载体文件路径，作为该文件系统的根目录
// 虚拟磁盘一行16byte，一个数据块有32行
char* fs_img = "/home/ubuntu/code/SFS/sfs.img";
FILE* fs;              // 文件系统载体文件
struct sb* sb;         // 超级块作为SFS文件系统的全局变量
struct entry* root_entry;  // 根目录
struct entry* work_entry;  // 工作目录

/*
 * 超级块（super block），用于描述整个文件系统
 * 超级块大小为72字节（9个long），其占用1个磁盘块
 * 虚拟磁盘（sfs.img）
 * inode bitmap: 0x200
 * data bitmap:  0x400
 * inode area:   0x
 * data area:    0x200C00
*/
struct sb {
    long fs_size;                  // 文件系统的大小，以块为单位（16*1024=16384块）
    long first_blk;                // 数据区的第一块块号，根目录也放在此（4102）
    long datasize;                 // 数据区大小，以块为单位（4*512*8） 
    long first_inode;              // inode区起始块号（6）
    long inode_area_size;          // inode区大小，以块为单位（512*8）
    long first_blk_of_inodebitmap; // inode位图区起始块号（1）
    long inodebitmap_size;         // inode位图区大小，以块为单位（1）
    long first_blk_of_databitmap;  // 数据块位图起始块号（2）
    long databitmap_size;          // 数据块位图大小，以块为单位（4）
};

/*
 * SFS文件系统采用inode方式管理文件，具体而言：
 * 空闲块和空闲inode均采用位图的方式管理
 * 文件数据块采用直接和间接索引的方式，支持多级目录
*/
struct inode {
    short int st_mode;       // 权限，2字节
    short int st_ino;        // inode号，2字节
    char st_nlink;           // 连接数，1字节
    uid_t st_uid;            // 拥有者的用户ID，4字节
    gid_t st_gid;            // 拥有者的组ID，4字节
    off_t st_size;           // 文件大小，4字节
    struct timespec st_atim; // 上次访问时间（time of last access），16字节
    // addr磁盘地址有7个，其中addr[0]-addr[3]是直接地址
    // addr[4]、addr[5]、addr[6]分别为一次、二次、三次间接索引
    short int addr[7];       // 磁盘地址，14字节
};

/*
 * entry为SFS文件系统的目录项
 * 在SFS中，目录也被作为文件，只不过这个文件存放的是一个的目录项
 * 每个目录项的大小为16字节，格式如下：
 * 文件名8字节，扩展名3字节，inode号2字节（实际使用12位），备用3字节
*/
#define UNUSED 0
#define FILE_TYPE 1 // 普通文件
#define DIR_TYPE 2  // 目录文件

struct entry {
    char name[MAX_FILE_NAME + 1];           // 文件名，8+1字节
    char extension[MAX_FILE_EXTENSION + 1]; // 文件扩展名，3+1字节
    char type; // 目录项类型（0未使用，1普通文件，2目录文件）
    short int inode;                        // inode号，2字节
    // 备用3字节，其中1字节用于判断目录项类型 
    // char reserved[2];
    // 备用2字节分别用于为name和ext字段补'\0'
};

// 多级目录结构
struct dir {
    struct entry* entries[100];
    size_t num_entries;
};

/*
 * 数据块
*/
struct data_block {
    // char data[BLOCK_SIZE - 4]; // sizeof(size_t) = 4
    // size_t size; // 该磁盘块实际占用的字节大小，不超过512字节
    char data[BLOCK_SIZE];
};

/**
 * inode迭代器，一次取出一个数据块
*/
struct inode_iter {
    struct inode* inode;
    size_t read_size; // 已读取数据块大小
    int index;        // 当前索引下标
    short int datablock_no; // 当前迭代的数据块号
};

// 以上是SFS相关数据结构
// ***************************************************************************************
// 以下是SFS数据结构（inode、entry等）初始化函数

void new_inode(struct inode* inode, short int ino, char type) {
    inode->st_ino = ino;
    if (type == DIR_TYPE) {
        inode->st_mode = __S_IFDIR | 0755; // 目录文件
    } else if (type == FILE_TYPE) {
        inode->st_mode = __S_IFREG | 0444; // 普通文件
    }
    inode->st_nlink = 1; // 链接数
    inode->st_uid = 0;   // 用户id
    inode->st_gid = 0;   // 用户组id
    inode->st_size = 0;  // 文件大小
    // inode->st_atim = time(NULL); // 上次访问时间（time of last access）
}

void new_entry(struct entry* entry, char* name, char* ext, char type, short int ino) {
    entry->type = type;
    entry->inode = ino;
    strcpy(entry->name, name);
    strcpy(entry->extension, ext);
    if (type == DIR_TYPE) {
        strcpy(entry->extension, "");
    }
}

void new_inode_iter(struct inode_iter* iter, struct inode* inode) {
    iter->inode = inode;
    iter->read_size = 0;
    iter->index = 0;
    iter->datablock_no = -1; // 当前迭代的数据块号
}

// 以上是SFS数据结构（inode、entry等）初始化函数
#endif