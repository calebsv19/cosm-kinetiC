#include "app/scene_objects.h"

#include "physics/objects/object_manager.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

void scene_objects_init(SceneState *scene) {
    if (!scene) return;
    object_manager_init(&scene->objects, 256);
    scene->objects_gravity_enabled = true;
    scene->objects_elastic = false;
    for (size_t i = 0; i < MAX_IMPORTED_SHAPES; ++i) {
        scene->import_body_map[i] = -1;
    }
}

void scene_objects_shutdown(SceneState *scene) {
    if (!scene) return;
    object_manager_shutdown(&scene->objects);
}

void scene_objects_add_presets(SceneState *scene) {
    // Preset objects (circles/boxes) are already baked into the static mask;
    // runtime bodies for them were not used in the current pipeline.
    (void)scene;
}

void scene_objects_set_gravity(SceneState *scene, bool enabled) {
    if (!scene) return;
    scene->objects_gravity_enabled = enabled;
    object_manager_step(&scene->objects, 0.0, scene->config, enabled);
}

void scene_objects_set_elastic(SceneState *scene, bool elastic) {
    if (!scene) return;
    scene->objects_elastic = elastic;
    for (int i = 0; i < scene->objects.count; ++i) {
        scene->objects.objects[i].body.restitution = elastic ? 1.0f : 0.4f;
    }
}

void scene_objects_reset_gravity(SceneState *scene) {
    if (!scene || !scene->config) return;
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        if (!scene->import_shapes[i].gravity_enabled) continue;
        int body_idx = scene->import_body_map[i];
        float px = scene->import_start_pos_x[i] * (float)scene->config->window_w;
        float py = scene->import_start_pos_y[i] * (float)scene->config->window_h;
        float rot = scene->import_start_rot_deg[i] * (float)M_PI / 180.0f;
        scene->import_shapes[i].position_x = scene->import_start_pos_x[i];
        scene->import_shapes[i].position_y = scene->import_start_pos_y[i];
        scene->import_shapes[i].rotation_deg = scene->import_start_rot_deg[i];
        if (body_idx >= 0 && body_idx < scene->objects.count) {
            RigidBody2D *b = &scene->objects.objects[body_idx].body;
            b->position = vec2(px, py);
            b->velocity = vec2(0.0f, 0.0f);
            b->angular_velocity = 0.0f;
            b->angle = rot;
        }
    }
    scene->obstacle_mask_dirty = true;
}
