#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "bf_fs.h"

static char fs_root[BF_MAX_PATH] = {0};

int bf_fs_init(const char *root) {
    strncpy(fs_root, root, BF_MAX_PATH - 1);
    struct stat st;
    if (stat(fs_root, &st) != 0) {
        if (mkdir(fs_root, 0755) != 0) return -1;
    }
    char subdir[BF_MAX_PATH];
    snprintf(subdir, sizeof(subdir), "%s/bin", fs_root);
    mkdir(subdir, 0755);
    snprintf(subdir, sizeof(subdir), "%s/lib", fs_root);
    mkdir(subdir, 0755);
    snprintf(subdir, sizeof(subdir), "%s/home", fs_root);
    mkdir(subdir, 0755);
    snprintf(subdir, sizeof(subdir), "%s/tmp", fs_root);
    mkdir(subdir, 0755);
    snprintf(subdir, sizeof(subdir), "%s/etc", fs_root);
    mkdir(subdir, 0755);
    return 0;
}

static void bf_fs_realpath(const char *vpath, char *out, size_t outsz) {
    if (vpath[0] == '/') {
        snprintf(out, outsz, "%s%s", fs_root, vpath);
    } else {
        snprintf(out, outsz, "%s/%s", fs_root, vpath);
    }
}

int bf_fs_exists(const char *path) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));
    struct stat st;
    return stat(real, &st) == 0;
}

int bf_fs_isdir(const char *path) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));
    struct stat st;
    if (stat(real, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

char *bf_fs_read(const char *path, size_t *out_len) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));

    FILE *f = fopen(real, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0 || sz > BF_MAX_PROGRAM) { fclose(f); return NULL; }

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }

    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    if (out_len) *out_len = (size_t)sz;
    return buf;
}

int bf_fs_write(const char *path, const char *data, size_t len) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));

    FILE *f = fopen(real, "wb");
    if (!f) return -1;

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    return (written == len) ? 0 : -1;
}

int bf_fs_list(const char *path, bf_dirent_t *out, int max, int *count) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));

    DIR *d = opendir(real);
    if (!d) return -1;

    struct dirent *de;
    *count = 0;

    while ((de = readdir(d)) != NULL && *count < max) {
        if (de->d_name[0] == '.') continue;

        bf_dirent_t *e = &out[*count];
        strncpy(e->name, de->d_name, BF_MAX_FILENAME - 1);
        snprintf(e->path, BF_MAX_PATH, "%s/%s", path, de->d_name);

        char full[BF_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", real, de->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->size   = (size_t)st.st_size;
        }

        (*count)++;
    }

    closedir(d);
    return 0;
}

int bf_fs_mkdir(const char *path) {
    char real[BF_MAX_PATH];
    bf_fs_realpath(path, real, sizeof(real));
    return mkdir(real, 0755);
}

int bf_fs_resolve(const char *cwd, const char *input, char *out, size_t outsz) {
    if (!input || !input[0]) {
        strncpy(out, cwd, outsz - 1);
        return 0;
    }

    char tmp[BF_MAX_PATH];
    if (input[0] == '/') {
        strncpy(tmp, input, sizeof(tmp) - 1);
    } else {
        snprintf(tmp, sizeof(tmp), "%s/%s", cwd, input);
    }

    char resolved[BF_MAX_PATH];
    int in_len = strlen(tmp);
    int out_pos = 0;
    char *parts[128];
    int nparts = 0;

    char work[BF_MAX_PATH];
    strncpy(work, tmp, sizeof(work) - 1);

    char *tok = strtok(work, "/");
    while (tok) {
        if (strcmp(tok, ".") == 0) {
        } else if (strcmp(tok, "..") == 0) {
            if (nparts > 0) nparts--;
        } else {
            parts[nparts++] = tok;
        }
        tok = strtok(NULL, "/");
    }

    resolved[0] = '/';
    out_pos = 1;
    for (int i = 0; i < nparts; i++) {
        int plen = strlen(parts[i]);
        if (out_pos + plen + 1 >= (int)outsz) return -1;
        if (i > 0) resolved[out_pos++] = '/';
        memcpy(resolved + out_pos, parts[i], plen);
        out_pos += plen;
    }
    resolved[out_pos] = '\0';

    if (out_pos == 1) strcpy(resolved, "/");

    strncpy(out, resolved, outsz - 1);
    return 0;
    (void)in_len;
}
