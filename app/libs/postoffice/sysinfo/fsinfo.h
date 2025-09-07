// Internal sysinfo helpers (not installed)
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long free_disk_space(const char *path);
int get_fs_type(const char *path, char *fs_type, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif
