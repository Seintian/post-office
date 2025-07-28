#include "utils/configs.h"
#include "utils/errors.h"
#include "hashtable/hashtable.h"
#include "hashset/hashset.h"
#include "inih/ini.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// djb2 string hash
static unsigned long str_hash(const void *s) {
    const unsigned char *p = s;
    unsigned long hash = 5381;
    while (*p)
        hash = ((hash << 5) + hash) + *p++;

    return hash;
}

// string comparison function
static int str_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// inih handler context
struct _po_config_t {
    hashtable_t *entries;   // section.key-value pairs
    hashset_t *sections;    // set of section names
    bool strict;            // strict parsing mode
};

static char *get_full_key(const char *section, const char *key) {
    size_t section_len = section ? strlen(section) : 0;
    size_t key_len = strlen(key);
    if (section_len == 0)
        return strdup(key);

    char *full_key = malloc(section_len + key_len + 2);
    if (!full_key)
        return NULL;

    snprintf(full_key, section_len + key_len + 2, "%s.%s", section, key);
    return full_key;
}

static bool is_empty(const char *str) {
    return !str || str[0] == '\0';
}

static int ini_handler_cb(
    void *user,
    const char *section,
    const char *name,
    const char *value
) {
    if (!user) {
        errno = EINVAL;
        return STOP_PARSING;
    }

    po_config_t *ctx = user;
    if (ctx->strict && (is_empty(section) || is_empty(name) || is_empty(value))) {
        errno = EINVAL;
        return STOP_PARSING;
    }

    char *full_key = get_full_key(section, name);
    if (!full_key)
        return STOP_PARSING;

    char full_key_backup[1024];
    strncpy(full_key_backup, full_key, sizeof(full_key_backup));
    full_key_backup[sizeof(full_key_backup) - 1] = '\0';

    char *value_copy = strdup(value);
    if (!value_copy)
        return STOP_PARSING;

    if (hashtable_contains_key(ctx->entries, full_key)) {
        free(full_key);

        if (ctx->strict) {
            free(value_copy);
            errno = EEXIST;
            return STOP_PARSING;
        }

        free(hashtable_get(ctx->entries, full_key_backup));
        hashtable_put(ctx->entries, full_key_backup, value_copy);
    }
    else if (hashtable_put(ctx->entries, full_key, value_copy) != 1) {
        free(value_copy);
        free(full_key);
        return STOP_PARSING;
    }

    char *section_key = strdup(section);
    if (!section_key)
        return STOP_PARSING;

    if (ctx->strict && hashset_contains(ctx->sections, section_key)) {
        free(section_key);
        errno = EEXIST;
        return STOP_PARSING;
    }
    if (hashset_add(ctx->sections, section_key) != 1)
        free(section_key);

    return CONTINUE_PARSING;
}


static int _po_config_load(
    const char *filename,
    po_config_t **cfg_out,
    bool strict
) {
    if (!filename || !cfg_out) {
        errno = EINVAL;
        return -1;
    }

    po_config_t *ctx = calloc(1, sizeof(po_config_t));
    if (!ctx)
        return -1;

    ctx->strict = strict;
    ctx->entries = hashtable_create(&str_cmp, &str_hash);
    ctx->sections = hashset_create(&str_cmp, &str_hash);

    if (!ctx->entries || !ctx->sections) {
        po_config_free(&ctx);
        return -1;
    }

    int ret = ini_parse(filename, &ini_handler_cb, ctx);
    if (ret != 0) {
        po_config_free(&ctx);
        return -1;
    }

    *cfg_out = ctx;
    return ret;
}

int po_config_load_strict(const char *filename, po_config_t **cfg_out) {
    return _po_config_load(filename, cfg_out, true);
}

int po_config_load(const char *filename, po_config_t **cfg_out) {
    return _po_config_load(filename, cfg_out, false);
}

static void clear_keys_values(const hashtable_t *table) {
    if (!table)
        return;

    void **keys = hashtable_keyset(table);
    if (!keys)
        return;

    for (size_t i = 0; i < hashtable_size(table); i++) {
        if (!keys[i])
            continue;

        free(hashtable_get(table, keys[i]));
    }

    for (size_t i = 0; i < hashtable_size(table); i++)
        free(keys[i]);
    free(keys);
}

static void clear_keys(const hashset_t *set) {
    if (!set)
        return;

    void **keys = hashset_keys(set);
    if (!keys)
        return;

    for (size_t i = 0; i < hashset_size(set); i++)
        free(keys[i]);
    free(keys);
}

void po_config_free(po_config_t **cfg) {
    po_config_t *config = *cfg;
    if (config->entries) {
        clear_keys_values(config->entries);
        hashtable_free(config->entries);
    }
    if (config->sections) {
        clear_keys(config->sections);
        hashset_free(config->sections);
    }

    free(config);
    *cfg = NULL;
}

int po_config_get_str(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
) {
    char *full_key = get_full_key(section, key);
    if (!full_key)
        return -1;

    const char *v = hashtable_get(cfg->entries, full_key);
    free(full_key);
    if (!v)
        return -1;

    *out_value = v;
    return 0;
}

int po_config_get_long(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    long *out_value
) {
    const char *v;
    if (po_config_get_str(cfg, section, key, &v) != 0)
        return -1;

    char *endptr;
    long value = strtol(v, &endptr, 10);
    if (*endptr != '\0') {
        errno = EINVAL;
        return -1;
    }

    *out_value = value;
    return 0;
}

int po_config_get_bool(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    bool *out_value
) {
    const char *v;
    if (po_config_get_str(cfg, section, key, &v) != 0)
        return -1;

    if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0) {
        *out_value = false;
        return 0;
    }
    else if (strcmp(v, "1") == 0 || strcmp(v, "true") == 0) {
        *out_value = true;
        return 0;
    }

    errno = EINVAL;
    return -1;
}
