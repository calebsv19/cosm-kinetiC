#include "app/sim_runtime_3d_domain.h"
#include "import/runtime_scene_bridge.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nearly_equal(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0001f;
}

static bool test_quality_mapping(void) {
    AppConfig cfg = {0};
    cfg.quality_index = 0;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 40) return false;
    cfg.quality_index = 1;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 56) return false;
    cfg.quality_index = 2;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 72) return false;
    cfg.quality_index = 3;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 96) return false;
    cfg.quality_index = 4;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 72) return false;
    cfg.quality_index = 5;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 16) return false;
    cfg.quality_index = -1;
    cfg.grid_w = 220;
    cfg.grid_h = 48;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 96) return false;
    cfg.grid_w = 96;
    cfg.grid_h = 96;
    if (sim_runtime_3d_major_axis_cells_for_config(&cfg) != 48) return false;
    return true;
}

static bool test_legacy_domain_derivation(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimRuntime3DDomainDesc desc = {0};

    cfg.quality_index = 2;
    cfg.grid_w = 256;
    cfg.grid_h = 256;
    preset.domain_width = 4.0f;
    preset.domain_height = 2.0f;

    if (!sim_runtime_3d_domain_desc_from_legacy(&cfg, &preset, &desc)) return false;
    if (desc.grid_w != 72) return false;
    if (desc.grid_h != 36) return false;
    if (desc.grid_d != 36) return false;
    if (desc.slice_cell_count != (size_t)desc.grid_w * (size_t)desc.grid_h) return false;
    if (desc.cell_count != desc.slice_cell_count * (size_t)desc.grid_d) return false;
    if (!nearly_equal(desc.world_max_x, 4.0f)) return false;
    if (!nearly_equal(desc.world_max_y, 2.0f)) return false;
    if (!nearly_equal(desc.world_max_z, 2.0f)) return false;
    if (!nearly_equal(desc.voxel_size, 4.0f / 72.0f)) return false;
    return true;
}

static bool test_tiny3d_debug_profile_derivation(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimRuntime3DDomainDesc desc = {0};

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    preset.domain_width = 4.0f;
    preset.domain_height = 2.0f;

    if (!sim_runtime_3d_domain_desc_from_legacy(&cfg, &preset, &desc)) return false;
    if (desc.grid_w != 16) return false;
    if (desc.grid_h != 8) return false;
    if (desc.grid_d != 8) return false;
    if (!nearly_equal(desc.voxel_size, 4.0f / 16.0f)) return false;
    if (desc.cell_count != (size_t)16 * (size_t)8 * (size_t)8) return false;
    return true;
}

static bool test_runtime_visual_precedence(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntime3DDomainDesc desc = {0};

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    preset.domain_width = 10.0f;
    preset.domain_height = 10.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -2.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -3.0;
    visual.scene_domain.max.x = 4.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 1.0;

    if (!sim_runtime_3d_domain_desc_resolve(&cfg, &preset, &visual, &desc)) return false;
    if (!nearly_equal(desc.world_min_x, -2.0f)) return false;
    if (!nearly_equal(desc.world_min_y, -1.0f)) return false;
    if (!nearly_equal(desc.world_min_z, -3.0f)) return false;
    if (!nearly_equal(desc.world_max_x, 4.0f)) return false;
    if (!nearly_equal(desc.world_max_y, 2.0f)) return false;
    if (!nearly_equal(desc.world_max_z, 1.0f)) return false;
    if (desc.grid_w != 56) return false;
    if (desc.grid_h != 28) return false;
    if (desc.grid_d != 38) return false;

    memset(&visual, 0, sizeof(visual));
    visual.retained_scene.has_line_drawing_scene3d = true;
    visual.retained_scene.bounds.enabled = true;
    visual.retained_scene.bounds.min.x = -1.0;
    visual.retained_scene.bounds.min.y = -2.0;
    visual.retained_scene.bounds.min.z = -0.5;
    visual.retained_scene.bounds.max.x = 2.0;
    visual.retained_scene.bounds.max.y = 2.0;
    visual.retained_scene.bounds.max.z = 0.5;
    if (!sim_runtime_3d_domain_desc_resolve(&cfg, &preset, &visual, &desc)) return false;
    if (!nearly_equal(desc.world_min_x, -1.0f)) return false;
    if (!nearly_equal(desc.world_max_y, 2.0f)) return false;
    if (!nearly_equal(desc.world_max_z, 0.5f)) return false;
    if (desc.grid_h != 56) return false;
    if (desc.grid_w != 42) return false;
    if (desc.grid_d != 14) return false;

    return true;
}

static bool test_volume_init_and_clear(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimRuntime3DDomainDesc desc = {0};
    SimRuntime3DVolume volume = {0};
    size_t idx = 0;

    cfg.quality_index = 0;
    cfg.grid_w = 96;
    cfg.grid_h = 96;
    preset.domain_width = 1.0f;
    preset.domain_height = 0.5f;

    if (!sim_runtime_3d_domain_desc_from_legacy(&cfg, &preset, &desc)) return false;
    if (!sim_runtime_3d_volume_init(&volume, &desc)) return false;

    idx = sim_runtime_3d_volume_index(&volume.desc, 1, 2, 3);
    volume.density[idx] = 9.0f;
    volume.velocity_x[idx] = 3.0f;
    volume.velocity_y[idx] = -2.0f;
    volume.velocity_z[idx] = 7.0f;
    volume.pressure[idx] = 5.0f;
    sim_runtime_3d_volume_clear(&volume);
    if (!nearly_equal(volume.density[idx], 0.0f)) return false;
    if (!nearly_equal(volume.velocity_x[idx], 0.0f)) return false;
    if (!nearly_equal(volume.velocity_y[idx], 0.0f)) return false;
    if (!nearly_equal(volume.velocity_z[idx], 0.0f)) return false;
    if (!nearly_equal(volume.pressure[idx], 0.0f)) return false;

    sim_runtime_3d_volume_destroy(&volume);
    return true;
}

int main(void) {
    if (!test_quality_mapping()) {
        fprintf(stderr, "sim_runtime_3d_domain_contract_test: quality mapping failed\n");
        return 1;
    }
    if (!test_legacy_domain_derivation()) {
        fprintf(stderr, "sim_runtime_3d_domain_contract_test: legacy derivation failed\n");
        return 1;
    }
    if (!test_tiny3d_debug_profile_derivation()) {
        fprintf(stderr, "sim_runtime_3d_domain_contract_test: tiny3d derivation failed\n");
        return 1;
    }
    if (!test_runtime_visual_precedence()) {
        fprintf(stderr, "sim_runtime_3d_domain_contract_test: runtime visual precedence failed\n");
        return 1;
    }
    if (!test_volume_init_and_clear()) {
        fprintf(stderr, "sim_runtime_3d_domain_contract_test: volume init/clear failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_3d_domain_contract_test: success\n");
    return 0;
}
