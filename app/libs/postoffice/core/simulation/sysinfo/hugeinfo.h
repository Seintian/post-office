#ifndef _HUGEINFO_H
#define _HUGEINFO_H

#include <stddef.h>
#include "core/simulation/sysinfo.h"


// Returns number of supported sizes, fills sizes[k] (caller allocates).
// Returns -1 on error.
int list_hugepage_sizes(unsigned long *sizes_kB, size_t max, size_t *count);

// Fills *info for given size (in kB). Returns 0 on success, -1 on error.
int get_hugepage_info(unsigned long size_kB, hugepage_info_t *info);

#endif // _HUGEINFO_H
