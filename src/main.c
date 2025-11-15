#include <stdbool.h>
#include <stdio.h>

#include "app/app_config.h"
#include "app/scene_controller.h"
#include "app/preset_io.h"
#include "app/scene_menu.h"
#include "app/scene_presets.h"
#include "config/config_loader.h"

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
        .custom_slot_index = library.active_slot
    };

    while (scene_menu_run(&cfg, &preset_state, &selection, &library)) {
        CustomPresetSlot *slot = preset_library_get_slot(&library, selection.custom_slot_index);
        FluidScenePreset *preset_to_run = slot ? &slot->preset : &preset_state;
        scene_controller_run(&cfg, preset_to_run, "data/snapshots");
    }

    preset_library_save(preset_path, &library);
    preset_library_shutdown(&library);

    return 0;
}
