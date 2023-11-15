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
    strncpy(parent, path, k);
}

/**
 * 获取路径最后的文件名
 * example: path="abc/de/fgh" -> file_name="fgh"
*/
void get_file_name(const char* path, char* file_name) {
    char* path_copy = (char*)malloc(sizeof(path));
    strcpy(path_copy, path);
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

#endif