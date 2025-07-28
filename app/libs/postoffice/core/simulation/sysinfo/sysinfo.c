#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "core/simulation/sysinfo.h"
#include "utils/configs.h"
#include "utils/errors.h"
#include "hugeinfo.h"
#include "fsinfo.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <fcntl.h>


#define CPUINFO_FILE    "/proc/cpuinfo"
#define MEMINFO_FILE    "/proc/meminfo"
#define NETINFO_FILE    "/proc/net/dev"
#define OSINFO_FILE     "/etc/os-release"
#define SOMAXCONN_FILE  "/proc/sys/net/core/somaxconn"


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
        info->hugepage_info = (hugepage_info_t){0}; // Huge pages not supported

    for (size_t i = 0; i < count; i++) {
        hugepage_info_t huge_info;
        if (get_hugepage_info(sizes_kB[i], &huge_info) == 0) {
            if (huge_info.size_kB > info->hugepage_info.size_kB)
                info->hugepage_info = huge_info;
        }
        else fprintf(
            stderr,
            "Failed to get hugepage info for size %lu kB: %s\n",
            sizes_kB[i],
            po_strerror(errno)
        );
    }

    po_config_free(&cfg);
    return 0;
}

static int load_resource_limits(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
        return -1;
    info->max_open_files = rl.rlim_cur;

    if (getrlimit(RLIMIT_NPROC, &rl) != 0)
        return -1;
    info->max_processes = rl.rlim_cur;

    if (getrlimit(RLIMIT_STACK, &rl) != 0)
        return -1;
    info->max_stack_size = rl.rlim_cur;

    return 0;
}

static int load_filesysteminfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    info->disk_free = free_disk_space("/");
    if (info->disk_free == 0UL && errno != 0)
        return -1;

    if (get_fs_type("/", info->fs_type, sizeof(info->fs_type)) != 0)
        return -1;

    return 0;
}

static int load_networkinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

    struct ifreq ifr;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    int rc = ioctl(fd, SIOCGIFMTU, &ifr);
    close(fd);
    if (rc < 0)
        return -1;

    info->mtu = ifr.ifr_mtu;
    return 0;
}

static int load_kernelinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    int fd = open(SOMAXCONN_FILE, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[sizeof(int)];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';
    info->somaxconn = atoi(buf);

    return 0;
}

int po_sysinfo_collect(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    if (load_cpuinfo(info) != 0)
        return -1;

    if (load_memoryinfo(info) != 0)
        return -1;

    if (load_resource_limits(info) != 0)
        return -1;

    if (load_filesysteminfo(info) != 0)
        return -1;

    if (load_networkinfo(info) != 0)
        return -1;

    if (load_kernelinfo(info) != 0)
        return -1;

    info->is_little_endian = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;

    return 0;
}

void po_sysinfo_print(const po_sysinfo_t *info, FILE *out) {
    if (!info || !out)
        return;

    fprintf(out, "System Information:\n");
    fprintf(out, "  Physical Cores: %d\n",         info->physical_cores);
    fprintf(out, "  Logical Processors: %ld\n",    info->logical_processors);
    fprintf(out, "  L1 Cache Size: %zu bytes\n",   info->cache_l1);
    fprintf(out, "  L2 Cache Size: %zu bytes\n",   info->cache_l2);
    fprintf(out, "  L3 Cache Size: %zu bytes\n",   info->cache_l3);
    fprintf(out, "  Total RAM: %zu bytes\n",       info->total_ram);
    fprintf(out, "  Free RAM: %zu bytes\n",        info->free_ram);
    fprintf(out, "  Page Size: %zu bytes\n",       info->page_size);
    fprintf(out, "  Huge Page Size: %lu kB\n",     info->hugepage_info.size_kB);
    fprintf(out, "  Number of Huge Pages: %ld\n",  info->hugepage_info.nr);
    fprintf(out, "  Free Huge Pages: %ld\n",       info->hugepage_info.free);
    fprintf(out, "  Overcommit Huge Pages: %ld\n", info->hugepage_info.overcommit);
    fprintf(out, "  Surplus Huge Pages: %ld\n",    info->hugepage_info.surplus);
    fprintf(out, "  Reserved Huge Pages: %ld\n",   info->hugepage_info.reserved);
    fprintf(out, "  Max Open Files: %ld\n",        info->max_open_files);
    fprintf(out, "  Max Processes: %ld\n",         info->max_processes);
    fprintf(out, "  Max Stack Size: %ld bytes\n",  info->max_stack_size);
    fprintf(out, "  Free Disk Space: %zu bytes\n", info->disk_free);
    fprintf(out, "  Filesystem Type: %s\n",        info->fs_type);
    fprintf(out, "  MTU: %d\n",                    info->mtu);
    fprintf(out, "  Somaxconn: %d\n",              info->somaxconn);
    fprintf(out, "  Is Little Endian: %d\n",       info->is_little_endian);
}
