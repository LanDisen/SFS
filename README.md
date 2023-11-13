# SFS

SFS（Simple File System）是一个简单的类UFS文件系统。

## How to use?

创建一个8M大小的映像文件用于挂载文件系统

```bash
dd bs=1K count=8K if=/dev/zero of=sfs.img
```

初始化该映像文件
```
make
./sfs
```

创建一个文件夹用于挂载文件系统

```bash
mkdir testmount
```

挂载文件系统

```bash
./sfs -d testmount
```

取消文件系统挂载

```bash
fusermount -u testmount
```