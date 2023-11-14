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

#endif