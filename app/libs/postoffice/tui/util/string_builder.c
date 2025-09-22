/**
 * @file string_builder.c
 * @ingroup tui_util
 * @brief HOW: Incremental string assembly with amortized growth.
 */

#include "string_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct string_builder {
    char *buf;
    size_t len;
    size_t cap;
};

static int sb_grow(string_builder *sb, size_t need_extra) {
    size_t need = sb->len + need_extra + 1; /* +1 for NUL */
    if (need <= sb->cap)
        return 0;
    size_t new_cap = sb->cap ? sb->cap : 64;
    while (new_cap < need)
        new_cap *= 2;
    char *p = (char *)realloc(sb->buf, new_cap);
    if (!p)
        return -1;
    sb->buf = p;
    sb->cap = new_cap;
    return 0;
}

string_builder *sb_create(void) {
    return (string_builder *)calloc(1, sizeof(string_builder));
}

void sb_destroy(string_builder *sb) {
    if (!sb)
        return;
    free(sb->buf);
    free(sb);
}

int sb_append(string_builder *sb, const char *s) {
    if (!sb || !s)
        return -1;
    size_t sl = strlen(s);
    if (sb_grow(sb, sl) != 0)
        return -1;
    memcpy(sb->buf + sb->len, s, sl);
    sb->len += sl;
    sb->buf[sb->len] = '\0';
    return 0;
}

int sb_vappendf(string_builder *sb, const char *fmt, va_list ap) {
    if (!sb || !fmt)
        return -1;
    va_list ap2;
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (need < 0)
        return -1;
    if (sb_grow(sb, (size_t)need) != 0)
        return -1;
    int wrote = vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
    if (wrote < 0)
        return -1;
    sb->len += (size_t)wrote;
    return 0;
}

int sb_appendf(string_builder *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rc = sb_vappendf(sb, fmt, ap);
    va_end(ap);
    return rc;
}

void sb_clear(string_builder *sb) {
    if (!sb)
        return;
    sb->len = 0;
    if (sb->buf)
        sb->buf[0] = '\0';
}

const char *sb_data(const string_builder *sb) {
    return sb ? (sb->buf ? sb->buf : "") : "";
}
size_t sb_length(const string_builder *sb) {
    return sb ? sb->len : 0;
}
