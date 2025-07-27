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
struct po_config {
    hashset_t   *sections;      // set of section names
    hashset_t   *values;        // set of all values
    hashtable_t *config;        // map: section_name -> hashtable of key->value
    const char  *last_section;  // track section changes
};

static int ini_handler_cb(
    void *user,
    const char *section,
    const char *name,
    const char *value
) {
    po_config_t *ctx = user;
    if (!user || !name) {
        errno = EINVAL;
        return STOP_PARSING;
    }

    if (ctx->last_section == NULL || strcmp(ctx->last_section, section) != 0) {
        if (hashset_contains(ctx->sections, section)) {
            errno = INIH_EDUPSECTION;
            return STOP_PARSING;
        }

        if (hashset_add(ctx->sections, strdup(section)) < 0) {
            errno = ENOMEM;
            return STOP_PARSING;
        }

        ctx->last_section = strdup(section);
        hashtable_t *kv = hashtable_create(str_cmp, str_hash);
        if (!kv) {
            errno = ENOMEM;
            return STOP_PARSING;
        }

        if (hashtable_put(ctx->config, strdup(section), kv) < 0) {
            errno = ENOMEM;
            return STOP_PARSING;
        }
    }

    // retrieve kv table for this section
    const hashtable_t *kv = hashtable_get(ctx->config, section);
    if (!kv) {
        errno = EFAULT;
        return STOP_PARSING;
    }

    // detect duplicate key
    if (hashtable_contains_key(kv, name)) {
        errno = INIH_EDUPKEY;
        return STOP_PARSING;
    }

    // track value globally (use value string as key)
    if (hashset_add(ctx->values, strdup(value)) < 0) {
        errno = ENOMEM;
        return STOP_PARSING;
    }

    return CONTINUE_PARSING;
}

static int ini_handler_cb_strict(
    void *user,
    const char *section,
    const char *name,
    const char *value
) {
    po_config_t *ctx = user;
    if (!user || !section || !name || !value) {
        errno = EINVAL;
        return STOP_PARSING;
    }

    if (ctx->last_section == NULL || strcmp(ctx->last_section, section) != 0) {
        if (hashset_contains(ctx->sections, section)) {
            errno = INIH_EDUPSECTION;
            return STOP_PARSING;
        }

        // insert section into set
        if (hashset_add(ctx->sections, strdup(section)) < 0) {
            errno = ENOMEM;
            return STOP_PARSING;
        }

        ctx->last_section = strdup(section);
        // prepare key->value table for this section
        hashtable_t *kv = hashtable_create(str_cmp, str_hash);
        if (!kv) {
            errno = ENOMEM;
            return STOP_PARSING;
        }

        if (hashtable_put(ctx->config, strdup(section), kv) < 0) {
            errno = ENOMEM;
            return STOP_PARSING;
        }
    }

    hashtable_t *kv = hashtable_get(ctx->config, section);
    if (!kv) {
        errno = EFAULT;
        return STOP_PARSING;
    }

    if (hashtable_contains_key(kv, name)) {
        errno = INIH_EDUPKEY;
        return STOP_PARSING;
    }

    if (hashtable_put(kv, strdup(name), strdup(value)) < 0) {
        errno = ENOMEM;
        return STOP_PARSING;
    }

    if (hashset_add(ctx->values, strdup(value)) < 0) {
        errno = ENOMEM;
        return STOP_PARSING;
    }

    return CONTINUE_PARSING;
}

