#include "utils/configs.h"
#include "utils/errors.h"
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
    size_t       count;
};

// inih callback
static int ini_handler_cb(
    void *user,
    const char *section,
    const char *name,
    const char *value
) {
    if (!user || !name) {
        errno = EINVAL;         // invalid argument
        return STOP_PARSING;    // signal error to ini_parse
    }

    po_config_t *cfg = user;
    cfg->entries = realloc(     // Grow entries array
        cfg->entries,
        (cfg->count + 1) * sizeof(cfg_entry_t)
    );
    if (!cfg->entries)
        return STOP_PARSING;

    cfg_entry_t *e = &cfg->entries[cfg->count++];

    // No section allowed
    if (section) {
        e->section = strdup(section);
        if (!e->section) {
            cfg->count--;  // rollback count
            return STOP_PARSING;
        }
    }
    else e->section = NULL;  // no section

    e->key = strdup(name);
    if (!e->key) {
        free(e->section);
        cfg->count--;
        return STOP_PARSING;
    }

    // No value allowed
    if (value) {
        e->value = strdup(value);
        if (!e->value) {
            free(e->section);
            free(e->key);
            cfg->count--;
            return STOP_PARSING;
        }
    }
    else e->value = NULL;  // no value

    return CONTINUE_PARSING;
}

// strict inih callback
static int ini_handler_cb_strict(
    void *user,
    const char *section,
    const char *name,
    const char *value
) {
    if (!user) {
        errno = EINVAL;             // invalid argument
        return STOP_PARSING;        // signal error to ini_parse
    }
    if (!section) {
        errno = INIH_ENOSECTION;    // no section provided
        return STOP_PARSING;
    }
    if (!name) {
        errno = INIH_ENOKEY;        // no key provided
        return STOP_PARSING;
    }
    if (!value) {
        errno = INIH_ENOVALUE;      // no value provided
        return STOP_PARSING;
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

int po_config_load_strict(const char *filename, po_config_t **cfg_out) {
    po_config_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg)
        return -1;

    int err = ini_parse(filename, ini_handler_cb_strict, cfg);
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

static inline bool is_empty(const char *str) {
    return str == NULL || str[0] == '\0';
}

static const cfg_entry_t *find_entry(
    const po_config_t *cfg,
    const char *section,
    const char *key
) {
    if (!cfg || !key) {
        errno = EINVAL;  // invalid argument
        return NULL;
    }

    for (size_t i = 0; i < cfg->count; i++) {
        if (is_empty(section) && is_empty(cfg->entries[i].section)) {
            // Match any section
            if (strcmp(cfg->entries[i].key, key) == 0)
                return &cfg->entries[i];
        }
        else if (!is_empty(section) && is_empty(cfg->entries[i].section))
            continue;

        else if (!is_empty(section) && cfg->entries[i].section &&
                strcmp(cfg->entries[i].section, section) == 0 &&
                strcmp(cfg->entries[i].key, key) == 0)
        {
            // Match specific section and key
            return &cfg->entries[i];
        }
    }

    return NULL;
}

static const cfg_entry_t *find_entry_strict(
    const po_config_t *cfg,
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
    const po_config_t *cfg,
    const char *section,
    const char *key,
    const char **out_value
) {
    const cfg_entry_t *e;
    if (is_empty(section)) {
        e = find_entry(cfg, NULL, key);
    }
    else {
        e = find_entry_strict(cfg, section, key);
    }
    if (!e)
        return -1;

    *out_value = e->value;
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
    if (*end != '\0')
        return -2;  // parse error

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
