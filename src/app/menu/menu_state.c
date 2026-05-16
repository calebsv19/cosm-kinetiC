#include "app/menu/menu_state.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/editor/scene_editor_retained_document.h"
#include "app/quality_profiles.h"
#include "app/scene_controller.h"
#include "app/data_paths.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_render.h"
#include "import/runtime_scene_bridge.h"
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
    memset(&ctx->editor_bootstrap.retained_scene, 0, sizeof(ctx->editor_bootstrap.retained_scene));
    ctx->editor_bootstrap.has_retained_scene = false;
    ctx->editor_bootstrap.retained_runtime_scene_path[0] = '\0';
    ctx->retained_runtime_scene_path[0] = '\0';
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

static void menu_reload_settings_from_active_preset(SceneMenuInteraction *ctx) {
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
}

const char *menu_mode_label(SimulationMode mode) {
    switch (mode) {
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

int menu_visible_entry_count(const SceneMenuInteraction *ctx) {
    if (!ctx) return 0;
    if (menu_showing_retained_catalog(ctx)) {
        return ctx->scene_library.retained_scenes.count;
    }
    return menu_visible_slot_count(ctx);
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
    menu_refresh_scene_library(ctx);
    menu_reload_settings_from_active_preset(ctx);
}

bool menu_select_retained_scene(SceneMenuInteraction *ctx, int retained_scene_index) {
    const PhysicsSimSceneLibraryEntry *entry = NULL;
    RuntimeSceneBridgePreflight summary = {0};
    AppConfig cfg_copy = {0};
    FluidScenePreset projected = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};

    if (!ctx || !ctx->cfg || !ctx->selection) return false;
    if (ctx->scene_library.retained_scenes.count <= 0) {
        menu_clear_retained_scene_selection(ctx);
        menu_refresh_scene_library(ctx);
        return false;
    }
    if (retained_scene_index < 0 ||
        retained_scene_index >= ctx->scene_library.retained_scenes.count) {
        retained_scene_index = ctx->scene_library.retained_scenes.selected_index;
    }
    if (retained_scene_index < 0 ||
        retained_scene_index >= ctx->scene_library.retained_scenes.count) {
        retained_scene_index = 0;
    }

    entry = &ctx->scene_library.retained_scenes.entries[retained_scene_index];
    cfg_copy = *ctx->cfg;
    projected = ctx->preset_output ? *ctx->preset_output : ctx->preview_preset;
    if (!runtime_scene_bridge_apply_file(entry->source_path,
                                         &cfg_copy,
                                         &projected,
                                         &summary)) {
        menu_set_status(ctx, summary.diagnostics[0] ? summary.diagnostics : "Failed to open retained scene.", true);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        menu_set_status(ctx,
                        retained.diagnostics[0] ? retained.diagnostics : "Retained scene contract invalid.",
                        true);
        return false;
    }

    ctx->selection->retained_scene_index = retained_scene_index;
    snprintf(ctx->selection->retained_runtime_scene_path,
             sizeof(ctx->selection->retained_runtime_scene_path),
             "%s",
             entry->source_path);
    ctx->scene_library.retained_scenes.selected_index = retained_scene_index;
    ctx->preview_preset = projected;
    ctx->active_preset = &ctx->preview_preset;
    memset(&ctx->editor_bootstrap, 0, sizeof(ctx->editor_bootstrap));
    ctx->editor_bootstrap.has_retained_scene = true;
    ctx->editor_bootstrap.retained_scene = retained;
    snprintf(ctx->editor_bootstrap.retained_runtime_scene_path,
             sizeof(ctx->editor_bootstrap.retained_runtime_scene_path),
             "%s",
             entry->source_path);
    snprintf(ctx->retained_runtime_scene_path,
             sizeof(ctx->retained_runtime_scene_path),
             "%s",
             entry->source_path);
    menu_refresh_scene_library(ctx);
    menu_reload_settings_from_active_preset(ctx);
    return true;
}

bool menu_duplicate_retained_scene(SceneMenuInteraction *ctx) {
    const PhysicsSimSceneLibraryEntry *entry = NULL;
    char duplicate_path[512];
    char diagnostics[256];
    int retained_index = -1;
    if (!ctx) return false;
    if (!menu_showing_retained_catalog(ctx)) return false;
    if (ctx->scene_library.retained_scenes.count <= 0) return false;
    retained_index = ctx->selection ? ctx->selection->retained_scene_index : -1;
    if (retained_index < 0 || retained_index >= ctx->scene_library.retained_scenes.count) {
        retained_index = ctx->scene_library.retained_scenes.selected_index;
    }
    if (retained_index < 0 || retained_index >= ctx->scene_library.retained_scenes.count) {
        return false;
    }
    entry = &ctx->scene_library.retained_scenes.entries[retained_index];
    if (!physics_sim_ensure_runtime_dirs()) {
        menu_set_status(ctx, "Failed to ensure runtime scene directories.", true);
        return false;
    }
    if (!scene_editor_retained_document_duplicate_scene_file(entry->source_path,
                                                             physics_sim_default_runtime_scene_user_dir(),
                                                             duplicate_path,
                                                             sizeof(duplicate_path),
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        menu_set_status(ctx,
                        diagnostics[0] ? diagnostics : "Failed to duplicate retained scene.",
                        true);
        return false;
    }
    snprintf(ctx->retained_runtime_scene_path,
             sizeof(ctx->retained_runtime_scene_path),
             "%s",
             duplicate_path);
    menu_refresh_scene_library(ctx);
    retained_index = physics_sim_editor_scene_library_find_retained_index_by_path(&ctx->scene_library,
                                                                                  duplicate_path);
    if (retained_index >= 0) {
        (void)menu_select_retained_scene(ctx, retained_index);
    }
    menu_set_status(ctx,
                    diagnostics[0] ? diagnostics : "Retained scene duplicated.",
                    false);
    return true;
}

void menu_ensure_slot_for_mode(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    if (menu_showing_retained_catalog(ctx)) {
        menu_refresh_scene_library(ctx);
        if (ctx->scene_library.retained_scenes.count > 0) {
            (void)menu_select_retained_scene(ctx, ctx->selection ? ctx->selection->retained_scene_index : -1);
        }
        return;
    }
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

void menu_scroll_to_row(SceneMenuInteraction *ctx, int row_index) {
    if (!ctx) return;
    int row_height = menu_preset_row_height(ctx);
    float row_top = (float)row_index * (float)row_height;
    float row_bottom = row_top + (float)row_height;
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
    if (cfg->grid_d < 0) cfg->grid_d = 0;
    if (cfg->grid_d > 256) cfg->grid_d = 256;
}

void menu_begin_headless_frames_edit(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    int frames = draft ? draft->headless_frame_count : 0;
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
                menu_settings_shell_set_headless_frame_count(&ctx->settings_shell, (int)frames);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->headless_frames_input);
    ctx->editing_headless_frames = false;
}

void menu_begin_viscosity_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    float v = draft ? draft->velocity_damping : 0.0f;
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
                menu_settings_shell_set_velocity_damping(&ctx->settings_shell, (float)v);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->viscosity_input);
    ctx->editing_viscosity = false;
}

