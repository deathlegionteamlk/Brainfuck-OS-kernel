#ifndef BF_FS_H
#define BF_FS_H

#include "bf_types.h"
#include <stdio.h>

#define BF_FS_MAX_ENTRIES   512
#define BF_FS_ROOT          "/bfroot"

typedef struct {
    char    path[BF_MAX_PATH];
    char    name[BF_MAX_FILENAME];
    int     is_dir;
    size_t  size;
} bf_dirent_t;

int   bf_fs_init(const char *root);
int   bf_fs_exists(const char *path);
int   bf_fs_isdir(const char *path);
char *bf_fs_read(const char *path, size_t *out_len);
int   bf_fs_write(const char *path, const char *data, size_t len);
int   bf_fs_list(const char *path, bf_dirent_t *out, int max, int *count);
int   bf_fs_mkdir(const char *path);
int   bf_fs_resolve(const char *cwd, const char *input, char *out, size_t outsz);

#endif
