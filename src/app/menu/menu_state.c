#include "app/menu/menu_state.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/quality_profiles.h"
#include "app/scene_controller.h"

static int quality_count(void) {
    return quality_profile_count();
}

void menu_apply_quality_profile_index(SceneMenuInteraction *ctx, int index) {
    if (!ctx || !ctx->cfg) return;
    quality_profile_apply(ctx->cfg, index);
    menu_clamp_grid_size(ctx->cfg);
    ctx->quality_index = (index >= 0 && index < quality_count()) ? index : -1;
    if (ctx->selection) {
        ctx->selection->quality_index = ctx->quality_index;
    }
}

void menu_set_custom_quality(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    ctx->quality_index = -1;
    ctx->cfg->quality_index = -1;
    if (ctx->selection) {
        ctx->selection->quality_index = -1;
    }
}

void menu_cycle_quality(SceneMenuInteraction *ctx, int delta) {
    if (!ctx) return;
    int count = quality_count();
    if (count <= 0) return;
    int current = ctx->quality_index;
    if (current < 0) current = 0;
    current = (current + delta + count) % count;
    menu_apply_quality_profile_index(ctx, current);
}

const char *menu_current_quality_name(const SceneMenuInteraction *ctx) {
    if (!ctx) return "Custom";
    return quality_profile_name(ctx->quality_index);
}

void menu_run_headless_batch(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    if (ctx->active_mode == SIM_MODE_STRUCTURAL) {
        menu_set_status(ctx, "Headless runs are fluid-only.", true);
        return;
    }
    AppConfig cfg_copy = *ctx->cfg;
    if (ctx->quality_index >= 0) {
        quality_profile_apply(&cfg_copy, ctx->quality_index);
    }
    HeadlessOptions opts = {
        .enabled = true,
        .frame_limit = cfg_copy.headless_frame_count,
        .skip_present = cfg_copy.headless_skip_present,
        .ignore_input = false,
        .preserve_sdl_state = true
    };
    const char *output_dir = cfg_copy.headless_output_dir[0]
                                 ? cfg_copy.headless_output_dir
                                 : "data/snapshots";
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library,
                                                     ctx->selection->custom_slot_index);
    FluidScenePreset preset_copy = slot ? slot->preset : ctx->preview_preset;
    int result = scene_controller_run(&cfg_copy, &preset_copy, ctx->shape_library, output_dir, &opts);
    if (result == 2) {
        menu_set_status(ctx, "Headless run canceled.", true);
    } else {
        menu_set_status(ctx, "Headless run complete.", true);
    }
}

void menu_set_status(SceneMenuInteraction *ctx, const char *text, bool wait_ack) {
    if (!ctx || !text) return;
    snprintf(ctx->status_text, sizeof(ctx->status_text), "%s", text);
    ctx->status_visible = true;
    ctx->status_wait_ack = wait_ack;
}

void menu_clear_status(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    ctx->status_text[0] = '\0';
    ctx->status_visible = false;
    ctx->status_wait_ack = false;
}

bool menu_point_in_rect(int x, int y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

SimulationMode menu_normalize_sim_mode(SimulationMode mode) {
    if (mode < SIM_MODE_BOX || mode >= SIMULATION_MODE_COUNT) {
        return SIM_MODE_BOX;
    }
    return mode;
}

static FluidSceneDomainType domain_for_mode(SimulationMode mode) {
    switch (mode) {
    case SIM_MODE_WIND_TUNNEL:
        return SCENE_DOMAIN_WIND_TUNNEL;
    case SIM_MODE_STRUCTURAL:
        return SCENE_DOMAIN_STRUCTURAL;
    default:
        return SCENE_DOMAIN_BOX;
    }
}

static FluidSceneDomainType current_domain(const SceneMenuInteraction *ctx) {
    if (!ctx) return SCENE_DOMAIN_BOX;
    return domain_for_mode(menu_normalize_sim_mode(ctx->active_mode));
}

const char *menu_mode_label(SimulationMode mode) {
    switch (mode) {
    case SIM_MODE_WIND_TUNNEL:
        return "Wind Tunnel";
    case SIM_MODE_STRUCTURAL:
        return "Structural";
    default:
        return "Grid";
    }
}

static const char *structural_default_scene_path(void) {
    return "config/structural_scene.txt";
}

void menu_assign_structural_preset_path(CustomPresetSlot *slot, int index) {
    if (!slot) return;
    if (slot->preset.domain != SCENE_DOMAIN_STRUCTURAL) return;
    if (slot->preset.structural_scene_path[0] != '\0' &&
        strcmp(slot->preset.structural_scene_path, structural_default_scene_path()) != 0) {
        return;
    }
    snprintf(slot->preset.structural_scene_path,
             sizeof(slot->preset.structural_scene_path),
             "config/structural_preset_%02d.txt",
             index + 1);
}

int menu_visible_slot_count(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return 0;
    FluidSceneDomainType domain = current_domain(ctx);
    int total = preset_library_count(ctx->library);
    int visible = 0;
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (slot && slot->preset.domain == domain) {
            ++visible;
        }
    }
    return visible;
}

