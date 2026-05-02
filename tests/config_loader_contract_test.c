#include "app/app_config.h"
#include "config/config_loader.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool test_grid_depth_roundtrip_and_fallback(void) {
    const char *path = "/private/tmp/physics_sim_config_loader_contract_test.json";
    AppConfig saved = app_config_default();
    AppConfig loaded = {0};
    ConfigLoadOptions opts = {
        .path = path,
        .allow_missing = false
    };

    saved.grid_w = 192;
    saved.grid_h = 224;
    saved.grid_d = 48;
    saved.space_mode = SPACE_MODE_3D;
    saved.physics_substeps = 5;
    saved.fluid_solver_iterations = 27;
    if (!config_loader_save(&saved, path)) return false;
    if (!config_loader_load(&loaded, &opts)) return false;
    if (loaded.grid_w != 192) return false;
    if (loaded.grid_h != 224) return false;
    if (loaded.grid_d != 48) return false;
    if (loaded.space_mode != SPACE_MODE_3D) return false;
    if (loaded.physics_substeps != 5) return false;
    if (loaded.fluid_solver_iterations != 27) return false;

    saved.grid_d = 0;
    saved.space_mode = SPACE_MODE_2D;
    if (!config_loader_save(&saved, path)) return false;
    memset(&loaded, 0, sizeof(loaded));
    if (!config_loader_load(&loaded, &opts)) return false;
    if (loaded.grid_d != 0) return false;
    if (loaded.space_mode != SPACE_MODE_2D) return false;

    remove(path);
    return true;
}

int main(void) {
    if (!test_grid_depth_roundtrip_and_fallback()) {
        fprintf(stderr, "config_loader_contract_test: grid depth roundtrip failed\n");
        return 1;
    }
    fprintf(stdout, "config_loader_contract_test: success\n");
    return 0;
}
