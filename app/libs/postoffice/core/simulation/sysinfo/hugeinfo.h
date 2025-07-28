#ifndef _HUGEINFO_H
#define _HUGEINFO_H

#include <stddef.h>

struct hugepage_info {
    unsigned long size_kB;  // page size in kB
    long nr;                // nr_hugepages
    long free;              // free_hugepages
    long overcommit;        // nr_overcommit_hugepages
    long surplus;           // surplus_hugepages
    long reserved;          // resv_hugepages
};

// Returns number of supported sizes, fills sizes[k] (caller allocates).
// Returns -1 on error.
int list_hugepage_sizes(unsigned long *sizes_kB, size_t max, size_t *count);

// Fills *info for given size (in kB). Returns 0 on success, -1 on error.
int get_hugepage_info(unsigned long size_kB, struct hugepage_info *info);

#endif // _HUGEINFO_H