int menu_slot_index_from_visible_row(const SceneMenuInteraction *ctx, int row_index) {
    if (!ctx || !ctx->library || row_index < 0) return -1;
    FluidSceneDomainType domain = current_domain(ctx);
    int total = preset_library_count(ctx->library);
    int visible = 0;
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (!slot) continue;
        if (slot->preset.domain != domain) continue;
        if (visible == row_index) {
            return i;
        }
        ++visible;
    }
    return -1;
}

bool menu_slot_matches_current_mode(const SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return false;
    const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, slot_index);
    if (!slot) return false;
    return slot->preset.domain == current_domain(ctx);
}

static int find_first_slot_for_mode(const SceneMenuInteraction *ctx, FluidSceneDomainType domain) {
    if (!ctx || !ctx->library) return -1;
    int total = preset_library_count(ctx->library);
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (slot && slot->preset.domain == domain) {
            return i;
        }
    }
    return -1;
}

int menu_visible_row_from_slot(const SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return -1;
    if (slot_index < 0 || slot_index >= preset_library_count(ctx->library)) return -1;
    FluidSceneDomainType domain = current_domain(ctx);
    int visible = 0;
    for (int i = 0; i < preset_library_count(ctx->library); ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (!slot) continue;
        if (slot->preset.domain != domain) continue;
        if (i == slot_index) {
            return visible;
        }
        ++visible;
    }
    return -1;
}

float menu_preset_total_height(const SceneMenuInteraction *ctx) {
    if (!ctx) return (float)PRESET_ROW_HEIGHT;
    int count = menu_visible_slot_count(ctx);
    if (count < 0) count = 0;
    return (float)count * (float)PRESET_ROW_HEIGHT +
           ADD_ENTRY_GAP +
           (float)PRESET_ROW_HEIGHT;
}

int menu_preset_index_from_point(SceneMenuInteraction *ctx,
                                 int x,
                                 int y,
                                 bool *is_add_entry) {
    if (!ctx) return -1;
    if (!menu_point_in_rect(x, y, &ctx->list_rect)) {
        if (is_add_entry) *is_add_entry = false;
        return -1;
    }

    float local_y = (float)(y - ctx->list_rect.y) + scrollbar_offset(&ctx->scrollbar);
    if (local_y < 0.0f) local_y = 0.0f;
    int count = menu_visible_slot_count(ctx);
    float add_start = (float)count * (float)PRESET_ROW_HEIGHT + (float)ADD_ENTRY_GAP;
    float add_end = add_start + (float)PRESET_ROW_HEIGHT;
    if (local_y >= add_start) {
        bool inside_add = local_y < add_end;
        if (is_add_entry) *is_add_entry = inside_add;
        return inside_add ? count : -1;
    }

    int row = (int)(local_y / (float)PRESET_ROW_HEIGHT);
    if (row < 0 || row >= count) {
        if (is_add_entry) *is_add_entry = false;
        return -1;
    }
    if (is_add_entry) *is_add_entry = false;
    return row;
}

bool menu_preset_row_rect(SceneMenuInteraction *ctx,
                          int row_index,
                          bool is_add_entry,
                          SDL_Rect *out_rect) {
    if (!ctx || !out_rect) return false;
    float offset = scrollbar_offset(&ctx->scrollbar);
    float y = (float)ctx->list_rect.y +
              (float)row_index * (float)PRESET_ROW_HEIGHT -
              offset;
    if (is_add_entry) {
        y += (float)ADD_ENTRY_GAP;
    }
    SDL_Rect rect = {
        .x = ctx->list_rect.x + 8,
        .y = (int)(y + 6.0f),
        .w = ctx->list_rect.w - SCROLLBAR_WIDTH - 16,
        .h = PRESET_ROW_HEIGHT - 12
    };
    *out_rect = rect;
    return rect.y + rect.h >= ctx->list_rect.y &&
           rect.y <= ctx->list_rect.y + ctx->list_rect.h;
}

SDL_Rect menu_preset_delete_button_rect(const SDL_Rect *row_rect) {
    SDL_Rect rect = {
        .x = row_rect->x + row_rect->w - 34,
        .y = row_rect->y + 8,
        .w = 26,
        .h = row_rect->h - 16
    };
    return rect;
}

