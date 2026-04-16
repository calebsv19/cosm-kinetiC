#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "app/app_config.h"
#include "app/data_paths.h"
#include "app/scene_controller.h"
#include "app/preset_io.h"
#include "app/scene_menu.h"
#include "app/scene_presets.h"
#include "app/structural/structural_controller.h"
#include "app/quality_profiles.h"
#include "config/config_loader.h"
#include "geo/shape_library.h"
#include "physics_sim/physics_sim_app_main.h"
#include "render/timer_hud_adapter.h"
#include "timer_hud/time_scope.h"
#include "render/vk_shared_device.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static bool physics_sim_dir_exists_local(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool physics_sim_apply_startup_root_fallbacks(AppConfig *cfg,
                                                     char *warning_out,
                                                     size_t warning_out_size) {
    bool changed = false;
    bool warned = false;
    if (!cfg) return false;
    if (warning_out && warning_out_size > 0) {
        warning_out[0] = '\0';
    }

    if (cfg->input_root[0] == '\0') {
        snprintf(cfg->input_root,
                 sizeof(cfg->input_root),
                 "%s",
                 physics_sim_default_input_root());
        changed = true;
    } else if (!physics_sim_dir_exists_local(cfg->input_root)) {
        if (warning_out && warning_out_size > 0 && !warned) {
            snprintf(warning_out,
                     warning_out_size,
                     "Startup fallback: input root '%s' missing; using '%s'.",
                     cfg->input_root,
                     physics_sim_default_input_root());
            warned = true;
        }
        snprintf(cfg->input_root,
                 sizeof(cfg->input_root),
                 "%s",
                 physics_sim_default_input_root());
        changed = true;
    }

    if (cfg->headless_output_dir[0] == '\0') {
        snprintf(cfg->headless_output_dir,
                 sizeof(cfg->headless_output_dir),
                 "%s",
                 physics_sim_default_snapshot_dir());
        changed = true;
    } else if (!physics_sim_dir_exists_local(cfg->headless_output_dir)) {
        if (warning_out && warning_out_size > 0 && !warned) {
            snprintf(warning_out,
                     warning_out_size,
                     "Startup fallback: output root '%s' missing; using '%s'.",
                     cfg->headless_output_dir,
                     physics_sim_default_snapshot_dir());
            warned = true;
        }
        snprintf(cfg->headless_output_dir,
                 sizeof(cfg->headless_output_dir),
                 "%s",
                 physics_sim_default_snapshot_dir());
        changed = true;
    }
    return changed;
}

