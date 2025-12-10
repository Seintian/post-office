/**
 * @file configs.h
 * @ingroup utils
 * @brief INI-style configuration loader and accessor utilities.
 *
 * Thin wrapper around the inih parser offering convenient typed accessors and
 * consistent error codes from @ref errors.h. All functions are thread-compatible
 * and avoid global state; a loaded configuration is represented by an opaque
 * handle @ref po_config_t.
 */

#ifndef PO_UTILS_CONFIGS_H
#define PO_UTILS_CONFIGS_H
/** \ingroup utils */

#include <stdbool.h>
#include <stddef.h>
#include <sys/cdefs.h>

/** Stop parsing early from user handlers (internal convention). */
#define STOP_PARSING 0

/** Continue parsing (internal convention). */
#define CONTINUE_PARSING 1

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle for a loaded configuration object. */
typedef struct po_config po_config_t;

/**
 * @brief Load and parse an INI file from disk.
 *
 * @param filename Path to the INI file.
 * @param cfg_out  Out-parameter receiving the allocated config handle.
 * @return 0 on success; non-zero error code from @ref errors.h on failure.
 */
int po_config_load(const char *filename, po_config_t **cfg_out) __nonnull((1, 2));

/**
 * @brief Load an INI file with strict validation.
 *
 * Strict mode rejects empty sections, duplicate keys, or missing values.
 *
 * @param filename Path to the INI file.
 * @param cfg_out  Out-parameter receiving the allocated config handle.
 * @return 0 on success; non-zero error code on failure.
 */
int po_config_load_strict(const char *filename, po_config_t **cfg_out) __nonnull((1, 2));

/**
 * @brief Destroy a configuration handle and release associated resources.
 * @param cfg Address of the config handle; on return set to NULL.
 */
void po_config_free(po_config_t **cfg) __nonnull((1));

/**
 * @brief Retrieve a value as a string.
 *
 * - Returns 0 if (section,key) exists, non-zero otherwise.
 * - On success, `*out_value` is a NUL-terminated string view owned by the config.
 *
 * @param cfg       Config handle.
 * @param section   Section name.
 * @param key       Key name.
 * @param out_value Receives pointer to the string value (valid until @ref po_config_free).
 */
int po_config_get_str(const po_config_t *cfg, const char *section, const char *key,
                      const char **out_value) __nonnull((1, 3, 4));

/**
 * @brief Retrieve a value as an int.
 *
 * @return 0 on success (valid int), non-zero on missing key or parse error.
 */
int po_config_get_int(const po_config_t *cfg, const char *section, const char *key, int *out_value)
    __nonnull((1, 3, 4));

/**
 * @brief Retrieve a value as a long.
 *
 * @return 0 on success (valid long), non-zero on missing key or parse error.
 */
int po_config_get_long(const po_config_t *cfg, const char *section, const char *key,
                       long *out_value) __nonnull((1, 3, 4));

/**
 * @brief Retrieve a value as a boolean.
 *
 * Accepts only "0" or "1", "false" or "true". Any other value yields a parse error.
 *
 * @return 0 on success; non-zero on missing key or invalid value.
 */
int po_config_get_bool(const po_config_t *cfg, const char *section, const char *key,
                       bool *out_value) __nonnull((1, 3, 4));

/**
 * @brief Set a value as a string.
 *
 * Updates the existing value or inserts a new one if it doesn't exist.
 * The value is duplicated, so the caller retains ownership of the input string.
 *
 * @param cfg       Config handle.
 * @param section   Section name.
 * @param key       Key name.
 * @param value     Value to set.
 * @return 0 on success; non-zero on allocation failure or other error.
 */
int po_config_set_str(po_config_t *cfg, const char *section, const char *key, const char *value)
    __nonnull((1, 3, 4));

/**
 * @brief Callback for configuration iteration.
 *
 * @param section   Section name.
 * @param key       Key name (without section prefix).
 * @param value     Value string.
 * @param user_data User-provided context.
 */
typedef void (*po_config_entry_cb)(const char *section, const char *key, const char *value,
                                   void *user_data);

/**
 * @brief Iterate over all configuration entries.
 *
 * @param cfg       Config handle.
 * @param cb        Callback function to invoke for each entry.
 * @param user_data User-provided context passed to the callback.
 */
void po_config_foreach(const po_config_t *cfg, po_config_entry_cb cb, void *user_data)
    __nonnull((1, 2));

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_CONFIGS_H
