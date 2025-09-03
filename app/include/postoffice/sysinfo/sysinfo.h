#ifndef _SYSINFO_H
#define _SYSINFO_H

/** \ingroup sysinfo */
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    unsigned long size_kB;
    long nr;
    long free;
    long overcommit;
    long surplus;
    long reserved;
} hugepage_info_t;

typedef struct {
    int32_t physical_cores;
    int64_t logical_processors;
    int64_t cache_l1;
    int64_t dcache_lnsize;
    int64_t dcache_l1;
    int64_t cache_l2;
    int64_t cache_l3;
    int64_t total_ram;
    int64_t free_ram;
    int64_t page_size;
    hugepage_info_t hugepage_info;
    uint64_t max_open_files;
    uint64_t max_processes;
    uint64_t max_stack_size;
    uint64_t disk_free;
    char fs_type[16];
    int32_t mtu;
    int32_t somaxconn;
    int32_t is_little_endian;
} po_sysinfo_t;

int po_sysinfo_collect(po_sysinfo_t *info);
void po_sysinfo_print(const po_sysinfo_t *info, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // _SYSINFO_H
