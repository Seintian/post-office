#include <stdio.h>
#include <unistd.h>

#include "sysinfo/sysinfo.h"

int main() {
    po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) != 0) {
        fprintf(stderr, "Failed to collect system information\n");
        return 1;
    }

    po_sysinfo_print(&sysinfo, stdout);
    return 0;
}
