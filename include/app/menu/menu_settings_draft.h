#ifndef MENU_SETTINGS_DRAFT_H
#define MENU_SETTINGS_DRAFT_H

#include "app/menu/menu_settings_types.h"
#include "app/scene_menu.h"

void menu_settings_shell_init(MenuSettingsShellState *state,
                              const AppConfig *cfg,
                              const SceneMenuSelection *selection,
                              SimulationMode sim_mode,
                              SpaceMode space_mode);
void menu_settings_shell_reload_from_runtime(MenuSettingsShellState *state,
                                             const AppConfig *cfg,
                                             const SceneMenuSelection *selection,
                                             SimulationMode sim_mode,
                                             SpaceMode space_mode);
void menu_settings_shell_capture_saved_from_runtime(MenuSettingsShellState *state,
                                                    const AppConfig *cfg,
                                                    const SceneMenuSelection *selection,
                                                    SimulationMode sim_mode,
                                                    SpaceMode space_mode);
void menu_settings_shell_sync_provider(MenuSettingsShellState *state,
                                       SimulationMode sim_mode,
                                       SpaceMode space_mode);
void menu_settings_shell_sync_quality_context(MenuSettingsShellState *state,
                                              const SceneMenuSelection *selection,
                                              SpaceMode space_mode);
void menu_settings_shell_load_defaults(MenuSettingsShellState *state,
                                       SimulationMode sim_mode,
                                       SpaceMode space_mode);
void menu_settings_shell_apply_to_runtime(const MenuSettingsShellState *state,
                                          AppConfig *cfg,
                                          SceneMenuSelection *selection);
bool menu_settings_shell_is_dirty(const MenuSettingsShellState *state,
                                  const AppConfig *cfg,
                                  const SceneMenuSelection *selection);
bool menu_settings_shell_saved_differs_from_runtime(const MenuSettingsShellState *state,
                                                    const AppConfig *cfg,
                                                    const SceneMenuSelection *selection);
void menu_settings_shell_restore_saved_to_draft(MenuSettingsShellState *state,
                                                SimulationMode sim_mode,
                                                SpaceMode space_mode);
void menu_settings_shell_apply_saved_to_runtime(const MenuSettingsShellState *state,
                                                AppConfig *cfg,
                                                SceneMenuSelection *selection);
void menu_settings_shell_set_custom_quality(MenuSettingsShellState *state);
void menu_settings_shell_apply_quality(MenuSettingsShellState *state, int index);
void menu_settings_shell_nudge_field(MenuSettingsShellState *state,
                                     MenuSettingsFieldId field,
                                     int direction);
void menu_settings_shell_toggle_field(MenuSettingsShellState *state,
                                      MenuSettingsFieldId field);
void menu_settings_shell_adjust_grid(MenuSettingsShellState *state, int delta);
void menu_settings_shell_adjust_substeps(MenuSettingsShellState *state, int delta);
void menu_settings_shell_adjust_solver_iterations(MenuSettingsShellState *state, int delta);
void menu_settings_shell_toggle_volume_frames(MenuSettingsShellState *state);
void menu_settings_shell_toggle_render_frames(MenuSettingsShellState *state);
void menu_settings_shell_set_headless_frame_count(MenuSettingsShellState *state, int frames);
void menu_settings_shell_set_tunnel_inflow_speed(MenuSettingsShellState *state, float speed);
void menu_settings_shell_set_velocity_damping(MenuSettingsShellState *state, float damping);
const MenuSettingsDraft *menu_settings_shell_draft(const MenuSettingsShellState *state);

#endif