void menu_begin_inflow_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    float v = draft ? draft->tunnel_inflow_speed : 0.0f;
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
                menu_settings_shell_set_tunnel_inflow_speed(&ctx->settings_shell, (float)v);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->inflow_input);
    ctx->editing_inflow = false;
}

void menu_begin_input_root_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    text_input_begin(&ctx->input_root_input,
                     ctx->cfg->input_root,
                     sizeof(ctx->cfg->input_root) - 1);
    ctx->editing_input_root = true;
}

void menu_finish_input_root_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_input_root) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->input_root_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                snprintf(ctx->cfg->input_root,
                         sizeof(ctx->cfg->input_root),
                         "%s",
                         physics_sim_default_input_root());
            } else {
                snprintf(ctx->cfg->input_root,
                         sizeof(ctx->cfg->input_root),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->input_root_input);
    ctx->editing_input_root = false;
}

void menu_begin_output_root_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    text_input_begin(&ctx->output_root_input,
                     ctx->cfg->headless_output_dir,
                     sizeof(ctx->cfg->headless_output_dir) - 1);
    ctx->editing_output_root = true;
}

void menu_finish_output_root_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_output_root) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->output_root_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                snprintf(ctx->cfg->headless_output_dir,
                         sizeof(ctx->cfg->headless_output_dir),
                         "%s",
                         physics_sim_default_snapshot_dir());
            } else {
                snprintf(ctx->cfg->headless_output_dir,
                         sizeof(ctx->cfg->headless_output_dir),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->output_root_input);
    ctx->editing_output_root = false;
}

void menu_begin_warm_start_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg || !menu_warm_start_controls_visible(ctx)) return;
    text_input_begin(&ctx->warm_start_input,
                     ctx->cfg->atmospheric_warm_start_path,
                     sizeof(ctx->cfg->atmospheric_warm_start_path) - 1);
    ctx->editing_warm_start = true;
}

void menu_finish_warm_start_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_warm_start) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->warm_start_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                ctx->cfg->atmospheric_warm_start_path[0] = '\0';
            } else {
                snprintf(ctx->cfg->atmospheric_warm_start_path,
                         sizeof(ctx->cfg->atmospheric_warm_start_path),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->warm_start_input);
    ctx->editing_warm_start = false;
}
