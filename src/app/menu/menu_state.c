#include "app/menu/menu_state.h"
#include "app/menu/menu_state_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/quality_profiles.h"
#include "app/platform/physics_sim_path_opener.h"
#include "app/scene_controller.h"
#include "app/scene_project_cache_output.h"
#include "app/data_paths.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_render.h"
#include "render/text_upload_policy.h"

static SpaceMode current_space_mode_for_quality(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return SPACE_MODE_2D;
    return menu_normalize_space_mode(ctx->cfg->space_mode);
}

static int quality_count(const SceneMenuInteraction *ctx) {
    return quality_profile_count_for_space_mode(current_space_mode_for_quality(ctx));
}

static int menu_preset_row_height(const SceneMenuInteraction *ctx) {
    int row_height = PRESET_ROW_HEIGHT;
    if (!ctx || !ctx->font) return row_height;
    {
        int font_h = TTF_FontHeight(ctx->font);
        if (font_h > 0) {
            font_h = physics_sim_text_logical_pixels(ctx->renderer, font_h);
            int scaled = font_h + 24;
            if (scaled > row_height) row_height = scaled;
        }
    }
    return row_height;
}

void menu_apply_quality_profile_index(SceneMenuInteraction *ctx, int index) {
    if (!ctx) return;
    menu_settings_shell_apply_quality(&ctx->settings_shell, index);
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
}

void menu_set_custom_quality(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    menu_settings_shell_set_custom_quality(&ctx->settings_shell);
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
}

void menu_cycle_quality(SceneMenuInteraction *ctx, int delta) {
    if (!ctx) return;
    int count = quality_count(ctx);
    if (count <= 0) return;
    int current = ctx->quality_index;
    if (current < 0) current = 0;
    current = (current + delta + count) % count;
    menu_apply_quality_profile_index(ctx, current);
}

const char *menu_current_quality_name(const SceneMenuInteraction *ctx) {
    if (!ctx) return "Custom";
    return menu_settings_render_quality_name(&ctx->settings_shell);
}

void menu_run_headless_batch(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    if (ctx->active_mode == SIM_MODE_STRUCTURAL) {
        menu_set_status(ctx, "Headless runs are fluid-only.", true);
        return;
    }
    AppConfig cfg_copy = *ctx->cfg;
    if (ctx->quality_index >= 0) {
        quality_profile_apply_for_space_mode(&cfg_copy,
                                             current_space_mode_for_quality(ctx),
                                             ctx->quality_index);
    }
    HeadlessOptions opts = {
        .enabled = true,
        .frame_limit = cfg_copy.headless_frame_count,
        .sim_steps_per_frame = 1,
        .skip_present = cfg_copy.headless_skip_present,
        .ignore_input = false,
        .preserve_sdl_state = true
    };
    SceneRuntimeLaunch runtime_launch = {0};
    const char *output_dir = physics_sim_resolve_snapshot_output_dir(cfg_copy.headless_output_dir);
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library,
                                                     ctx->selection->custom_slot_index);
    FluidScenePreset preset_copy = slot ? slot->preset : ctx->preview_preset;
    if (menu_showing_retained_catalog(ctx) && ctx->retained_runtime_scene_path[0]) {
        runtime_launch.has_retained_scene = true;
        snprintf(runtime_launch.retained_runtime_scene_path,
                 sizeof(runtime_launch.retained_runtime_scene_path),
                 "%s",
                 ctx->retained_runtime_scene_path);
    }
    int result = scene_controller_run(&cfg_copy,
                                      &preset_copy,
                                      runtime_launch.has_retained_scene ? &runtime_launch : NULL,
                                      ctx->shape_library,
                                      output_dir,
                                      &opts);
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

void menu_clear_retained_scene_selection(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    if (ctx->selection) {
        ctx->selection->retained_scene_index = -1;
        ctx->selection->retained_runtime_scene_path[0] = '\0';
    }
    if (ctx->cfg) {
        ctx->cfg->retained_runtime_scene_path[0] = '\0';
    }
    memset(&ctx->editor_bootstrap.retained_scene, 0, sizeof(ctx->editor_bootstrap.retained_scene));
    ctx->editor_bootstrap.has_retained_scene = false;
    ctx->editor_bootstrap.retained_runtime_scene_path[0] = '\0';
    ctx->retained_runtime_scene_path[0] = '\0';
    memset(&ctx->scene_project_cache_status, 0, sizeof(ctx->scene_project_cache_status));
}

