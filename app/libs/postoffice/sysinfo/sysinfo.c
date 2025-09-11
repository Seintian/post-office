#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "sysinfo/sysinfo.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fsinfo.h"
#include "hugeinfo.h"
#include "utils/errors.h"

#define CPUINFO_FILE "/proc/cpuinfo"
#define MEMINFO_FILE "/proc/meminfo"
#define NETINFO_FILE "/proc/net/dev"
#define OSINFO_FILE "/etc/os-release"
#define SOMAXCONN_FILE "/proc/sys/net/core/somaxconn"

static int load_cpuinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    // Logical processors
#ifdef _SC_NPROCESSORS_ONLN
    long lp = sysconf(_SC_NPROCESSORS_ONLN);
    if (lp < 1) {
        lp = 1;
    }
    info->logical_processors = lp;
#else
    info->logical_processors = 1;
#endif

    // Physical cores: try to parse /proc/cpuinfo for "cpu cores\t: <n>"
    int phys = 0;
    FILE *f = fopen(CPUINFO_FILE, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu cores", 9) == 0) {
                const char *colon = strchr(line, ':');
                if (colon) {
                    int v = atoi(colon + 1);
                    if (v > 0) {
                        phys = v;
                        break;
                    }
                }
            }
        }

        fclose(f);
    }

    if (phys <= 0)
        phys = (int)info->logical_processors;
    info->physical_cores = phys;

    // Cache/sysconf values (best-effort)
#ifdef _SC_LEVEL1_ICACHE_SIZE
    info->cache_l1 = sysconf(_SC_LEVEL1_ICACHE_SIZE);
#else
    info->cache_l1 = -1;
#endif
#ifdef _SC_LEVEL1_DCACHE_SIZE
    info->dcache_l1 = sysconf(_SC_LEVEL1_DCACHE_SIZE);
#else
    info->dcache_l1 = -1;
#endif
#ifdef _SC_LEVEL1_DCACHE_LINESIZE
    info->dcache_lnsize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#else
    info->dcache_lnsize = -1;
#endif
#ifdef _SC_LEVEL2_CACHE_SIZE
    info->cache_l2 = sysconf(_SC_LEVEL2_CACHE_SIZE);
#else
    info->cache_l2 = -1;
#endif
#ifdef _SC_LEVEL3_CACHE_SIZE
    info->cache_l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);
#else
    info->cache_l3 = -1;
#endif

    return 0;
}

static long parse_meminfo_value_kb_to_bytes(long kb) {
    if (kb < 0)
        return -1;

    return kb * 1024L;
}

static int load_memoryinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    long total_kb = -1, free_kb = -1;
    FILE *f = fopen(MEMINFO_FILE, "r");
    if (f) {
        char key[64];
        long val;
        char unit[16];
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%63[^:]: %ld %15s", key, &val, unit) >= 2) {
                if (strcmp(key, "MemTotal") == 0)
                    total_kb = val;

                else if (strcmp(key, "MemFree") == 0)
                    free_kb = val;
            }
        }

        fclose(f);
    }

    if (total_kb < 0 || free_kb < 0)
        return -1;

    info->total_ram = parse_meminfo_value_kb_to_bytes(total_kb);
    info->free_ram = parse_meminfo_value_kb_to_bytes(free_kb);

#ifdef _SC_PAGESIZE
    info->page_size = sysconf(_SC_PAGESIZE);
#else
    info->page_size = 4096;
#endif

    // Hugepages best-effort
    info->hugepage_info = (po_hugepage_info_t){0};
    unsigned long sizes_kB[8] = {0};
    size_t count = 0;
    if (list_hugepage_sizes(sizes_kB, 8, &count) == 0) {
        for (size_t i = 0; i < count; i++) {
            po_hugepage_info_t huge_info;
            if (get_hugepage_info(sizes_kB[i], &huge_info) == 0) {
                if (huge_info.size_kB > info->hugepage_info.size_kB)
                    info->hugepage_info = huge_info;
            }
        }
    }

    return 0;
}

