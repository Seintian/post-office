#ifndef PO_UTILS_CONFIGS_H
#define PO_UTILS_CONFIGS_H
/** \ingroup utils */

#include <stddef.h>
#include <stdbool.h>
#include <sys/cdefs.h>


#define STOP_PARSING 0

#define CONTINUE_PARSING 1

#ifdef __cplusplus
extern "C" {
#endif

// Opaque config handle
typedef struct po_config po_config_t;

// Load file. Returns 0 on success, or non‑zero error code on failure.
int po_config_load(const char *filename, po_config_t **cfg_out) __nonnull((1,2));

// Load file with strict parsing (no empty sections, no missing values).
int po_config_load_strict(const char *filename, po_config_t **cfg_out) __nonnull((1,2));

// Free resources.
void po_config_free(po_config_t **cfg) __nonnull((1));

// Retrieve a value as string.
//   - Returns 0 if (section,key) existed, non‑zero otherwise.
//   - On success *out_value is your NUL‑terminated string.
int po_config_get_str(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
) __nonnull((1, 3, 4));

// Retrieve a value as int.
//   - Returns 0 on success (valid int), non‑zero on missing key or parse error.
int po_config_get_int(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    int *out_value
) __nonnull((1, 3, 4));

// Retrieve a value as long.
//   - Returns 0 on success (valid long), non‑zero on missing key or parse error.
int po_config_get_long(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    long *out_value
) __nonnull((1, 3, 4));

// Retrieve a value as bool ("0"/"1").
//   - Returns 0 on success (value was exactly "0" or "1"), else non‑zero.
int po_config_get_bool(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    bool *out_value
) __nonnull((1, 3, 4));

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_CONFIGS_H
