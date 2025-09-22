/**
 * @file ui_registry.h
 * @ingroup tui_core
 * @brief WHAT: Registration of screens and widgets for lookup and creation.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_REGISTRY_H
#define POSTOFFICE_TUI_CORE_UI_REGISTRY_H

struct ui_registry; /* opaque */
struct ui_context;  /* fwd */

struct ui_registry *ui_registry_create(struct ui_context *ctx);
void ui_registry_destroy(struct ui_registry *reg);

/* Widget factory registration for dynamic creation by type name */
typedef void *(*ui_widget_factory)(void *create_args);
bool ui_registry_register_widget(struct ui_registry *reg, const char *type_name,
                                 ui_widget_factory fn);
void *ui_registry_create_widget(struct ui_registry *reg, const char *type_name, void *create_args);

#endif /* POSTOFFICE_TUI_CORE_UI_REGISTRY_H */
