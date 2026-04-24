#include "app/scene_runtime_launch_projection.h"
#include "app/sim_runtime_3d_space.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nearly_equal(double a, double b, double tolerance) {
    return fabs(a - b) <= tolerance;
}

static bool test_retained_runtime_launch_projection_overwrites_stale_live_preset(void) {
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    SceneRuntimeLaunch runtime_launch = {0};
    char diagnostics[256];

    cfg.space_mode = SPACE_MODE_3D;
    cfg.quality_index = 5;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.80f,
        .position_y = 0.80f,
        .position_z = 3.25f,
        .size_x = 0.20f,
        .size_y = 0.20f,
        .size_z = 0.20f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.82f,
        .position_y = 0.84f,
        .position_z = 3.50f,
        .radius = 0.04f,
        .strength = 1.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    runtime_launch.has_retained_scene = true;
    snprintf(runtime_launch.retained_runtime_scene_path,
             sizeof(runtime_launch.retained_runtime_scene_path),
             "%s",
             "data/runtime/scenes/scene_ps4d_visual_test.json");

    if (!scene_runtime_launch_apply_retained_projection(&runtime_launch,
                                                        &cfg,
                                                        &preset,
                                                        diagnostics,
                                                        sizeof(diagnostics))) {
        fprintf(stderr,
                "scene_runtime_launch_projection_contract_test: projection failed: %s\n",
                diagnostics);
        return false;
    }

    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count < 2 || preset.emitter_count != 1) return false;
    if (preset.emitters[0].attached_object != 1 || preset.emitters[0].attached_import != -1) return false;

    if (!nearly_equal(preset.objects[1].position_x, 0.5, 0.000001)) return false;
    if (!nearly_equal(preset.objects[1].position_y, 0.5, 0.000001)) return false;
    if (!nearly_equal(preset.objects[1].position_z, 0.25, 0.000001)) return false;

    if (!nearly_equal(preset.emitters[0].position_x, 0.5, 0.000001)) return false;
    if (!nearly_equal(preset.emitters[0].position_y, 0.5, 0.000001)) return false;
    if (!nearly_equal(preset.emitters[0].position_z, 0.25, 0.000001)) return false;

    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(preset.emitters[0].position_x, -6.0f, 6.0f),
                      0.0,
                      0.000001)) {
        return false;
    }
    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(preset.emitters[0].position_y, -5.0f, 5.0f),
                      0.0,
                      0.000001)) {
        return false;
    }
    if (!nearly_equal(preset.emitters[0].position_z, 0.25, 0.000001)) return false;

    return true;
}

int main(void) {
    if (!test_retained_runtime_launch_projection_overwrites_stale_live_preset()) {
        return 1;
    }

    fprintf(stdout, "scene_runtime_launch_projection_contract_test: PASS\n");
    return 0;
}
