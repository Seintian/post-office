#include "utils/configs.h"
#include "inih/ini.h"            // inih API: ini_parse, ini_handler
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>               // for errno, EINVAL

// Simple map entry
typedef struct {
    char *section;
    char *key;
    char *value;
} cfg_entry_t;

// Config struct
struct po_config {
    cfg_entry_t *entries;   // dynamic array
    size_t      count;
};

// Helper to build the lookup key "section.key"
// static char *make_full_key(const char *section, const char *key) {
//     size_t sl = strlen(section), kl = strlen(key);
//     if (sl == 0 || kl == 0) {
//         errno = EINVAL;
//         return NULL;  // invalid input
//     }

//     // +1 for dot, +1 for NUL
//     char *buf = malloc(sl + 1 + kl + 1);
//     if (!buf)
//         return NULL;  // allocation failed

//     memcpy(buf, section, sl);
//     buf[sl] = '.';
//     memcpy(buf+sl+1, key, kl);
//     buf[sl+1+kl] = '\0';
//     return buf;
// }

// inih callback
static int ini_handler_cb(void *user, const char *section,
                          const char *name, const char *value)
{
    if (!user || !section || !name || !value) {
        return STOP_PARSING;  // signal error to ini_parse
    }

    po_config_t *cfg = user;
    // Grow entries array
    cfg->entries = realloc(
        cfg->entries,
        (cfg->count + 1) * sizeof(cfg_entry_t)
    );
    if (!cfg->entries)
        return STOP_PARSING;

    // Store entry
    cfg_entry_t *e = &cfg->entries[cfg->count++];
    e->section = strdup(section);
    if (!e->section) {
        cfg->count--;  // rollback count
        return STOP_PARSING;
    }

    e->key     = strdup(name);
    if (!e->key) {
        free(e->section);
        cfg->count--;
        return STOP_PARSING;
    }

    e->value   = strdup(value);
    if (!e->value) {
        free(e->section);
        free(e->key);
        cfg->count--;
        return STOP_PARSING;
    }

    return CONTINUE_PARSING;
}

int po_config_load(const char *filename, po_config_t **cfg_out) {
    po_config_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg)
        return -1;

    int err = ini_parse(filename, ini_handler_cb, cfg);
    if (err != 0) {
        // ini_parse returns:
        //   0 = success
        //  >0 = line number of first error
        //  -1 = file open error
        //  -2 = memory alloc error (if INI_USE_STACK=0)
        po_config_free(cfg);
        return err;
    }

    *cfg_out = cfg;
    return 0;
}

void po_config_free(po_config_t *cfg) {
    if (!cfg)
        return;

    for (size_t i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].section);
        free(cfg->entries[i].key);
        free(cfg->entries[i].value);
    }
    free(cfg->entries);
    free(cfg);
}

static cfg_entry_t *find_entry(
    po_config_t *cfg,
    const char *section,
    const char *key
) {
    for (size_t i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].section, section) == 0 &&
            strcmp(cfg->entries[i].key, key) == 0)
        {
            return &cfg->entries[i];
        }
    }

    return NULL;
}

int po_config_get_str(
    po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
) {
    cfg_entry_t *e = find_entry(cfg, section, key);
    if (!e)
        return -1;

    *out_value = e->value;
    return 0;
}

int po_config_get_long(
    po_config_t *cfg,
    const char *section,
    const char *key,
    long *out_value
) {
    const char *s;
    if (po_config_get_str(cfg, section, key, &s) != 0)
        return -1;

    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0')
        return -2;  // parse error

    *out_value = v;
    return 0;
}

int po_config_get_bool(
    po_config_t *cfg,
    const char *section,
    const char *key,
    bool *out_value
) {
    const char *s;
    if (po_config_get_str(cfg, section, key, &s) != 0)
        return -1;

    if (strcmp(s, "0") == 0) {
        *out_value = false;
        return 0;
    }

    if (strcmp(s, "1") == 0) {
        *out_value = true;
        return 0;
    }

    return -2;  // invalid boolean
}
