#define _POSIX_C_SOURCE 200809L
#include "utils/files.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool fs_exists(const char *path) {
    if (!path)
        return false;

    struct stat st;
    return stat(path, &st) == 0;
}

bool fs_is_regular_file(const char *path) {
    if (!path)
        return false;

    struct stat st;
    if (stat(path, &st) != 0)
        return false;

    return S_ISREG(st.st_mode);
}

bool fs_is_directory(const char *path) {
    if (!path)
        return false;

    struct stat st;
    if (stat(path, &st) != 0)
        return false;

    return S_ISDIR(st.st_mode);
}

bool fs_is_socket(const char *path) {
    if (!path)
        return false;

    struct stat st;
    if (lstat(path, &st) != 0)
        return false;

    return S_ISSOCK(st.st_mode);
}

char *fs_read_file_to_buffer(const char *path, size_t *out_size) {
    if (out_size)
        *out_size = 0;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    size_t usize = (size_t)sz;
    char *buf = (char *)malloc(usize + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t rd = fread(buf, 1, usize, f);
    fclose(f);
    if (rd != usize) {
        free(buf);
        return NULL;
    }

    buf[usize] = '\0';
    if (out_size)
        *out_size = usize;

    return buf;
}

bool fs_write_buffer_to_file(const char *path, const void *buffer, size_t size) {
    if (!path || (!buffer && size > 0)) {
        errno = EINVAL;
        return false;
    }

    FILE *f = fopen(path, "wb");
    if (!f)
        return false;

    size_t wr = fwrite(buffer, 1, size, f);
    int rc = fflush(f);
    int ec = ferror(f);
    fclose(f);

    return wr == size && rc == 0 && ec == 0;
}

static bool mkdir_one(const char *path, mode_t mode) {
    if (mkdir(path, mode) == 0)
        return true;

    if (errno == EEXIST)
        return fs_is_directory(path);

    return false;
}

bool fs_create_directory_recursive(const char *path, mode_t mode) {
    if (!path || !*path) {
        errno = EINVAL;
        return false;
    }

    // duplicate and iterate over components
    char *tmp = strdup(path);
    if (!tmp)
        return false;

    size_t len = strlen(tmp);
    // strip trailing slashes (except root "/")
    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (!mkdir_one(tmp, mode)) {
                free(tmp);
                return false;
            }
            *p = '/';
        }
    }

    bool ok = mkdir_one(tmp, mode);
    free(tmp);
    return ok;
}

char *fs_path_join(const char *base, const char *leaf) {
    if (!base && !leaf) {
        errno = EINVAL;
        return NULL;
    }
    if (!base)
        return strdup(leaf);
    if (!leaf)
        return strdup(base);

    size_t bl = strlen(base);
    size_t ll = strlen(leaf);
    bool need_sep = bl > 0 && base[bl - 1] != '/';

    size_t total = bl + (need_sep ? 1 : 0) + ll + 1;
    char *out = (char *)malloc(total);
    if (!out)
        return NULL;

    if (need_sep)
        snprintf(out, total, "%s/%s", base, leaf);

    else
        snprintf(out, total, "%s%s", base, leaf);

    return out;
}