int physics_sim_app_main_legacy(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char preset_input_path[512];
    char shape_dir_buffer[512];
    const char *preset_load_path = NULL;
    const char *preset_save_path = physics_sim_runtime_preset_path();
    const char *config_load_path = physics_sim_resolve_config_load_path();
    const char *config_save_path = physics_sim_runtime_config_path();

    AppConfig cfg;
    char startup_root_warning[256];
    ConfigLoadOptions opts = {
        .path = config_load_path,
        .allow_missing = true,
    };
    if (!config_loader_load(&cfg, &opts)) {
        fprintf(stderr, "Failed to load config, continuing with defaults.\n");
    }

    if (!physics_sim_ensure_runtime_dirs()) {
        fprintf(stderr, "[runtime] Failed to ensure data/runtime path; save may fail.\n");
    }
    if (physics_sim_apply_startup_root_fallbacks(&cfg,
                                                 startup_root_warning,
                                                 sizeof(startup_root_warning))) {
        if (startup_root_warning[0]) {
            fprintf(stderr, "[startup] %s\n", startup_root_warning);
        }
        if (!config_loader_save(&cfg, config_save_path)) {
            fprintf(stderr, "[startup] Failed to persist fallback root updates.\n");
        }
    }

    timer_hud_register_backend();
    ts_init();

    if (cfg.input_root[0] == '\0') {
        snprintf(cfg.input_root,
                 sizeof(cfg.input_root),
                 "%s",
                 physics_sim_default_input_root());
    }
    preset_load_path = physics_sim_resolve_preset_load_path_for_root(cfg.input_root,
                                                                      preset_input_path,
                                                                      sizeof(preset_input_path));

    const char *shape_dir = getenv("SHAPE_ASSET_DIR");
    if (!shape_dir || shape_dir[0] == '\0') {
        shape_dir = physics_sim_resolve_shape_asset_dir_for_root(cfg.input_root,
                                                                 shape_dir_buffer,
                                                                 sizeof(shape_dir_buffer));
    }

    ShapeAssetLibrary shape_lib;
    bool loaded_shapes = shape_library_load_dir(shape_dir, &shape_lib);
    if (!loaded_shapes) {
        fprintf(stderr, "[shape] No ShapeAssets loaded from %s\n", shape_dir);
        memset(&shape_lib, 0, sizeof(shape_lib));
    }

    CustomPresetLibrary library;
    preset_library_init(&library);
    preset_library_load(preset_load_path, &library);
    if (preset_library_count(&library) == 0) {
        const FluidScenePreset *default_custom = scene_presets_get_default();
        CustomPresetSlot *slot = preset_library_add_slot(&library, "Custom Preset 1", default_custom);
        if (slot && default_custom) {
            slot->preset = *default_custom;
            slot->preset.name = slot->name;
            slot->preset.is_custom = true;
            slot->occupied = true;
        }
    }

    const FluidScenePreset *default_preset = scene_presets_get_default();
    FluidScenePreset preset_state = *default_preset;

    SceneMenuSelection selection = {
        .custom_slot_index = library.active_slot,
        .quality_index = cfg.quality_index,
        .headless_frame_count = cfg.headless_frame_count,
        .sim_mode = cfg.sim_mode,
        .tunnel_inflow_speed = cfg.tunnel_inflow_speed
    };
    for (int i = 0; i < SIMULATION_MODE_COUNT; ++i) {
        selection.last_mode_slot[i] = -1;
    }
    if (cfg.sim_mode >= 0 && cfg.sim_mode < SIMULATION_MODE_COUNT) {
        selection.last_mode_slot[cfg.sim_mode] = selection.custom_slot_index;
    }

    if (cfg.headless_enabled) {
        if (cfg.sim_mode == SIM_MODE_STRUCTURAL) {
            fprintf(stderr, "[main] Structural mode ignores headless config.\n");
            cfg.headless_enabled = false;
        }
    }

    if (cfg.headless_enabled) {
        if (cfg.headless_quality_index >= 0) {
            quality_profile_apply(&cfg, cfg.headless_quality_index);
        }

        int slot_index = cfg.headless_custom_slot;
        if (slot_index < 0 || slot_index >= preset_library_count(&library)) {
            slot_index = 0;
        }
        CustomPresetSlot *slot = preset_library_get_slot(&library, slot_index);
        FluidScenePreset *preset_to_run = slot ? &slot->preset : &preset_state;
        HeadlessOptions headless_opts = {
            .enabled = true,
            .frame_limit = cfg.headless_frame_count,
            .skip_present = cfg.headless_skip_present,
            .ignore_input = false,
            .preserve_sdl_state = false
        };
        SceneRuntimeLaunch runtime_launch = {0};
        const char *output_dir = physics_sim_resolve_snapshot_output_dir(cfg.headless_output_dir);
        if (cfg.space_mode == SPACE_MODE_3D && selection.retained_runtime_scene_path[0]) {
            runtime_launch.has_retained_scene = true;
            snprintf(runtime_launch.retained_runtime_scene_path,
                     sizeof(runtime_launch.retained_runtime_scene_path),
                     "%s",
                     selection.retained_runtime_scene_path);
        }
        scene_controller_run(&cfg,
                             preset_to_run,
                             runtime_launch.has_retained_scene ? &runtime_launch : NULL,
                             &shape_lib,
                             output_dir,
                             &headless_opts);

        preset_library_save(preset_save_path, &library);
        config_loader_save(&cfg, config_save_path);
        shape_library_free(&shape_lib);
        preset_library_shutdown(&library);
        ts_shutdown();
        vk_shared_device_shutdown();
        if (TTF_WasInit()) {
            TTF_Quit();
        }
        if (SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_Quit();
        }
        return 0;
    }

    while (scene_menu_run(&cfg, &preset_state, &selection, &library, &shape_lib)) {
        CustomPresetSlot *slot = preset_library_get_slot(&library, selection.custom_slot_index);
        FluidScenePreset *preset_to_run = slot ? &slot->preset : &preset_state;
        if (selection.sim_mode == SIM_MODE_STRUCTURAL) {
            const char *preset_path = (preset_to_run && preset_to_run->structural_scene_path[0])
                                          ? preset_to_run->structural_scene_path
                                          : NULL;
            structural_controller_run(&cfg, &shape_lib, preset_path);
        } else {
            const char *output_dir = physics_sim_resolve_snapshot_output_dir(cfg.headless_output_dir);
            SceneRuntimeLaunch runtime_launch = {0};
            if (cfg.space_mode == SPACE_MODE_3D && selection.retained_runtime_scene_path[0]) {
                runtime_launch.has_retained_scene = true;
                snprintf(runtime_launch.retained_runtime_scene_path,
                         sizeof(runtime_launch.retained_runtime_scene_path),
                         "%s",
                         selection.retained_runtime_scene_path);
            }
            scene_controller_run(&cfg,
                                 preset_to_run,
                                 runtime_launch.has_retained_scene ? &runtime_launch : NULL,
                                 &shape_lib,
                                 output_dir,
                                 NULL);
        }
        cfg.quality_index = selection.quality_index;
        cfg.headless_frame_count = selection.headless_frame_count;
        cfg.sim_mode = selection.sim_mode;
    }

    preset_library_save(preset_save_path, &library);
    config_loader_save(&cfg, config_save_path);
    shape_library_free(&shape_lib);
    preset_library_shutdown(&library);

    ts_shutdown();
    vk_shared_device_shutdown();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Quit();
    }

    return 0;
}

int main(int argc, char **argv) {
    physics_sim_app_set_legacy_entry(physics_sim_app_main_legacy);
    return physics_sim_app_main(argc, argv);
}
