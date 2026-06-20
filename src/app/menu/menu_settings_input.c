#include "app/menu/menu_settings_input.h"

#include <stdio.h>

#include "app/data_paths.h"
#include "config/config_loader.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_layout.h"
#include "app/menu/menu_settings_schema.h"
#include "app/menu/menu_state.h"

typedef struct MenuSettingsRuntimeBinding {
    AppConfig *cfg;
    SceneMenuSelection *selection;
    FluidScenePreset *active_preset;
    SimulationMode active_mode;
    SpaceMode space_mode;
} MenuSettingsRuntimeBinding;

static bool menu_settings_runtime_binding(const SceneMenuInteraction *ctx,
                                          MenuSettingsRuntimeBinding *out) {
    if (!ctx || !ctx->cfg || !out) return false;
    out->cfg = ctx->cfg;
    out->selection = ctx->selection;
    out->active_preset = ctx->active_preset;
    out->active_mode = ctx->active_mode;
    out->space_mode = ctx->cfg->space_mode;
    return true;
}

static void sync_quality_index(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    ctx->quality_index = ctx->settings_shell.draft.quality_index;
}

static void persist_runtime_config(const AppConfig *cfg) {
    const char *runtime_config_path = physics_sim_runtime_config_path();
    if (!cfg) return;
    if (!config_loader_save(cfg, runtime_config_path)) {
        fprintf(stderr, "[menu] Failed to persist runtime config to %s\n",
                runtime_config_path);
    }
}

bool menu_settings_has_pending_changes(const SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return false;
    (void)menu_settings_runtime_binding(ctx, &binding);
    return menu_settings_shell_is_dirty(&ctx->settings_shell,
                                        binding.cfg,
                                        binding.selection,
                                        binding.active_preset);
}

bool menu_settings_saved_differs_from_current(const SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return false;
    (void)menu_settings_runtime_binding(ctx, &binding);
    return menu_settings_shell_saved_differs_from_runtime(&ctx->settings_shell,
                                                          binding.cfg,
                                                          binding.selection,
                                                          binding.active_preset);
}

void menu_settings_commit(SceneMenuInteraction *ctx, bool persist) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return;
    if (!menu_settings_runtime_binding(ctx, &binding)) return;
    menu_settings_shell_apply_to_runtime(&ctx->settings_shell,
                                         binding.cfg,
                                         binding.selection,
                                         binding.active_preset);
    sync_quality_index(ctx);
    if (persist) {
        persist_runtime_config(binding.cfg);
        menu_settings_shell_capture_saved_from_runtime(&ctx->settings_shell,
                                                       binding.cfg,
                                                       binding.selection,
                                                       binding.active_preset,
                                                       binding.active_mode,
                                                       binding.space_mode);
    }
}

void menu_settings_save_current(SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return;
    if (!menu_settings_runtime_binding(ctx, &binding)) return;
    if (menu_settings_has_pending_changes(ctx)) {
        menu_settings_commit(ctx, false);
        if (!menu_settings_runtime_binding(ctx, &binding)) return;
    }
    persist_runtime_config(binding.cfg);
    menu_settings_shell_capture_saved_from_runtime(&ctx->settings_shell,
                                                   binding.cfg,
                                                   binding.selection,
                                                   binding.active_preset,
                                                   binding.active_mode,
                                                   binding.space_mode);
    sync_quality_index(ctx);
}

void menu_settings_restore_saved(SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return;
    if (!menu_settings_runtime_binding(ctx, &binding)) return;
    menu_settings_shell_apply_saved_to_runtime(&ctx->settings_shell,
                                               binding.cfg,
                                               binding.selection,
                                               binding.active_preset);
    menu_settings_shell_restore_saved_to_draft(&ctx->settings_shell,
                                               binding.active_mode,
                                               binding.space_mode);
    sync_quality_index(ctx);
}

void menu_settings_reset_to_runtime(SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return;
    if (!menu_settings_runtime_binding(ctx, &binding)) return;
    menu_settings_shell_reload_from_runtime(&ctx->settings_shell,
                                            binding.cfg,
                                            binding.selection,
                                            binding.active_preset,
                                            binding.active_mode,
                                            binding.space_mode);
    sync_quality_index(ctx);
}

