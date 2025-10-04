#include "utils/configs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "hashset/hashset.h"
#include "hashtable/hashtable.h"
#include "inih/ini.h"
#include "utils/errors.h"

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
struct po_config {
    po_hashtable_t *entries; // section.key-value pairs
    po_hashset_t *sections;  // set of section names
    bool strict;             // strict parsing mode
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

static int ini_handler_cb(void *user, const char *section, const char *name, const char *value) {
    if (!user) {
        errno = INIH_ENOUSER;
        return STOP_PARSING;
    }

    po_config_t *ctx = user;
    if (ctx->strict) {
        int last_errno = errno;
        errno = 0;

        if (is_empty(section))
            errno = INIH_ENOSECTION;

        if (errno == 0 && is_empty(name))
            errno = INIH_ENOKEY;

        if (errno == 0 && is_empty(value))
            errno = INIH_ENOVALUE;

        if (errno != 0)
            return STOP_PARSING;

        errno = last_errno;
    }

    char *full_key = get_full_key(section, name);
    if (!full_key)
        return STOP_PARSING;

    char *value_copy = strdup(value);
    if (!value_copy)
        return STOP_PARSING;

    if (po_hashtable_contains_key(ctx->entries, full_key)) {
        free(full_key);
        free(value_copy);

        if (ctx->strict) {
            errno = INIH_EDUPKEY;
            return STOP_PARSING;
        }
    } else if (po_hashtable_put(ctx->entries, full_key, value_copy) != 1) {
        free(value_copy);
        free(full_key);
        return STOP_PARSING;
    }

    char *section_key = strdup(section);
    if (!section_key)
        return STOP_PARSING;

    if (po_hashset_contains(ctx->sections, section_key)) {
        free(section_key);

        if (ctx->strict) {
            errno = INIH_EDUPSECTION;
            return STOP_PARSING;
        }
    } else if (po_hashset_add(ctx->sections, section_key) != 1) {
        free(section_key);
        return STOP_PARSING;
    }

    return CONTINUE_PARSING;
}

static int _po_config_load(const char *filename, po_config_t **cfg_out, bool strict) {
    if (!filename || !cfg_out) {
        errno = EINVAL;
        return -1;
    }

    po_config_t *ctx = calloc(1, sizeof(po_config_t));
    if (!ctx)
        return -1;

    ctx->strict = strict;
    ctx->entries = po_hashtable_create(&str_cmp, &str_hash);
    ctx->sections = po_hashset_create(&str_cmp, &str_hash);

    if (!ctx->entries || !ctx->sections) {
        if (ctx->entries)
            po_hashtable_destroy(&ctx->entries);

        if (ctx->sections)
            po_hashset_destroy(&ctx->sections);

        free(ctx);
        return -1;
    }

    int ret = ini_parse(filename, &ini_handler_cb, ctx);
    if (ret != 0) {
        *cfg_out = ctx;
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

static void clear_keys(const po_hashset_t *set) {
    if (!set)
        return;

    void **keys = po_hashset_keys(set);
    if (!keys)
        return;

    for (size_t i = 0; i < po_hashset_size(set); i++)
        free(keys[i]);
    free(keys);
}

void po_config_free(po_config_t **cfg) {
    po_config_t *config = *cfg;
    if (!config)
        return;

    if (config->entries) {
        size_t size = po_hashtable_size(config->entries);

        void **keys = po_hashtable_keyset(config->entries);
        void **values = po_hashtable_values(config->entries);

        for (size_t i = 0; i < size; i++)
            free(values[i]);

        po_hashtable_destroy(&config->entries);

        for (size_t i = 0; i < size; i++)
            free(keys[i]);
        free(keys);
        free(values);
    }

    if (config->sections) {
        clear_keys(config->sections);
        po_hashset_destroy(&config->sections);
    }

    free(config);
    *cfg = NULL;
}

int po_config_get_str(const po_config_t *cfg, const char *section, const char *key,
                      const char **out_value) {
    char *full_key = get_full_key(section, key);
    if (!full_key)
        return -1;

    const char *v = po_hashtable_get(cfg->entries, full_key);
    free(full_key);
    if (!v) {
        errno = INIH_ENOKEY;
        return -1;
    }

    *out_value = v;
    return 0;
}

int po_config_get_int(const po_config_t *cfg, const char *section, const char *key,
                      int *out_value) {
    const char *v;
    if (po_config_get_str(cfg, section, key, &v) != 0)
        return -1;

    char *endptr;
    long value = strtol(v, &endptr, 10);
    if (*endptr != '\0') {
        errno = INIH_EINVAL;
        return -1;
    }
    if (value < INT_MIN || value > INT_MAX) {
        errno = INIH_ERANGE;
        return -1;
    }

    *out_value = (int)value;
    return 0;
}

int po_config_get_long(const po_config_t *cfg, const char *section, const char *key,
                       long *out_value) {
    const char *v;
    if (po_config_get_str(cfg, section, key, &v) != 0)
        return -1;

    char *endptr;
    long value = strtol(v, &endptr, 10);
    if (*endptr != '\0') {
        errno = INIH_EINVAL;
        return -1;
    }

    *out_value = value;
    return 0;
}

int po_config_get_bool(const po_config_t *cfg, const char *section, const char *key,
                       bool *out_value) {
    const char *v;
    if (po_config_get_str(cfg, section, key, &v) != 0)
        return -1;

    if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0) {
        *out_value = false;
        return 0;
    }
    if (strcmp(v, "1") == 0 || strcmp(v, "true") == 0) {
        *out_value = true;
        return 0;
    }

    errno = INIH_EINVAL;
    return -1;
}
