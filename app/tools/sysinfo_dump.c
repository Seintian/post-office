#include <stdio.h>
#include <postoffice/sysinfo/sysinfo.h>

int main(void) {
    po_sysinfo_t info = {0};
    if (po_sysinfo_collect(&info) != 0) {
        fprintf(stderr, "sysinfo: collect failed\n");
        return 1;
    }
    po_sysinfo_print(&info, stdout);
    return 0;
}
