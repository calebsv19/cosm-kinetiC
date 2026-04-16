#include "app/scene_objects.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool test_scene_objects_add_presets_seeds_dynamic_body_velocity(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};

    cfg.window_w = 800;
    cfg.window_h = 600;

    preset.object_count = 1;
    preset.objects[0].type = PRESET_OBJECT_BOX;
    preset.objects[0].position_x = 0.5f;
    preset.objects[0].position_y = 0.25f;
    preset.objects[0].size_x = 0.1f;
    preset.objects[0].size_y = 0.2f;
    preset.objects[0].angle = 0.3f;
    preset.objects[0].is_static = false;
    preset.objects[0].gravity_enabled = true;
    preset.objects[0].initial_velocity_x = 0.25f;
    preset.objects[0].initial_velocity_y = -0.50f;

    scene.config = &cfg;
    scene.preset = &preset;
    scene_objects_init(&scene);
    scene_objects_add_presets(&scene);

    if (scene.objects.count != 1) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.position.x - 400.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.position.y - 150.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.velocity.x - 200.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.velocity.y - (-300.0f)) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.angle - 0.3f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (!scene.objects.objects[0].body.gravity_enabled) {
        scene_objects_shutdown(&scene);
        return false;
    }

    scene_objects_shutdown(&scene);
    return true;
}

static bool test_scene_objects_add_presets_skips_static_objects(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};

    cfg.window_w = 640;
    cfg.window_h = 480;

    preset.object_count = 1;
    preset.objects[0].type = PRESET_OBJECT_CIRCLE;
    preset.objects[0].position_x = 0.5f;
    preset.objects[0].position_y = 0.5f;
    preset.objects[0].size_x = 0.05f;
    preset.objects[0].is_static = true;
    preset.objects[0].gravity_enabled = false;
    preset.objects[0].initial_velocity_x = 1.0f;
    preset.objects[0].initial_velocity_y = 1.0f;

    scene.config = &cfg;
    scene.preset = &preset;
    scene_objects_init(&scene);
    scene_objects_add_presets(&scene);

    if (scene.objects.count != 0) {
        scene_objects_shutdown(&scene);
        return false;
    }

    scene_objects_shutdown(&scene);
    return true;
}

int main(void) {
    if (!test_scene_objects_add_presets_seeds_dynamic_body_velocity()) {
        fprintf(stderr, "scene_objects_runtime_contract_test: dynamic body velocity seed failed\n");
        return 1;
    }
    if (!test_scene_objects_add_presets_skips_static_objects()) {
        fprintf(stderr, "scene_objects_runtime_contract_test: static object skip failed\n");
        return 1;
    }
    fprintf(stdout, "scene_objects_runtime_contract_test: success\n");
    return 0;
}
