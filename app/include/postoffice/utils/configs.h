#ifndef PO_UTILS_CONFIGS_H
#define PO_UTILS_CONFIGS_H

#include <stddef.h>
#include <stdbool.h>

#define STOP_PARSING 0

#define CONTINUE_PARSING 1

#ifdef __cplusplus
extern "C" {
#endif

// Opaque config handle
typedef struct po_config po_config_t;

// Load file. Returns 0 on success, or non‑zero error code on failure.
int po_config_load(const char *filename, po_config_t **cfg_out);

// Load file with strict parsing (no empty sections, no missing values).
int po_config_load_strict(const char *filename, po_config_t **cfg_out);

// Free resources.
void po_config_free(po_config_t *cfg);

// Retrieve a value as string.
//   - Returns 0 if (section,key) existed, non‑zero otherwise.
//   - On success *out_value is your NUL‑terminated string (owned by cfg).
int po_config_get_str(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
);

// Retrieve a value as int.
//   - Returns 0 on success (valid integer), non‑zero on missing key or parse error.
int po_config_get_long(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    long *out_value
);

// Retrieve a value as bool ("0"/"1").
//   - Returns 0 on success (value was exactly "0" or "1"), else non‑zero.
int po_config_get_bool(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    bool *out_value
);

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_CONFIGS_H
