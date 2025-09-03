// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sysinfo/sysinfo.h"
#include <stddef.h>

// Enumerate available hugepage sizes in kB. Returns 0 on success, -1 on error.
int list_hugepage_sizes(unsigned long *sizes_kB, size_t max, size_t *count);

// Populate info for the given hugepage size (kB). Returns 0 on success, -1 on error.
int get_hugepage_info(unsigned long size_kB, hugepage_info_t *info);