void menu_select_custom(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library || !ctx->selection) return;
    int count = preset_library_count(ctx->library);
    if (count <= 0) {
        ctx->selection->custom_slot_index = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    if (slot_index < 0 || slot_index >= count || !menu_slot_matches_current_mode(ctx, slot_index)) {
        slot_index = find_first_slot_for_mode(ctx, current_domain(ctx));
    }
    if (slot_index < 0 || slot_index >= count) {
        ctx->selection->custom_slot_index = -1;
        ctx->selection->last_mode_slot[ctx->active_mode] = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }

    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    if (!slot) {
        ctx->selection->custom_slot_index = -1;
        ctx->selection->last_mode_slot[ctx->active_mode] = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    ctx->selection->custom_slot_index = slot_index;
    ctx->library->active_slot = slot_index;
    ctx->selection->last_mode_slot[ctx->active_mode] = slot_index;
    ctx->active_preset = &slot->preset;
}

void menu_ensure_slot_for_mode(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    if (menu_visible_slot_count(ctx) > 0) return;
    char default_name[CUSTOM_PRESET_NAME_MAX];
    int slot_count = preset_library_count(ctx->library);
    snprintf(default_name, sizeof(default_name), "%s Preset %d",
             menu_mode_label(ctx->active_mode),
             slot_count + 1);
    FluidSceneDomainType domain = current_domain(ctx);
    const FluidScenePreset *base = scene_presets_get_default_for_domain(domain);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     base);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.domain = domain;
    menu_assign_structural_preset_path(slot, preset_library_count(ctx->library) - 1);
    int new_index = preset_library_count(ctx->library) - 1;
    menu_select_custom(ctx, new_index);
}

void menu_switch_mode(SceneMenuInteraction *ctx, SimulationMode new_mode) {
    if (!ctx) return;
    SimulationMode normalized = menu_normalize_sim_mode(new_mode);
    if (ctx->active_mode == normalized) return;
    if (ctx->rename_input.active) {
        menu_finish_rename(ctx, false);
    }
    ctx->active_mode = normalized;
    if (ctx->cfg) ctx->cfg->sim_mode = normalized;
    if (ctx->selection) ctx->selection->sim_mode = normalized;
    menu_ensure_slot_for_mode(ctx);
    int preferred = -1;
    if (ctx->selection &&
        normalized >= 0 && normalized < SIMULATION_MODE_COUNT) {
        preferred = ctx->selection->last_mode_slot[normalized];
    }
    menu_select_custom(ctx, preferred);
    scrollbar_set_offset(&ctx->scrollbar, 0.0f);
    ctx->hover_slot = -1;
    ctx->hover_add_entry = false;
    ctx->last_clicked_slot = -1;
}

void menu_scroll_to_row(SceneMenuInteraction *ctx, int row_index) {
    if (!ctx) return;
    float row_top = (float)row_index * (float)PRESET_ROW_HEIGHT;
    float row_bottom = row_top + (float)PRESET_ROW_HEIGHT;
    float offset = scrollbar_offset(&ctx->scrollbar);
    float view = (float)ctx->list_rect.h;

    if (row_top < offset) {
        scrollbar_set_offset(&ctx->scrollbar, row_top);
    } else if (row_bottom > offset + view) {
        scrollbar_set_offset(&ctx->scrollbar, row_bottom - view);
    }
}

void menu_scroll_to_slot(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx) return;
    int row = menu_visible_row_from_slot(ctx, slot_index);
    if (row >= 0) {
        menu_scroll_to_row(ctx, row);
    }
}

void menu_add_new_preset(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    char default_name[CUSTOM_PRESET_NAME_MAX];
    int slot_count = preset_library_count(ctx->library);
    snprintf(default_name, sizeof(default_name), "Custom Preset %d", slot_count + 1);
    FluidSceneDomainType domain = current_domain(ctx);
    const FluidScenePreset *base = scene_presets_get_default_for_domain(domain);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     base);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.domain = domain;
    slot->occupied = true;
    int new_index = preset_library_count(ctx->library) - 1;
    menu_assign_structural_preset_path(slot, new_index);
    menu_select_custom(ctx, new_index);
    menu_scroll_to_slot(ctx, new_index);
    menu_begin_rename(ctx, new_index);
}

static void adjust_slot_indices_after_delete(SceneMenuInteraction *ctx, int removed_index) {
    if (!ctx) return;
    if (ctx->selection) {
        for (int mode = 0; mode < SIMULATION_MODE_COUNT; ++mode) {
            int stored = ctx->selection->last_mode_slot[mode];
            if (stored == removed_index) {
                ctx->selection->last_mode_slot[mode] = -1;
            } else if (stored > removed_index) {
                ctx->selection->last_mode_slot[mode] = stored - 1;
            }
        }
        if (ctx->selection->custom_slot_index == removed_index) {
            ctx->selection->custom_slot_index = -1;
        } else if (ctx->selection->custom_slot_index > removed_index) {
            ctx->selection->custom_slot_index--;
        }
    }
    if (ctx->library) {
        if (ctx->library->active_slot == removed_index) {
            ctx->library->active_slot = -1;
        } else if (ctx->library->active_slot > removed_index) {
            ctx->library->active_slot--;
        }
    }
    if (ctx->hover_slot == removed_index) {
        ctx->hover_slot = -1;
    } else if (ctx->hover_slot > removed_index) {
        ctx->hover_slot--;
    }
    if (ctx->last_clicked_slot == removed_index) {
        ctx->last_clicked_slot = -1;
    } else if (ctx->last_clicked_slot > removed_index) {
        ctx->last_clicked_slot--;
    }
}

