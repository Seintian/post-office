#include "core/simulation/sysinfo.h"
#include "utils/configs.h"
#include "utils/errors.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#define CPUINFO_FILE  "/proc/cpuinfo"
#define MEMINFO_FILE  "/proc/meminfo"
#define DISKINFO_FILE "/proc/diskstats"
#define NETINFO_FILE  "/proc/net/dev"
#define OSINFO_FILE   "/etc/os-release"

/*
    int      physical_cores;      Number of physical CPU cores
    int      logical_processors;  Number of logical processors (threads)
    int      numa_nodes;          Number of NUMA nodes

    size_t   cache_l1;            L1 cache size per core
    size_t   cache_l2;            L2 cache size per core
    size_t   cache_l3;            L3 cache size per NUMA node
*/
static int load_cpuinfo(po_sysinfo_t *info) {
    if (info == NULL) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    po_config_t *cfg;
    if (po_config_load(CPUINFO_FILE, &cfg) != INIH_EOK) {
        fprintf(stderr, "Failed to load configuration from file '%s': '%s'\n", CPUINFO_FILE, po_strerror(errno));
        return -1;
    }

    // Extract CPU topology
    const char *value;
    if (po_config_get_str(cfg, NULL, "cpu cores", &value) != 0) {
        fprintf(stderr, "Missing 'cpu cores' in '%s'\n", CPUINFO_FILE);
        po_config_free(&cfg);
        return -1; // Missing key
    }

    char *end;
    info->physical_cores = (int) strtoul(value, &end, 10);
    if (*end != '\0') {
        fprintf(stderr, "Invalid 'cpu cores' value in %s\n", CPUINFO_FILE);
        po_config_free(&cfg);
        return -1; // Invalid value
    }

    po_config_free(&cfg);
    return 0;
}

int po_sysinfo_collect(po_sysinfo_t *info) {
    if (info == NULL) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    if (load_cpuinfo(info) != 0) {
        fprintf(stderr, "Failed to load CPU info: %s\n", po_strerror(errno));
        return -1; // Error loading CPU info
    }

    return 0;
}

void po_sysinfo_print(const po_sysinfo_t *info, FILE *out) {
    if (info == NULL || out == NULL)
        return; // Nothing to print

    fprintf(out, "System Information:\n");
    fprintf(out, "Physical Cores: %d\n", info->physical_cores);
    fprintf(out, "Logical Processors: %d\n", info->logical_processors);
    fprintf(out, "NUMA Nodes: %d\n", info->numa_nodes);
    fprintf(out, "L1 Cache Size: %zu bytes\n", info->cache_l1);
    fprintf(out, "L2 Cache Size: %zu bytes\n", info->cache_l2);
    fprintf(out, "L3 Cache Size: %zu bytes\n", info->cache_l3);
    fprintf(out, "Total RAM: %zu bytes\n", info->total_ram);
    fprintf(out, "Free RAM: %zu bytes\n", info->free_ram);
    fprintf(out, "Page Size: %zu bytes\n", info->page_size);
    fprintf(out, "Huge Page Size: %zu bytes\n", info->hugepage_size);
    fprintf(out, "Max Open Files: %ld\n", info->max_open_files);
    fprintf(out, "Max Processes: %ld\n", info->max_processes);
    fprintf(out, "Max Stack Size: %ld bytes\n", info->max_stack_size);
    fprintf(out, "Free Disk Space: %zu bytes\n", info->disk_free);
    fprintf(out, "Filesystem Type: %s\n", info->fs_type);
    fprintf(out, "MTU: %d\n", info->mtu);
    fprintf(out, "Somaxconn: %d\n", info->somaxconn);
    fprintf(out, "Is Little Endian: %d\n", info->is_little_endian);
}