void menu_update_scene_project_cache_status(SceneMenuInteraction *ctx) {
    char error[160];
    if (!ctx) return;
    memset(&ctx->scene_project_cache_status, 0, sizeof(ctx->scene_project_cache_status));
    if (!menu_showing_retained_catalog(ctx) || !ctx->retained_runtime_scene_path[0]) {
        return;
    }
    if (!scene_project_cache_output_status_from_runtime_scene(ctx->retained_runtime_scene_path,
                                                              &ctx->scene_project_cache_status,
                                                              error,
                                                              sizeof(error))) {
        return;
    }
    if (ctx->cfg) {
        (void)scene_project_cache_output_make_update_command(
            ctx->scene_project_cache_status.project_root,
            ctx->cfg->headless_frame_count,
            ctx->cfg->grid_w,
            ctx->cfg->grid_h,
            ctx->cfg->grid_d,
            ctx->scene_project_cache_status.update_command,
            sizeof(ctx->scene_project_cache_status.update_command));
    }
}

bool menu_has_scene_project_cache_status(const SceneMenuInteraction *ctx) {
    return ctx && ctx->scene_project_cache_status.is_scene_project;
}

bool menu_copy_scene_project_cache_command(SceneMenuInteraction *ctx) {
    if (!menu_has_scene_project_cache_status(ctx) ||
        !ctx->scene_project_cache_status.update_command[0]) {
        if (ctx) menu_set_status(ctx, "No scene-project cache command for this selection.", false);
        return false;
    }
    if (SDL_SetClipboardText(ctx->scene_project_cache_status.update_command) != 0) {
        menu_set_status(ctx, "Failed to copy scene-project cache command.", false);
        return false;
    }
    menu_set_status(ctx, "Scene-project cache update command copied.", false);
    return true;
}

bool menu_show_scene_project_cache(SceneMenuInteraction *ctx) {
    PhysicsSimPathOpenerResult result;
    if (!menu_has_scene_project_cache_status(ctx) ||
        !ctx->scene_project_cache_status.project_root[0]) {
        if (ctx) menu_set_status(ctx, "No scene-project cache root for this selection.", false);
        return false;
    }
    result = PhysicsSim_PathOpener_OpenDirectory(ctx->scene_project_cache_status.project_root);
    if (result == PHYSICS_SIM_PATH_OPENER_OPENED) {
        menu_set_status(ctx, "Opened scene-project cache root.", false);
        return true;
    }
    if (result == PHYSICS_SIM_PATH_OPENER_UNAVAILABLE) {
        menu_set_status(ctx, "No system file manager opener is available.", false);
        return false;
    }
    menu_set_status(ctx, "Could not open the scene-project cache root.", false);
    return false;
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
    case SIM_MODE_ATMOSPHERIC:
        return SCENE_DOMAIN_ATMOSPHERIC;
    case SIM_MODE_WATER:
        return SCENE_DOMAIN_WATER;
    case SIM_MODE_WIND_TUNNEL:
        return SCENE_DOMAIN_WIND_TUNNEL;
    case SIM_MODE_STRUCTURAL:
        return SCENE_DOMAIN_STRUCTURAL;
    default:
        return SCENE_DOMAIN_BOX;
    }
}

FluidSceneDomainType menu_current_domain(const SceneMenuInteraction *ctx) {
    if (!ctx) return SCENE_DOMAIN_BOX;
    return domain_for_mode(menu_normalize_sim_mode(ctx->active_mode));
}

void menu_reload_settings_from_active_preset(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    menu_settings_shell_reload_from_runtime(&ctx->settings_shell,
                                            ctx->cfg,
                                            ctx->selection,
                                            ctx->active_preset,
                                            ctx->active_mode,
                                            ctx->cfg->space_mode);
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
}

bool menu_showing_retained_catalog(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return false;
    if (ctx->active_mode == SIM_MODE_WATER) return false;
    return menu_normalize_space_mode(ctx->cfg->space_mode) == SPACE_MODE_3D;
}

bool menu_warm_start_controls_visible(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return false;
    return ctx->active_mode == SIM_MODE_ATMOSPHERIC &&
           menu_normalize_space_mode(ctx->cfg->space_mode) == SPACE_MODE_3D;
}

