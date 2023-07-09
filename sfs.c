#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

static int SFS_getattr();
static int SFS_readdir();
static int SFS_mkdir();
static int SFS_rmdir();
static int SFS_mknod();
static int SFS_write();
static int SFS_read();
static int SFS_unlink();

// 超级块（super block）
struct sb {
    long fs_size;  //文件系统的大小，以块为单位
    long first_blk;  //数据区的第一块块号，根目录也放在此
    long datasize;  //数据区大小，以块为单位 
    long first_inode;    //inode区起始块号
    long inode_area_size;   //inode区大小，以块为单位
    long fisrt_blk_of_inodebitmap;   //inode位图区起始块号
    long inodebitmap_size;  // inode位图区大小，以块为单位
    long first_blk_of_databitmap;   //数据块位图起始块号
    long databitmap_size;      //数据块位图大小，以块为单位
};

int main(int argc, char *argv[]) {

    return 0;
}
