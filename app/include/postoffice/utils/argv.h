#ifndef PO_UTILS_ARGV_H
#define PO_UTILS_ARGV_H
/** \ingroup utils */

#include <stdbool.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parsed arguments struct (example fields)
typedef struct {
    bool help;
    bool version;
    char *config_file;
    int loglevel;
    // Logging/syslog options
    bool syslog;        // enable syslog sink
    char *syslog_ident; // optional ident for syslog (NULL -> default)
    int fd;             // file descriptor for output
    // Demo flags
    bool tui_demo; // run a minimal TUI smoke demo and exit
    // add more fields as needed
} po_args_t;

// Initialize default values
void po_args_init(po_args_t *args) __nonnull((1));

// Parse command-line arguments
// Returns 0 on success, non-zero on error or help/version exit
int po_args_parse(po_args_t *args, int argc, char **argv, int fd) __nonnull((1, 3));

// Print usage information to stdout
void po_args_print_usage(int fd, const char *prog_name) __nonnull((2));

// Destroy (free) any dynamically allocated fields inside args
// Safe to call multiple times; leaves pointers NULL
void po_args_destroy(po_args_t *args) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_ARGV_H