void menu_delete_preset(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return;
    if (ctx->rename_input.active) {
        menu_finish_rename(ctx, false);
    }
    if (!preset_library_remove_slot(ctx->library, slot_index)) {
        return;
    }
    adjust_slot_indices_after_delete(ctx, slot_index);
    int count = preset_library_count(ctx->library);
    if (count == 0) {
        ctx->selection->custom_slot_index = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    int new_index = ctx->selection->custom_slot_index;
    if (new_index >= count) new_index = count - 1;
    menu_select_custom(ctx, new_index);
}

void menu_begin_rename(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx) return;
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    const char *initial = slot ? slot->name : "";
    ctx->renaming_slot = slot_index;
    text_input_begin(&ctx->rename_input,
                     initial,
                     CUSTOM_PRESET_NAME_MAX - 1);
}

void menu_finish_rename(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx) return;
    if (!ctx->rename_input.active || ctx->renaming_slot < 0) {
        return;
    }
    if (apply) {
        CustomPresetSlot *slot =
            preset_library_get_slot(ctx->library, ctx->renaming_slot);
        if (slot) {
            const char *value = text_input_value(&ctx->rename_input);
            if (!value || value[0] == '\0') {
                snprintf(slot->name, sizeof(slot->name),
                         "Custom Slot %d", ctx->renaming_slot + 1);
            } else {
                snprintf(slot->name, sizeof(slot->name), "%s", value);
            }
            slot->preset.name = slot->name;
            slot->occupied = true;
        }
    }
    text_input_end(&ctx->rename_input);
    ctx->renaming_slot = -1;
}

void menu_clamp_grid_size(AppConfig *cfg) {
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_w > 512) cfg->grid_w = 512;
    if (cfg->grid_h > 512) cfg->grid_h = 512;
}

void menu_begin_headless_frames_edit(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    char buffer[32];
    int frames = ctx->cfg ? ctx->cfg->headless_frame_count : 0;
    if (frames < 0) frames = 0;
    snprintf(buffer, sizeof(buffer), "%d", frames);
    text_input_begin(&ctx->headless_frames_input, buffer, sizeof(buffer) - 1);
    ctx->editing_headless_frames = true;
}

void menu_finish_headless_frames_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_headless_frames) return;
    if (apply) {
        const char *value = text_input_value(&ctx->headless_frames_input);
        if (value && value[0]) {
            char *end = NULL;
            long frames = strtol(value, &end, 10);
            if (end != value) {
                if (frames < 0) frames = 0;
                if (ctx->cfg) ctx->cfg->headless_frame_count = (int)frames;
                if (ctx->selection) ctx->selection->headless_frame_count = (int)frames;
            }
        }
    }
    text_input_end(&ctx->headless_frames_input);
    ctx->editing_headless_frames = false;
}

void menu_begin_viscosity_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    float v = ctx->cfg ? ctx->cfg->velocity_damping : 0.0f;
    if (v < 0.0f) v = 0.0f;
    snprintf(buffer, sizeof(buffer), "%.8f", v);
    text_input_begin(&ctx->viscosity_input, buffer, sizeof(buffer) - 1);
    ctx->editing_viscosity = true;
}

void menu_finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_viscosity) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->viscosity_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                if (v < 0.0) v = 0.0;
                if (v > 0.1) v = 0.1;
                ctx->cfg->velocity_damping = (float)v;
            }
        }
    }
    text_input_end(&ctx->viscosity_input);
    ctx->editing_viscosity = false;
}

void menu_begin_inflow_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    float v = ctx->cfg ? ctx->cfg->tunnel_inflow_speed : 0.0f;
    snprintf(buffer, sizeof(buffer), "%.6f", v);
    text_input_begin(&ctx->inflow_input, buffer, sizeof(buffer) - 1);
    ctx->editing_inflow = true;
}

void menu_finish_inflow_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_inflow) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->inflow_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                if (v < 0.0) v = 0.0;
                if (v > 500.0) v = 500.0;
                ctx->cfg->tunnel_inflow_speed = (float)v;
                if (ctx->selection) {
                    ctx->selection->tunnel_inflow_speed = (float)v;
                }
            }
        }
    }
    text_input_end(&ctx->inflow_input);
    ctx->editing_inflow = false;
}