void menu_settings_reset_to_defaults(SceneMenuInteraction *ctx) {
    MenuSettingsRuntimeBinding binding = {0};
    if (!ctx || !ctx->cfg) return;
    if (!menu_settings_runtime_binding(ctx, &binding)) return;
    menu_settings_shell_load_defaults(&ctx->settings_shell,
                                      binding.active_mode,
                                      binding.space_mode);
    sync_quality_index(ctx);
}

bool menu_settings_handle_primary_click(SceneMenuInteraction *ctx, int x, int y) {
    MenuSettingsFieldLayout layouts[MENU_SETTINGS_FIELD_COUNT];
    size_t layout_count = 0;
    size_t i = 0;
    if (!ctx) return false;
    if (ctx->settings_shell.provider == MENU_SETTINGS_PROVIDER_STRUCTURAL) {
        return false;
    }

    layout_count = menu_settings_layout_build_field_layouts(ctx,
                                                            layouts,
                                                            MENU_SETTINGS_FIELD_COUNT);
    for (i = 0; i < layout_count; ++i) {
        const MenuSettingsFieldDef *def = menu_settings_schema_field(layouts[i].field);
        if (!def || def->runtime_display_only) continue;
        if (menu_point_in_rect(x, y, &layouts[i].dec_rect)) {
            if (layouts[i].field == MENU_SETTINGS_FIELD_QUALITY_PRESET) {
                menu_cycle_quality(ctx, -1);
            } else {
                menu_settings_shell_nudge_field(&ctx->settings_shell,
                                                layouts[i].field,
                                                -1);
                sync_quality_index(ctx);
            }
            return true;
        }
        if (menu_point_in_rect(x, y, &layouts[i].inc_rect)) {
            if (layouts[i].field == MENU_SETTINGS_FIELD_QUALITY_PRESET) {
                menu_cycle_quality(ctx, 1);
            } else {
                menu_settings_shell_nudge_field(&ctx->settings_shell,
                                                layouts[i].field,
                                                1);
                sync_quality_index(ctx);
            }
            return true;
        }
    }

    if (menu_point_in_rect(x, y, &ctx->volume_toggle_rect)) {
        menu_settings_shell_toggle_field(&ctx->settings_shell,
                                         MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES);
        sync_quality_index(ctx);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->render_toggle_rect)) {
        menu_settings_shell_toggle_field(&ctx->settings_shell,
                                         MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES);
        sync_quality_index(ctx);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->blur_toggle_rect)) {
        menu_settings_shell_toggle_field(&ctx->settings_shell,
                                         MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR);
        sync_quality_index(ctx);
        return true;
    }

    if (ctx->atmospheric_initial_state_toggle_rect.w > 0 &&
        ctx->atmospheric_initial_state_toggle_rect.h > 0 &&
        menu_point_in_rect(x, y, &ctx->atmospheric_initial_state_toggle_rect)) {
        menu_settings_shell_toggle_field(&ctx->settings_shell,
                                         MENU_SETTINGS_FIELD_ATMOSPHERIC_INITIAL_STATE);
        sync_quality_index(ctx);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->settings_apply_button.rect)) {
        if (menu_settings_has_pending_changes(ctx)) {
            menu_settings_commit(ctx, false);
            menu_set_status(ctx, "Current runtime settings updated.", false);
        } else {
            menu_set_status(ctx, "No pending settings changes.", false);
        }
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->settings_save_button.rect)) {
        menu_settings_save_current(ctx);
        menu_set_status(ctx, "Current settings saved.", false);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->settings_reset_button.rect)) {
        menu_settings_reset_to_runtime(ctx);
        menu_set_status(ctx, "Pending settings reverted.", false);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->settings_restore_saved_button.rect)) {
        menu_settings_restore_saved(ctx);
        menu_set_status(ctx, "Saved settings restored.", false);
        return true;
    }

    if (menu_point_in_rect(x, y, &ctx->settings_defaults_button.rect)) {
        menu_settings_reset_to_defaults(ctx);
        menu_set_status(ctx, "Loaded default settings into draft.", false);
        return true;
    }

    return false;
}
