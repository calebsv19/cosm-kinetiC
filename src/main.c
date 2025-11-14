#include <stdbool.h>
#include <stdio.h>

#include "app/app_config.h"
#include "app/scene_controller.h"
#include "app/scene_menu.h"
#include "app/scene_presets.h"
#include "config/config_loader.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    AppConfig cfg;
    ConfigLoadOptions opts = {
        .path = "config/app.json",
        .allow_missing = true,
    };
    if (!config_loader_load(&cfg, &opts)) {
        fprintf(stderr, "Failed to load config, continuing with defaults.\n");
    }

    const FluidScenePreset *default_preset = scene_presets_get_default();
    FluidScenePreset preset_state = *default_preset;
    int preset_index = 0;

    while (scene_menu_run(&cfg, &preset_state, &preset_index)) {
        scene_controller_run(&cfg, &preset_state, "data/snapshots");
    }

    return 0;
}
