#include "render/retained_runtime_scene_overlay_space.h"

#include "app/scene_state.h"

#include <stdbool.h>
#include <stdio.h>

static bool nearly_equal(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 0.0001;
}

static bool test_visual_bounds_prefers_authored_scene_domain(void) {
    SceneState scene = {0};
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};

    scene.runtime_visual.scene_domain.enabled = true;
    scene.runtime_visual.scene_domain.min = (CoreObjectVec3){-2.0, -1.0, 3.0};
    scene.runtime_visual.scene_domain.max = (CoreObjectVec3){7.0, 9.0, 11.0};

    if (!retained_runtime_overlay_compute_visual_bounds(&scene, &min, &max)) return false;
    return nearly_equal(min.x, -2.0) && nearly_equal(min.y, -1.0) && nearly_equal(min.z, 3.0) &&
           nearly_equal(max.x, 7.0) && nearly_equal(max.y, 9.0) && nearly_equal(max.z, 11.0);
}

static bool test_visual_bounds_fall_back_to_retained_prism_geometry(void) {
    SceneState scene = {0};
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    CoreSceneObjectContract *object = &scene.runtime_visual.retained_scene.objects[0];

    scene.runtime_visual.retained_scene.retained_object_count = 1;
    object->has_rect_prism_primitive = true;
    object->rect_prism_primitive.frame.origin = (CoreObjectVec3){1.0, 2.0, 3.0};
    object->rect_prism_primitive.frame.axis_u = (CoreObjectVec3){1.0, 0.0, 0.0};
    object->rect_prism_primitive.frame.axis_v = (CoreObjectVec3){0.0, 1.0, 0.0};
    object->rect_prism_primitive.frame.normal = (CoreObjectVec3){0.0, 0.0, 1.0};
    object->rect_prism_primitive.width = 2.0;
    object->rect_prism_primitive.height = 4.0;
    object->rect_prism_primitive.depth = 6.0;

    if (!retained_runtime_overlay_compute_visual_bounds(&scene, &min, &max)) return false;
    return nearly_equal(min.x, 0.0) && nearly_equal(min.y, 0.0) && nearly_equal(min.z, 0.0) &&
           nearly_equal(max.x, 2.0) && nearly_equal(max.y, 4.0) && nearly_equal(max.z, 6.0);
}

static bool test_emitter_actual_and_slice_points_follow_anchor_truth(void) {
    SceneState scene = {0};
    FluidScenePreset preset = {0};
    FluidEmitter *emitter = &preset.emitters[0];
    CoreObjectVec3 actual = {0};
    CoreObjectVec3 slice = {0};

    scene.preset = &preset;
    scene.runtime_visual.scene_domain.enabled = true;
    scene.runtime_visual.scene_domain.min = (CoreObjectVec3){0.0, 10.0, -2.0};
    scene.runtime_visual.scene_domain.max = (CoreObjectVec3){10.0, 30.0, 6.0};

    preset.object_count = 1;
    preset.emitter_count = 1;
    preset.objects[0].position_x = 0.2f;
    preset.objects[0].position_y = 0.25f;
    preset.objects[0].position_z = 0.4f;

    emitter->attached_object = 0;
    emitter->attached_import = -1;
    emitter->position_x = 0.8f;
    emitter->position_y = 0.9f;
    emitter->position_z = 0.95f;

    if (!retained_runtime_overlay_emitter_actual_and_slice_points(
            &scene, emitter, &actual, &slice)) {
        return false;
    }

    return nearly_equal(actual.x, 2.0) && nearly_equal(actual.y, 15.0) &&
           nearly_equal(actual.z, 1.2) && nearly_equal(slice.x, actual.x) &&
           nearly_equal(slice.y, actual.y) && nearly_equal(slice.z, 2.0);
}

int main(void) {
    if (!test_visual_bounds_prefers_authored_scene_domain()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_space_contract_test: scene domain bounds failed\n");
        return 1;
    }
    if (!test_visual_bounds_fall_back_to_retained_prism_geometry()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_space_contract_test: retained prism bounds failed\n");
        return 1;
    }
    if (!test_emitter_actual_and_slice_points_follow_anchor_truth()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_space_contract_test: emitter points failed\n");
        return 1;
    }
    fprintf(stdout, "retained_runtime_scene_overlay_space_contract_test: success\n");
    return 0;
}