void menu_refresh_scene_library(SceneMenuInteraction *ctx) {
    const char *current_path = NULL;
    int retained_index = -1;
    if (!ctx) return;
    current_path = ctx->retained_runtime_scene_path[0] ? ctx->retained_runtime_scene_path : NULL;
    physics_sim_editor_scene_library_refresh(&ctx->scene_library,
                                             ctx->active_preset ? ctx->active_preset : ctx->preset_output,
                                             NULL,
                                             ctx->cfg ? ctx->cfg->input_root : NULL,
                                             current_path);
    ctx->scene_library.mode = menu_showing_retained_catalog(ctx)
                                  ? PHYSICS_SIM_SCENE_LIBRARY_MODE_3D
                                  : PHYSICS_SIM_SCENE_LIBRARY_MODE_2D;
    if (ctx->selection && menu_showing_retained_catalog(ctx)) {
        retained_index = physics_sim_editor_scene_library_find_retained_index_by_path(&ctx->scene_library,
                                                                                      current_path);
        if (retained_index >= 0) {
            ctx->selection->retained_scene_index = retained_index;
            ctx->scene_library.retained_scenes.selected_index = retained_index;
        } else {
            if (current_path && current_path[0]) {
                menu_clear_retained_scene_selection(ctx);
            }
            if (ctx->scene_library.retained_scenes.selected_index >= 0) {
                ctx->selection->retained_scene_index = ctx->scene_library.retained_scenes.selected_index;
            } else {
                ctx->selection->retained_scene_index = -1;
            }
        }
    }
    menu_update_scene_project_cache_status(ctx);
}

const char *menu_mode_label(SimulationMode mode) {
    switch (mode) {
    case SIM_MODE_WATER:
        return "Water";
    case SIM_MODE_ATMOSPHERIC:
        return "Atmospheric";
    case SIM_MODE_WIND_TUNNEL:
        return "Wind Tunnel";
    case SIM_MODE_STRUCTURAL:
        return "Structural";
    default:
        return "Grid";
    }
}

SpaceMode menu_normalize_space_mode(SpaceMode mode) {
    if (mode < SPACE_MODE_2D || mode >= SPACE_MODE_COUNT) {
        return SPACE_MODE_2D;
    }
    return mode;
}

