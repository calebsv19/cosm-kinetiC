#ifndef MENU_SETTINGS_INPUT_H
#define MENU_SETTINGS_INPUT_H

#include <stdbool.h>

#include "app/menu/menu_types.h"

bool menu_settings_handle_primary_click(SceneMenuInteraction *ctx, int x, int y);
bool menu_settings_has_pending_changes(const SceneMenuInteraction *ctx);
bool menu_settings_saved_differs_from_current(const SceneMenuInteraction *ctx);
void menu_settings_commit(SceneMenuInteraction *ctx, bool persist);
void menu_settings_save_current(SceneMenuInteraction *ctx);
void menu_settings_restore_saved(SceneMenuInteraction *ctx);
void menu_settings_reset_to_runtime(SceneMenuInteraction *ctx);
void menu_settings_reset_to_defaults(SceneMenuInteraction *ctx);

#endif
