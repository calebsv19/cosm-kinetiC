#ifndef MENU_STATE_H
#define MENU_STATE_H

#include "app/menu/menu_types.h"

bool menu_point_in_rect(int x, int y, const SDL_Rect *rect);

void menu_clamp_grid_size(AppConfig *cfg);
void menu_apply_quality_profile_index(SceneMenuInteraction *ctx, int index);
void menu_set_custom_quality(SceneMenuInteraction *ctx);
void menu_cycle_quality(SceneMenuInteraction *ctx, int delta);
const char *menu_current_quality_name(const SceneMenuInteraction *ctx);

void menu_set_status(SceneMenuInteraction *ctx, const char *text, bool wait_ack);
void menu_clear_status(SceneMenuInteraction *ctx);

void menu_run_headless_batch(SceneMenuInteraction *ctx);

SimulationMode menu_normalize_sim_mode(SimulationMode mode);
const char *menu_mode_label(SimulationMode mode);
SpaceMode menu_normalize_space_mode(SpaceMode mode);
const char *menu_space_mode_label(SpaceMode mode);

int menu_visible_slot_count(const SceneMenuInteraction *ctx);
int menu_slot_index_from_visible_row(const SceneMenuInteraction *ctx, int row_index);
int menu_visible_row_from_slot(const SceneMenuInteraction *ctx, int slot_index);
bool menu_slot_matches_current_mode(const SceneMenuInteraction *ctx, int slot_index);
float menu_preset_total_height(const SceneMenuInteraction *ctx);

int menu_preset_index_from_point(SceneMenuInteraction *ctx,
                                 int x,
                                 int y,
                                 bool *is_add_entry);
bool menu_preset_row_rect(SceneMenuInteraction *ctx,
                          int row_index,
                          bool is_add_entry,
                          SDL_Rect *out_rect);
SDL_Rect menu_preset_delete_button_rect(const SDL_Rect *row_rect);

void menu_select_custom(SceneMenuInteraction *ctx, int slot_index);
void menu_ensure_slot_for_mode(SceneMenuInteraction *ctx);
void menu_switch_mode(SceneMenuInteraction *ctx, SimulationMode new_mode);
void menu_scroll_to_row(SceneMenuInteraction *ctx, int row_index);
void menu_scroll_to_slot(SceneMenuInteraction *ctx, int slot_index);
void menu_add_new_preset(SceneMenuInteraction *ctx);
void menu_delete_preset(SceneMenuInteraction *ctx, int slot_index);

void menu_begin_rename(SceneMenuInteraction *ctx, int slot_index);
void menu_finish_rename(SceneMenuInteraction *ctx, bool apply);

void menu_begin_headless_frames_edit(SceneMenuInteraction *ctx);
void menu_finish_headless_frames_edit(SceneMenuInteraction *ctx, bool apply);
void menu_begin_viscosity_edit(SceneMenuInteraction *ctx);
void menu_finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply);
void menu_begin_inflow_edit(SceneMenuInteraction *ctx);
void menu_finish_inflow_edit(SceneMenuInteraction *ctx, bool apply);
void menu_begin_input_root_edit(SceneMenuInteraction *ctx);
void menu_finish_input_root_edit(SceneMenuInteraction *ctx, bool apply);
void menu_begin_output_root_edit(SceneMenuInteraction *ctx);
void menu_finish_output_root_edit(SceneMenuInteraction *ctx, bool apply);

void menu_assign_structural_preset_path(CustomPresetSlot *slot, int index);

#endif
