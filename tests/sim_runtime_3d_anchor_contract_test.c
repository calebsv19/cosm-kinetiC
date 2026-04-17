#include "app/scene_state.h"
#include "app/sim_runtime_3d_anchor.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>

static bool nearly_equal(double a, double b) {
    return fabs(a - b) <= 0.000001;
}

static bool test_retained_object_origin_prefers_embedded_primitive_origin(void) {
    CoreSceneObjectContract object = {0};
    object.object.transform.position = (CoreObjectVec3){1.0, 2.0, 3.0};
    object.has_plane_primitive = true;
    object.plane_primitive.frame.origin = (CoreObjectVec3){4.0, 5.0, 6.0};
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).x, 4.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).y, 5.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).z, 6.0)) return false;

    object.has_plane_primitive = false;
    object.has_rect_prism_primitive = true;
    object.rect_prism_primitive.frame.origin = (CoreObjectVec3){7.0, 8.0, 9.0};
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).x, 7.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).y, 8.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).z, 9.0)) return false;

    object.has_rect_prism_primitive = false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).x, 1.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).y, 2.0)) return false;
    if (!nearly_equal(sim_runtime_3d_anchor_retained_object_origin(&object).z, 3.0)) return false;
    return true;
}

static bool test_retained_object_anchor_beats_preset_fallback(void) {
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    CoreObjectVec3 world = {0};
    CoreObjectVec3 world_min = {0.0, 0.0, 0.0};
    CoreObjectVec3 world_max = {10.0, 10.0, 10.0};

    preset.object_count = 1;
    preset.objects[0].position_x = 0.1f;
    preset.objects[0].position_y = 0.2f;
    preset.objects[0].position_z = 0.3f;
    scene.preset = &preset;
    scene.runtime_visual.retained_scene.retained_object_count = 1;
    scene.runtime_visual.retained_scene.objects[0].has_rect_prism_primitive = true;
    scene.runtime_visual.retained_scene.objects[0].rect_prism_primitive.frame.origin =
        (CoreObjectVec3){8.0, 7.0, 6.0};

    if (!sim_runtime_3d_anchor_resolve_emitter_world_anchor(&scene,
                                                            0,
                                                            -1,
                                                            0.5,
                                                            0.5,
                                                            0.5,
                                                            &world_min,
                                                            &world_max,
                                                            &world)) {
        return false;
    }
    return nearly_equal(world.x, 8.0) &&
           nearly_equal(world.y, 7.0) &&
           nearly_equal(world.z, 6.0);
}

static bool test_import_runtime_then_preset_then_free_fallback_order(void) {
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    CoreObjectVec3 world = {0};
    CoreObjectVec3 world_min = {0.0, 0.0, 0.0};
    CoreObjectVec3 world_max = {1.0, 1.0, 1.0};

    preset.import_shape_count = 1;
    preset.import_shapes[0].position_x = 0.4f;
    preset.import_shapes[0].position_y = 0.5f;
    preset.import_shapes[0].position_z = 0.6f;
    scene.preset = &preset;

    scene.import_shape_count = 1;
    scene.import_shapes[0].position_x = 0.7f;
    scene.import_shapes[0].position_y = 0.8f;
    scene.import_shapes[0].position_z = 0.9f;
    if (!sim_runtime_3d_anchor_resolve_emitter_world_anchor(&scene,
                                                            -1,
                                                            0,
                                                            0.1,
                                                            0.2,
                                                            0.3,
                                                            &world_min,
                                                            &world_max,
                                                            &world)) {
        return false;
    }
    if (!nearly_equal(world.x, 0.7) || !nearly_equal(world.y, 0.8) || !nearly_equal(world.z, 0.9)) {
        return false;
    }

    scene.import_shape_count = 0;
    if (!sim_runtime_3d_anchor_resolve_emitter_world_anchor(&scene,
                                                            -1,
                                                            0,
                                                            0.1,
                                                            0.2,
                                                            0.3,
                                                            &world_min,
                                                            &world_max,
                                                            &world)) {
        return false;
    }
    if (!nearly_equal(world.x, 0.4) || !nearly_equal(world.y, 0.5) || !nearly_equal(world.z, 0.6)) {
        return false;
    }

    preset.import_shape_count = 0;
    if (!sim_runtime_3d_anchor_resolve_emitter_world_anchor(&scene,
                                                            -1,
                                                            -1,
                                                            0.25,
                                                            0.35,
                                                            0.45,
                                                            &world_min,
                                                            &world_max,
                                                            &world)) {
        return false;
    }
    return nearly_equal(world.x, 0.25) &&
           nearly_equal(world.y, 0.35) &&
           nearly_equal(world.z, 0.45);
}

int main(void) {
    if (!test_retained_object_origin_prefers_embedded_primitive_origin()) {
        fprintf(stderr, "sim_runtime_3d_anchor_contract_test: retained origin contract failed\n");
        return 1;
    }
    if (!test_retained_object_anchor_beats_preset_fallback()) {
        fprintf(stderr, "sim_runtime_3d_anchor_contract_test: retained object anchor precedence failed\n");
        return 1;
    }
    if (!test_import_runtime_then_preset_then_free_fallback_order()) {
        fprintf(stderr, "sim_runtime_3d_anchor_contract_test: import/free fallback order failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_3d_anchor_contract_test: success\n");
    return 0;
}
