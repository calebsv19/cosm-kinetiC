#include "app/sim_mode.h"

#include <math.h>
#include <stddef.h>

#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/water_object_coupling.h"
#include "app/water_mode.h"

static void water_apply_config_tweaks(AppConfig *cfg) {
    if (!cfg) return;
    cfg->space_mode = SPACE_MODE_3D;
    cfg->water_level = water_mode_level_clamp(cfg->water_level);
    cfg->density_decay = 0.0f;
    cfg->fluid_buoyancy_force = 0.0f;
    if (cfg->density_diffusion < 0.0f) {
        cfg->density_diffusion = 0.0f;
    }
    if (cfg->fluid_solver_iterations < 30) {
        cfg->fluid_solver_iterations = 30;
    }
}

static void water_configure(AppConfig *cfg, FluidScenePreset *preset) {
    WaterModeConfig water = water_mode_config_default(cfg);
    if (!cfg || !preset) return;
    water_apply_config_tweaks(cfg);
    water = water_mode_config_default(cfg);
    water_mode_apply_preset(&water, preset);
}

static int water_fill_height_cells(int available_cells, float water_level) {
    float clamped = water_mode_level_clamp(water_level);
    int cells = 0;
    if (available_cells <= 0) return 0;
    cells = (int)floorf(clamped * (float)available_cells);
    if (clamped > 0.0f && cells < 1) cells = 1;
    if (cells > available_cells) cells = available_cells;
    return cells;
}

static size_t water_seed_initial_layer(SceneState *scene) {
    SimRuntime3DDomainDesc desc = {0};
    SceneFluidVolumeExportView3D existing = {0};
    int fill_h = 0;
    int water_min_y = 0;
    int water_max_y_exclusive = 0;
    int water_available_h = 0;
    size_t seeded = 0u;
    const uint8_t *solid_mask = NULL;

    if (!scene || !scene->backend || !scene->config) return 0u;
    if (!sim_runtime_backend_get_domain_desc_3d(scene->backend, &desc)) return 0u;
    water_min_y = desc.grid_h > 2 ? 1 : 0;
    water_max_y_exclusive = desc.grid_h > 2 ? desc.grid_h - 1 : desc.grid_h;
    water_available_h = water_max_y_exclusive - water_min_y;
    fill_h = water_fill_height_cells(water_available_h, scene->config->water_level);
    if (fill_h <= 0) return 0u;
    water_max_y_exclusive = water_min_y + fill_h;

    if (sim_runtime_backend_get_volume_export_view_3d(scene->backend, &existing)) {
        solid_mask = existing.solid_mask;
    }

    for (int z = 0; z < desc.grid_d; ++z) {
        for (int y = water_min_y; y < water_max_y_exclusive; ++y) {
            for (int x = 0; x < desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&desc, x, y, z);
                if (solid_mask && solid_mask[idx] != 0u) continue;
                if (sim_runtime_backend_debug_write_volume_cell_3d(scene->backend,
                                                                   x,
                                                                   y,
                                                                   z,
                                                                   1.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0u)) {
                    seeded++;
                }
            }
        }
    }
    return seeded;
}

static void water_prepare(SceneState *scene) {
    if (!scene || !scene->config || !scene->preset) return;
    scene_set_emitters_enabled(scene, false);
    sim_runtime_backend_reset_transient_state(scene->backend);
    (void)water_object_coupling_apply_fixture(scene);
    (void)water_seed_initial_layer(scene);
}

static void water_pre_substep(SceneState *scene,
                              double dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    (void)dt;
    if (!scene) return;
    scene_set_emitters_enabled(scene, false);
}

static void water_post_substep(SceneState *scene,
                               double dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    (void)dt;
    (void)water_object_coupling_apply_fixture(scene);
}

const SimModeHooks g_sim_mode_water = {
    .configure_app = water_configure,
    .prepare_scene = water_prepare,
    .pre_substep = water_pre_substep,
    .post_substep = water_post_substep,
};
