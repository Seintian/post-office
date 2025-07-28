#include "hugeinfo.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#define HUGEPAGES_FILE "/sys/kernel/mm/hugepages"

static int read_long(const char *path, long *out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[sizeof(long)];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';

    char *endptr;
    *out = strtol(buf, &endptr, 10);
    if (endptr == buf)
        return -1;

    return 0;
}

int list_hugepage_sizes(unsigned long *sizes_kB, size_t max, size_t *count) {
    DIR *d = opendir(HUGEPAGES_FILE);
    if (!d)
        return -1;

    const struct dirent *ent;
    size_t cnt = 0;
    while ((ent = readdir(d)) && cnt < max) {
        unsigned long sz;
        if (sscanf(ent->d_name, "hugepages-%lukB", &sz) == 1)
            sizes_kB[cnt++] = sz;
    }

    closedir(d);
    *count = cnt;
    return 0;
}

int get_hugepage_info(unsigned long size_kB, hugepage_info_t *info) {
    char path[256];

    #define SAFE_SNPRINTF(buf, size, fmt, ...)                      \
        do {                                                        \
            if (snprintf((buf), (size), (fmt), __VA_ARGS__) < 0) {  \
                errno = EINVAL;                                     \
                return -1;                                          \
            }                                                       \
        } while (0)

    #define R(name, field) do {                      \
        SAFE_SNPRINTF(                               \
            path,                                    \
            sizeof(path),                            \
            HUGEPAGES_FILE "/hugepages-%lukB/%s",    \
            size_kB,                                 \
            name                                     \
        );                                           \
        if (read_long(path, &info->field) < 0)       \
            return -1;                               \
    } while (0)

    info->size_kB = size_kB;
    R("nr_hugepages",            nr);
    R("free_hugepages",          free);
    R("nr_overcommit_hugepages", overcommit);
    R("surplus_hugepages",       surplus);
    R("resv_hugepages",          reserved);

    #undef R
    #undef SAFE_SNPRINTF

    return 0;
}
