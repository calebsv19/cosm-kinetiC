#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app/app_config.h"
#include "app/scene_controller.h"
#include "app/preset_io.h"
#include "app/scene_menu.h"
#include "app/scene_presets.h"
#include "app/structural/structural_controller.h"
#include "app/quality_profiles.h"
#include "config/config_loader.h"
#include "geo/shape_library.h"
#include "render/timer_hud_adapter.h"
#include "timer_hud/time_scope.h"
#include "render/vk_shared_device.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *preset_path = "config/custom_preset.txt";

    AppConfig cfg;
    ConfigLoadOptions opts = {
        .path = "config/app.json",
        .allow_missing = true,
    };
    if (!config_loader_load(&cfg, &opts)) {
        fprintf(stderr, "Failed to load config, continuing with defaults.\n");
    }
    const char *config_path = opts.path ? opts.path : "config/app.json";

    timer_hud_register_backend();
    ts_init();

    const char *shape_dir = getenv("SHAPE_ASSET_DIR");
    if (!shape_dir || shape_dir[0] == '\0') {
        shape_dir = "config/objects";
    }

    ShapeAssetLibrary shape_lib;
    bool loaded_shapes = shape_library_load_dir(shape_dir, &shape_lib);
    if (!loaded_shapes) {
        fprintf(stderr, "[shape] No ShapeAssets loaded from config/objects\n");
        memset(&shape_lib, 0, sizeof(shape_lib));
    }

    CustomPresetLibrary library;
    preset_library_init(&library);
    preset_library_load(preset_path, &library);
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
        const char *output_dir = cfg.headless_output_dir[0]
                                     ? cfg.headless_output_dir
                                     : "data/snapshots";
        scene_controller_run(&cfg, preset_to_run, &shape_lib, output_dir, &headless_opts);

        preset_library_save(preset_path, &library);
        config_loader_save(&cfg, config_path);
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
            scene_controller_run(&cfg, preset_to_run, &shape_lib, "data/snapshots", NULL);
        }
        cfg.quality_index = selection.quality_index;
        cfg.headless_frame_count = selection.headless_frame_count;
        cfg.sim_mode = selection.sim_mode;
    }

    preset_library_save(preset_path, &library);
    config_loader_save(&cfg, config_path);
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
