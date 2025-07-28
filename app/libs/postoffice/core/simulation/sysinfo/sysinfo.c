#include "core/simulation/sysinfo.h"
#include "utils/configs.h"
#include "utils/errors.h"
#include "hugeinfo.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define CPUINFO_FILE  "/proc/cpuinfo"
#define MEMINFO_FILE  "/proc/meminfo"
#define DISKINFO_FILE "/proc/diskstats"
#define NETINFO_FILE  "/proc/net/dev"
#define OSINFO_FILE   "/etc/os-release"


static int load_cpuinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    po_config_t *cfg;
    if (po_config_load(CPUINFO_FILE, &cfg) != INIH_EOK) {
        fprintf(stderr, "Failed to load configuration from file '%s': '%s'\n", CPUINFO_FILE, po_strerror(errno));
        po_config_free(&cfg);
        return -1;
    }

    if (po_config_get_int(cfg, NULL, "cpu cores", &info->physical_cores) != 0) {
        po_config_free(&cfg);
        return -1;
    }

    info->logical_processors = sysconf(_SC_NPROCESSORS_ONLN);

    info->cache_l1 = sysconf(_SC_LEVEL1_ICACHE_SIZE);
    info->dcache_l1 = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    info->cache_l2 = sysconf(_SC_LEVEL2_CACHE_SIZE);
    info->cache_l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);

    po_config_free(&cfg);
    return 0;
}

static long parse_meminfo_value(const char *value) {
    // Ignore the last part (e.g., "kB", "MB")
    size_t len = strlen(value);
    if (len < 2) {
        errno = EINVAL;
        return -1;
    }

    // Find the last space to separate the numeric part
    const char *last_space = strrchr(value, ' ');
    if (!last_space || last_space == value) {
        errno = EINVAL;
        return -1;
    }

    // Convert the numeric part to long
    size_t num_len = (size_t)(last_space - value);
    if (num_len == 0 || num_len >= len) {
        errno = EINVAL;
        return -1;
    }

    char *buf = malloc(num_len + 1);
    if (!buf)
        return -1;

    strncpy(buf, value, num_len);
    buf[num_len] = '\0';

    char *endptr;
    long val = strtol(buf, &endptr, 10);
    if (*endptr != ' ' && *endptr != '\0') {
        free(buf);
        return -1;
    }

    free(buf);
    return val * 1024L;
}

static int load_memoryinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    po_config_t *cfg;
    if (po_config_load(MEMINFO_FILE, &cfg) != INIH_EOK) {
        po_config_free(&cfg);
        return -1;
    }

    const char *value;
    if (po_config_get_str(cfg, NULL, "MemTotal", &value) != 0) {
        po_config_free(&cfg);
        return -1;
    }
    info->total_ram = parse_meminfo_value(value);

    if (po_config_get_str(cfg, NULL, "MemFree", &value) != 0) {
        po_config_free(&cfg);
        return -1;
    }
    info->free_ram = parse_meminfo_value(value);

    info->page_size = sysconf(_SC_PAGESIZE);

    unsigned long sizes_kB[8];
    size_t count;
    if (list_hugepage_sizes(sizes_kB, 8, &count) != 0)
        info->hugepage_size = 0; // Huge pages not supported

    size_t max_size = sizes_kB[0];
    for (size_t i = 0; i < count; i++) {
        if (sizes_kB[i] > max_size)
            max_size = sizes_kB[i];

        struct hugepage_info huge_info;
        if (get_hugepage_info(sizes_kB[i], &huge_info) == 0) {
            printf("> Hugepage size: %lu kB\n", huge_info.size_kB);
            printf("  - Total: %ld\n", huge_info.nr);
            printf("  - Free: %ld\n", huge_info.free);
            printf("  - Overcommit: %ld\n", huge_info.overcommit);
            printf("  - Surplus: %ld\n", huge_info.surplus);
            printf("  - Reserved: %ld\n", huge_info.reserved);
        }
        else {
            fprintf(stderr, "Failed to get hugepage info for size %lu kB: %s\n", sizes_kB[i], po_strerror(errno));
        }
    }
    info->hugepage_size = max_size * 1024;

    po_config_free(&cfg);
    return 0;
}

int po_sysinfo_collect(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    if (load_cpuinfo(info) != 0) {
        fprintf(stderr, "Failed to load CPU info: %s\n", po_strerror(errno));
        return -1; // Error loading CPU info
    }

    if (load_memoryinfo(info) != 0) {
        fprintf(stderr, "Failed to load memory info: %s\n", po_strerror(errno));
        return -1; // Error loading memory info
    }

    return 0;
}

void po_sysinfo_print(const po_sysinfo_t *info, FILE *out) {
    if (!info || !out)
        return; // Nothing to print

    fprintf(out, "System Information:\n");
    fprintf(out, "Physical Cores: %d\n", info->physical_cores);
    fprintf(out, "Logical Processors: %ld\n", info->logical_processors);
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
