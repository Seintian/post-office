#ifndef _FSINFO_H
#define _FSINFO_H

#include <stddef.h>
#include <stdio.h>


unsigned long free_disk_space(const char *path) __nonnull((1));

int get_fs_type(const char *path, char *fs_type, size_t size) __nonnull((1, 2));

#endif // _FSINFO_H