static int load_resource_limits(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
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
        return -1;
    }

    // Choose first non-loopback interface from /proc/net/dev
    char chosen[IFNAMSIZ] = {0};
    FILE *f = fopen(NETINFO_FILE, "r");
    if (f) {
        char line[512];
        int line_no = 0;
        while (fgets(line, sizeof(line), f)) {
            line_no++;
            if (line_no <= 2)
                continue; // skip headers

            char *colon = strchr(line, ':');
            if (!colon)
                continue;

            // interface name is before ':' possibly with spaces
            char name[IFNAMSIZ] = {0};
            size_t len = 0;
            char *p = line;
            while (*p == ' ')
                p++;

            while (p[len] && p[len] != ':' && len < IFNAMSIZ - 1)
                len++;

            strncpy(name, p, len);
            name[len] = '\0';
            if (strcmp(name, "lo") != 0) {
                strncpy(chosen, name, IFNAMSIZ - 1);
                break;
            }
        }

        fclose(f);
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        info->mtu = 0;
        return 0;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (chosen[0] == '\0')
        strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);

    else
        strncpy(ifr.ifr_name, chosen, IFNAMSIZ - 1);

    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(fd, SIOCGIFMTU, &ifr) == 0)
        info->mtu = ifr.ifr_mtu;

    else
        info->mtu = 0;

    close(fd);
    return 0; // best-effort
}

static int load_kernelinfo(po_sysinfo_t *info) {
    if (!info) {
        errno = EINVAL;
        return -1;
    }

    int fd = open(SOMAXCONN_FILE, O_RDONLY);
    if (fd < 0) {
        info->somaxconn = 0;
        return 0;
    }

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        info->somaxconn = 0;
        return 0;
    }

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
    fprintf(out, "  Physical Cores: %d\n",              info->physical_cores);
    fprintf(out, "  Logical Processors: %ld\n",         info->logical_processors);
    fprintf(out, "  L1 Cache Size: %ld bytes\n",        info->cache_l1);
    fprintf(out, "  Data Cache L1 Size: %ld bytes\n",   info->dcache_l1);
    fprintf(out, "  Data Cache Line Size: %ld bytes\n", info->dcache_lnsize);
    fprintf(out, "  L2 Cache Size: %ld bytes\n",        info->cache_l2);
    fprintf(out, "  L3 Cache Size: %ld bytes\n",        info->cache_l3);
    fprintf(out, "  Total RAM: %ld bytes\n",            info->total_ram);
    fprintf(out, "  Free RAM: %ld bytes\n",             info->free_ram);
    fprintf(out, "  Page Size: %ld bytes\n",            info->page_size);
    fprintf(out, "  Huge Page Size: %lu kB\n",          info->hugepage_info.size_kB);
    fprintf(out, "  Number of Huge Pages: %ld\n",       info->hugepage_info.nr);
    fprintf(out, "  Free Huge Pages: %ld\n",            info->hugepage_info.free);
    fprintf(out, "  Overcommit Huge Pages: %ld\n",      info->hugepage_info.overcommit);
    fprintf(out, "  Surplus Huge Pages: %ld\n",         info->hugepage_info.surplus);
    fprintf(out, "  Reserved Huge Pages: %ld\n",        info->hugepage_info.reserved);
    fprintf(out, "  Max Open Files: %lu\n",             info->max_open_files);
    fprintf(out, "  Max Processes: %lu\n",              info->max_processes);
    fprintf(out, "  Max Stack Size: %lu bytes\n",       info->max_stack_size);
    fprintf(out, "  Free Disk Space: %lu bytes\n",      info->disk_free);
    fprintf(out, "  Filesystem Type: %s\n",             info->fs_type);
    fprintf(out, "  MTU: %d\n",                         info->mtu);
    fprintf(out, "  Somaxconn: %d\n",                   info->somaxconn);
    fprintf(out, "  Is Little Endian: %s\n",            info->is_little_endian ? "true" : "false");
}
