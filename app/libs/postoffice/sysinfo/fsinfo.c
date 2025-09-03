#include "fsinfo.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

#define MOUNTS_FILE "/proc/mounts"

unsigned long free_disk_space(const char *path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0)
        return 0UL;
    return stat.f_frsize * stat.f_bavail;
}

int get_fs_type(const char *path, char *fs_type, size_t size) {
    FILE *fp = fopen(MOUNTS_FILE, "r");
    if (!fp)
        return -1;

    char dev[256];
    char mount[256];
    char type[64];
    char opts[256];
    int rc = -1;
    while (fscanf(fp, "%255s %255s %63s %255s %*d %*d\n", dev, mount, type, opts) == 4) {
        if (strncmp(path, mount, strlen(mount)) == 0) {
            strncpy(fs_type, type, size - 1);
            fs_type[size - 1] = '\0';
            rc = 0;
            break;
        }
    }
    fclose(fp);
    if (rc != 0)
        errno = ENOENT;
    return rc;
}
