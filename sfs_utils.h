/**
 * SFS文件系统相关辅助函数
*/
#ifndef __SFS_UTILS_H__
#define __SFS_UTILS_H__
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#include "sfs_ds.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * 分割路径
 * example: path="abc/ef/g" -> head="abc", tail="ef/g"
 *          path="abc/ef"   -> head="abc", tail="ef"
 *          path="abc"      -> head="abc", tail=""
 *          path=""         -> head="", tail=""
*/
void split_path(const char* path, char* head, char* tail) {
    if (strstr(path, "/") == NULL) {
        // path中没有分隔符
        strcpy(head, path);
        strcpy(tail, "");
        return;
    }
    if (path == NULL || strcmp(path, "") == 0) {
        strcpy(head, "");
        strcpy(tail, "");
        return;
    }
    char* path_copy = (char*)malloc(sizeof(path));
    strcpy(path_copy, path);
    strcpy(head, path);
    strtok(head, "/");
    strcpy(tail, path_copy + strlen(head) + 1);
    free(path_copy);
}

/**
 * 将src_path添加到dest_path末尾
 * example: dest_path="/abc", src_path="efg" -> "/abc/efg"
 *          dest_path="/",    src_path="ab"  -> "/ab"
*/
void concat_path(char* dest_path, char* src_path) {
    if (strcmp(dest_path, "/") != 0) {
        strcat(dest_path, "/");
    }
    strcat(dest_path, src_path);
}

/**
 * 计算目录层级（本质是'/'的数量，目前认为path第一个字符一定是'/'）
 * example: path="/abc/ef/g" -> 3
*/
int path_level(const char* path) {
    int cnt = 0; // 统计目录层级
    if (strcmp(path, "/") == 0) {
        return 0; // 根目录为0
    }
    int n = strlen(path);
    for (int i=0; i<n; i++) {
        if (path[i] == '/') {
            cnt++;
        }
    }
    return cnt;
}

/**
 * 获得上一级目录路径
 * example: path="/abc/de/f" -> parent="/abc/de"
 *          path="/abc" -> parent="/"
 *          path="/" -> parent="/"
*/
void get_parent_path(const char* path, char* parent) {
    if (strcmp(path, "/") == 0) {
        strcpy(parent, "/");
        return;
    }
    int n = strlen(path);
    int k = -1;
    for (int i=n-1; i>=0; i--) {
        if (path[i] == '/') {
            k = i;
            break;
        }
    }
    if (k == 0) {
        // 第一个"/"出现在第一个字符，如"/a"，则parent为根目录
        strcpy(parent, "/");
        return;
    }
    strncpy(parent, path, k);
    parent[k] = '\0';
}

/**
 * 获取路径最后的文件名
 * example: path="abc/de/fgh" -> file_name="fgh"
 *          path="/a" -> file_name="a"
*/
void get_file_name(const char* path, char* file_name) {
    char* path_copy = (char*)malloc(sizeof(path));
    strcpy(path_copy, path);
    if (path_copy[0] == '/') {
        strcpy(path_copy, path + 1);
    }
    char* head = (char*)malloc(sizeof(path));
    char* tail = (char*)malloc(sizeof(path));
    do {
        split_path(path_copy, head, tail);
        strcpy(path_copy, tail);
    } while (strcmp(tail, "") != 0);
    strcpy(file_name, head);
    free(head);
    free(tail);
}

/**
 * 分割文件名和扩展名（从右到左第一个分隔符开始分割）
 * @param file  文件完整名
 * @param fname 文件名
 * @param ext   扩展名
 * @example file="file.txt" -> fname="file", ext="txt"
*/
void fname_ext(const char* file, char fname[], char ext[]) {
    int n = strlen(file);
    int k = -1;
    for (int i=n-1; i>=0; i--) {
        if (file[i] == '.') {
            k = i;
            break;
        }
    }
    if (k == -1) {
        // 没有分隔符
        strcpy(fname, file);
        strcpy(ext, "");
    } else {
        // 存在分隔符
        memcpy(fname, file, k*sizeof(char));
        fname[k] = '\0';
        strcpy(ext, file + k + 1);
        ext[MAX_FILE_EXTENSION] = '\0';
    }
}

/**
 * 组合文件名和扩展名为完整文件名
 * @param fname 文件名
 * @param ext   扩展名
 * @param file  文件完整名
 * @example fname="file", ext="txt" -> file="file.txt"
*/
void full_name(const char fname[], const char ext[], char* file) {
    if (strcmp(ext, "") == 0) {
        // 扩展名为空
        strcpy(file, fname);
        return;
    }
    int n1 = strlen(fname);
    int n2 = strlen(ext);
    memcpy(file, fname, n1*sizeof(char));
    memcpy(file + n1*sizeof(char), ".", sizeof(char));
    memcpy(file + (n1+1)*sizeof(char), ext, n2*sizeof(char));
}

#endif