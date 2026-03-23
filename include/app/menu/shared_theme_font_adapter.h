#ifndef PHYSICS_SIM_SHARED_THEME_FONT_ADAPTER_H
#define PHYSICS_SIM_SHARED_THEME_FONT_ADAPTER_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct PhysicsSimMenuThemePalette {
    SDL_Color background_fill;
    SDL_Color panel_fill;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color accent_primary;
    SDL_Color button_fill;
    SDL_Color button_active_fill;
} PhysicsSimMenuThemePalette;

bool physics_sim_shared_theme_resolve_menu_palette(PhysicsSimMenuThemePalette* out_palette);
bool physics_sim_shared_theme_cycle_next(void);
bool physics_sim_shared_theme_cycle_prev(void);
bool physics_sim_shared_theme_set_preset(const char* preset_name);
bool physics_sim_shared_theme_current_preset(char* out_name, size_t out_name_size);
bool physics_sim_shared_theme_load_persisted(void);
bool physics_sim_shared_theme_save_persisted(void);
bool physics_sim_shared_font_resolve_menu_title(char* out_path, size_t out_path_size, int* out_point_size);
bool physics_sim_shared_font_resolve_menu_body(char* out_path, size_t out_path_size, int* out_point_size);
bool physics_sim_shared_font_resolve_menu_small(char* out_path, size_t out_path_size, int* out_point_size);

#endif