const char *menu_space_mode_label(SpaceMode mode) {
    switch (menu_normalize_space_mode(mode)) {
    case SPACE_MODE_3D:
        return "3D (Scaffold)";
    case SPACE_MODE_2D:
    default:
        return "2D";
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

float menu_preset_total_height(const SceneMenuInteraction *ctx) {
    int row_height = menu_preset_row_height(ctx);
    int count = 0;
    if (!ctx) return (float)row_height;
    count = menu_visible_entry_count(ctx);
    if (count < 0) count = 0;
    if (menu_showing_retained_catalog(ctx)) {
        return (float)count * (float)row_height;
    }
    return (float)count * (float)row_height + ADD_ENTRY_GAP + (float)row_height;
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

    int row_height = menu_preset_row_height(ctx);
    float local_y = (float)(y - ctx->list_rect.y) + scrollbar_offset(&ctx->scrollbar);
    if (local_y < 0.0f) local_y = 0.0f;
    int count = menu_visible_entry_count(ctx);
    if (!menu_showing_retained_catalog(ctx)) {
        float add_start = (float)count * (float)row_height + (float)ADD_ENTRY_GAP;
        float add_end = add_start + (float)row_height;
        if (local_y >= add_start) {
            bool inside_add = local_y < add_end;
            if (is_add_entry) *is_add_entry = inside_add;
            return inside_add ? count : -1;
        }
    }

    int row = (int)(local_y / (float)row_height);
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
    int row_height = menu_preset_row_height(ctx);
    int inset = row_height / 10;
    if (inset < 4) inset = 4;
    if (inset > 10) inset = 10;
    float offset = scrollbar_offset(&ctx->scrollbar);
    float y = (float)ctx->list_rect.y +
              (float)row_index * (float)row_height -
              offset;
    if (is_add_entry) {
        y += (float)ADD_ENTRY_GAP;
    }
    SDL_Rect rect = {
        .x = ctx->list_rect.x + 8,
        .y = (int)(y + (float)inset),
        .w = ctx->list_rect.w - SCROLLBAR_WIDTH - 16,
        .h = row_height - inset * 2
    };
    *out_rect = rect;
    return rect.y + rect.h >= ctx->list_rect.y &&
           rect.y <= ctx->list_rect.y + ctx->list_rect.h;
}

SDL_Rect menu_preset_delete_button_rect(const SDL_Rect *row_rect) {
    int size = 24;
    int x = 0;
    int y = 0;
    if (row_rect) {
        size = row_rect->h - 12;
        if (size < 20) size = 20;
        if (size > 34) size = 34;
        x = row_rect->x + row_rect->w - size - 8;
        y = row_rect->y + (row_rect->h - size) / 2;
    }
    SDL_Rect rect = {
        .x = x,
        .y = y,
        .w = size,
        .h = size
    };
    return rect;
}

void menu_switch_mode(SceneMenuInteraction *ctx, SimulationMode new_mode) {
    if (!ctx) return;
    SimulationMode normalized = menu_normalize_sim_mode(new_mode);
    if (ctx->active_mode == normalized) return;
    if (ctx->rename_input.active) {
        menu_finish_rename(ctx, false);
    }
    ctx->active_mode = normalized;
    if (ctx->cfg) {
        ctx->cfg->sim_mode = normalized;
        if (normalized == SIM_MODE_WATER) {
            ctx->cfg->space_mode = SPACE_MODE_3D;
        }
    }
    if (ctx->selection) ctx->selection->sim_mode = normalized;
    menu_settings_shell_sync_provider(&ctx->settings_shell,
                                      normalized,
                                      ctx->cfg ? ctx->cfg->space_mode : SPACE_MODE_2D);
    menu_settings_shell_sync_quality_context(&ctx->settings_shell,
                                             ctx->selection,
                                             ctx->cfg ? ctx->cfg->space_mode : SPACE_MODE_2D);
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
    menu_refresh_scene_library(ctx);
    if (menu_showing_retained_catalog(ctx)) {
        (void)menu_select_retained_scene(ctx,
                                         ctx->selection ? ctx->selection->retained_scene_index : -1);
        scrollbar_set_offset(&ctx->scrollbar, 0.0f);
        ctx->hover_slot = -1;
        ctx->hover_retained_scene_index = -1;
        ctx->hover_add_entry = false;
        ctx->last_clicked_slot = -1;
        return;
    }
    menu_ensure_slot_for_mode(ctx);
    int preferred = -1;
    if (ctx->selection &&
        normalized >= 0 && normalized < SIMULATION_MODE_COUNT) {
        preferred = ctx->selection->last_mode_slot[normalized];
    }
    menu_select_custom(ctx, preferred);
    scrollbar_set_offset(&ctx->scrollbar, 0.0f);
    ctx->hover_slot = -1;
    ctx->hover_retained_scene_index = -1;
    ctx->hover_add_entry = false;
    ctx->last_clicked_slot = -1;
}

void menu_switch_space_mode(SceneMenuInteraction *ctx, SpaceMode new_mode) {
    SpaceMode normalized = menu_normalize_space_mode(new_mode);
    if (!ctx || !ctx->cfg) return;
    if (ctx->active_mode == SIM_MODE_WATER) {
        normalized = SPACE_MODE_3D;
    }
    if (menu_normalize_space_mode(ctx->cfg->space_mode) == normalized) return;
    ctx->cfg->space_mode = normalized;
    menu_settings_shell_sync_provider(&ctx->settings_shell,
                                      ctx->active_mode,
                                      normalized);
    menu_settings_shell_sync_quality_context(&ctx->settings_shell,
                                             ctx->selection,
                                             normalized);
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
    menu_refresh_scene_library(ctx);
    if (menu_showing_retained_catalog(ctx)) {
        (void)menu_select_retained_scene(ctx,
                                         ctx->selection ? ctx->selection->retained_scene_index : -1);
    } else {
        menu_ensure_slot_for_mode(ctx);
        menu_select_custom(ctx, ctx->selection ? ctx->selection->custom_slot_index : -1);
    }
    scrollbar_set_offset(&ctx->scrollbar, 0.0f);
    ctx->hover_slot = -1;
    ctx->hover_retained_scene_index = -1;
    ctx->hover_add_entry = false;
    ctx->last_clicked_slot = -1;
}
