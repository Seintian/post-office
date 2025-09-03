#pragma once
#include <stddef.h>

unsigned long free_disk_space(const char *path);
int get_fs_type(const char *path, char *fs_type, size_t size);