int po_config_load_strict(const char *filename, po_config_t **cfg_out) {
    po_config_t ctx = {0};
    ctx.sections = hashset_create(str_cmp, str_hash);
    ctx.values   = hashset_create(str_cmp, str_hash);
    ctx.config   = hashtable_create(str_cmp, str_hash);
    if (!ctx.sections || !ctx.values || !ctx.config) {
        errno = ENOMEM;
        goto cleanup_ctx;
    }

    int err = ini_parse(filename, ini_handler_cb_strict, &ctx);
    if (err != 0) {
        if (err > 0)
            errno = INIH_ESYNTAX;

        goto cleanup_ctx;
    }

    po_config_t *cfg = malloc(sizeof(*cfg));
    if (!cfg) {
        errno = ENOMEM;
        goto cleanup_ctx;
    }

    cfg->sections = ctx.sections;
    cfg->values   = ctx.values;
    cfg->config   = ctx.config;

    *cfg_out = cfg;
    return 0;

cleanup_ctx:
    if (ctx.sections) hashset_free(ctx.sections);
    if (ctx.values)   hashset_free(ctx.values);
    if (ctx.config) {
        void **secs = hashtable_keyset(ctx.config);
        for (size_t i = 0; secs[i]; i++) {
            char *sec = secs[i];
            hashtable_t *kv = hashtable_get(ctx.config, sec);
            void **keys = hashtable_keyset(kv);
            for (size_t j = 0; keys[j]; j++) {
                char *v = hashtable_get(kv, keys[j]);
                free(v);
            }
            free(keys);
            hashtable_free(kv);
            free(sec);
        }
        free(secs);
        hashtable_free(ctx.config);
    }

    return -1;
}

int po_config_load(const char *filename, po_config_t **cfg_out) {
    po_config_t ctx = {0};
    ctx.sections = hashset_create(str_cmp, str_hash);
    ctx.values   = hashset_create(str_cmp, str_hash);
    ctx.config   = hashtable_create(str_cmp, str_hash);
    if (!ctx.sections || !ctx.values || !ctx.config) {
        errno = ENOMEM;
        goto cleanup_ctx;
    }

    int err = ini_parse(filename, ini_handler_cb, &ctx);
    if (err != 0) {
        if (err > 0)
            errno = INIH_ESYNTAX;

        goto cleanup_ctx;
    }

    po_config_t *cfg = malloc(sizeof(*cfg));
    if (!cfg) {
        errno = ENOMEM;
        goto cleanup_ctx;
    }

    cfg->sections = ctx.sections;
    cfg->values   = ctx.values;
    cfg->config   = ctx.config;

    *cfg_out = cfg;
    return 0;

cleanup_ctx:
    if (ctx.sections) hashset_free(ctx.sections);
    if (ctx.values)   hashset_free(ctx.values);
    if (ctx.config) {
        void **secs = hashtable_keyset(ctx.config);
        for (size_t i = 0; secs[i]; i++) {
            char *sec = secs[i];
            hashtable_t *kv = hashtable_get(ctx.config, sec);
            void **keys = hashtable_keyset(kv);
            for (size_t j = 0; keys[j]; j++) {
                char *v = hashtable_get(kv, keys[j]);
                free(v);
            }
            hashtable_free(kv);
            free(keys);
            free(sec);
        }
        free(secs);
        hashtable_free(ctx.config);
    }

    return -1;
}

void po_config_free(po_config_t *cfg) {
    void **secs = hashtable_keyset(cfg->config);
    for (size_t i = 0; secs[i]; i++) {
        char *sec = secs[i];
        hashtable_t *kv = hashtable_get(cfg->config, sec);
        void **keys = hashtable_keyset(kv);
        for (size_t j = 0; keys[j]; j++) {
            char *v = hashtable_get(kv, keys[j]);
            free(v);
        }
        hashtable_free(kv);
        free(keys);
        free(sec);
    }

    free(secs);

    hashset_free(cfg->values);
    hashset_free(cfg->sections);
    hashtable_free(cfg->config);
    free(cfg);
}

int po_config_get_str(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
) {
    const hashtable_t *kv = hashtable_get(cfg->config, section);
    if (!kv) {
        errno = ENOENT;
        return -1;
    }

    const char *v = hashtable_get(kv, key);
    if (!v) {
        errno = ENOENT;
        return -1;
    }

    *out_value = v;
    return 0;
}

int po_config_get_long(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    long *out_value
) {
    const char *s;
    if (po_config_get_str(cfg, section, key, &s) != 0)
        return -1;

    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0') {
        errno = EINVAL;
        return -1;
    }

    *out_value = v;
    return 0;
}

int po_config_get_bool(
    const po_config_t *cfg,
    const char *section,
    const char *key,
    bool *out_value
) {
    const char *s;
    if (po_config_get_str(cfg, section, key, &s) != 0) {
        return -1;
    }

    if (strcmp(s, "0") == 0) {
        *out_value = false;
        return 0;
    }

    if (strcmp(s, "1") == 0) {
        *out_value = true;
        return 0;
    }

    errno = EINVAL;
    return -1;
}
