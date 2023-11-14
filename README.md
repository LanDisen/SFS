# SFS

SFS（Simple File System）是一个简单的类UFS文件系统。

## How to use?

创建一个8M大小的虚拟磁盘

```bash
make img
```

编译执行

```bash
mkdir build
make
cd build
```

在build目录内创建一个空文件夹用于挂载文件系统

```bash
mkdir fuse
```

挂载文件系统（若未初始化虚拟磁盘该步骤会自动初始化）

```bash
./sfs -d fuse
```

卸载文件系统

```bash
fusermount -u fuse
```

## Tips

VSCode安装Hex Editor插件，右键点击sfs.img选择打开方式，选择Hex Editor，就可以查看该虚拟磁盘映像文件的内容，方便调试。
![image](img/sfsimg.png)