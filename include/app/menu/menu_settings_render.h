#ifndef MENU_SETTINGS_RENDER_H
#define MENU_SETTINGS_RENDER_H

#include <stddef.h>

#include "app/menu/menu_types.h"

const char *menu_settings_render_quality_name(const MenuSettingsShellState *state);
void menu_settings_render_grid_label(const MenuSettingsShellState *state,
                                     char *out,
                                     size_t out_size);
void menu_settings_render_substeps_label(const MenuSettingsShellState *state,
                                         char *out,
                                         size_t out_size);
void menu_settings_render_solver_label(const MenuSettingsShellState *state,
                                       char *out,
                                       size_t out_size);
void menu_settings_render_headless_frames_label(const MenuSettingsShellState *state,
                                                char *out,
                                                size_t out_size);
void menu_settings_render_inflow_label(const MenuSettingsShellState *state,
                                       char *out,
                                       size_t out_size);
void menu_settings_render_viscosity_label(const MenuSettingsShellState *state,
                                          char *out,
                                          size_t out_size);
void menu_settings_draw_simulation_panel(SceneMenuInteraction *ctx);

#endif
