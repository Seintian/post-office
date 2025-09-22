/**
 * @file string_builder.h
 * @ingroup tui_util
 * @brief Mutable string builder with amortized growth.
 */

#ifndef POSTOFFICE_TUI_UTIL_STRING_BUILDER_H
#define POSTOFFICE_TUI_UTIL_STRING_BUILDER_H

#include <stdarg.h>
#include <stddef.h>

typedef struct string_builder string_builder;

string_builder *sb_create(void);
void sb_destroy(string_builder *sb);
int sb_append(string_builder *sb, const char *s);
int sb_appendf(string_builder *sb, const char *fmt, ...);
int sb_vappendf(string_builder *sb, const char *fmt, va_list ap);
void sb_clear(string_builder *sb);
const char *sb_data(const string_builder *sb);
size_t sb_length(const string_builder *sb);

#endif /* POSTOFFICE_TUI_UTIL_STRING_BUILDER_H */
