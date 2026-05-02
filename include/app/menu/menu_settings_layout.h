#ifndef MENU_SETTINGS_LAYOUT_H
#define MENU_SETTINGS_LAYOUT_H

#include <stddef.h>

#include "app/menu/menu_types.h"

typedef struct MenuSettingsFieldLayout {
    MenuSettingsFieldId field;
    SDL_Rect cell_rect;
    SDL_Rect dec_rect;
    SDL_Rect inc_rect;
} MenuSettingsFieldLayout;

size_t menu_settings_layout_build_field_layouts(const SceneMenuInteraction *ctx,
                                                MenuSettingsFieldLayout *out_layouts,
                                                size_t max_layouts);
int menu_settings_layout_panel_height(const SceneMenuInteraction *ctx);
void menu_settings_layout_toggle_rects(const SceneMenuInteraction *ctx,
                                       SDL_Rect *volume_rect,
                                       SDL_Rect *render_rect,
                                       SDL_Rect *blur_rect);
void menu_settings_layout_action_rects(const SceneMenuInteraction *ctx,
                                       SDL_Rect *apply_rect,
                                       SDL_Rect *save_rect,
                                       SDL_Rect *revert_rect,
                                       SDL_Rect *restore_saved_rect,
                                       SDL_Rect *defaults_rect);
int menu_settings_layout_status_y(const SceneMenuInteraction *ctx);

#endif
