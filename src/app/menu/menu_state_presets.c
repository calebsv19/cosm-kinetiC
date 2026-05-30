#include "app/menu/menu_state_internal.h"

#include <stdio.h>
#include <string.h>

#include "app/editor/scene_editor_retained_document.h"
#include "app/data_paths.h"
#include "app/menu/menu_settings_draft.h"
#include "import/runtime_scene_bridge.h"
#include "render/text_upload_policy.h"

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

int menu_visible_slot_count(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return 0;
    FluidSceneDomainType domain = menu_current_domain(ctx);
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
    FluidSceneDomainType domain = menu_current_domain(ctx);
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
    return slot->preset.domain == menu_current_domain(ctx);
}

int menu_visible_row_from_slot(const SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return -1;
    if (slot_index < 0 || slot_index >= preset_library_count(ctx->library)) return -1;
    FluidSceneDomainType domain = menu_current_domain(ctx);
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

void menu_select_custom(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library || !ctx->selection) return;
    int count = preset_library_count(ctx->library);
    if (count <= 0) {
        ctx->selection->custom_slot_index = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    if (slot_index < 0 || slot_index >= count || !menu_slot_matches_current_mode(ctx, slot_index)) {
        slot_index = find_first_slot_for_mode(ctx, menu_current_domain(ctx));
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
    FluidSceneDomainType domain = menu_current_domain(ctx);
    const FluidScenePreset *base = scene_presets_get_default_for_domain(domain);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     base);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.domain = domain;
    menu_assign_structural_preset_path(slot, preset_library_count(ctx->library) - 1);
    menu_select_custom(ctx, preset_library_count(ctx->library) - 1);
}

void menu_scroll_to_row(SceneMenuInteraction *ctx, int row_index) {
    if (!ctx) return;
    int row_height = PRESET_ROW_HEIGHT;
    if (ctx->font) {
        int font_h = TTF_FontHeight(ctx->font);
        if (font_h > 0) {
            font_h = physics_sim_text_logical_pixels(ctx->renderer, font_h);
            int scaled = font_h + 24;
            if (scaled > row_height) row_height = scaled;
        }
    }
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
    FluidSceneDomainType domain = menu_current_domain(ctx);
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
        CustomPresetSlot *slot = preset_library_get_slot(ctx->library, ctx->renaming_slot);
        if (slot) {
            const char *value = text_input_value(&ctx->rename_input);
            if (!value || value[0] == '\0') {
                snprintf(slot->name, sizeof(slot->name), "Custom Slot %d", ctx->renaming_slot + 1);
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
