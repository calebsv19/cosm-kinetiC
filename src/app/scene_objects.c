#include "app/scene_objects.h"

#include "physics/objects/object_manager.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static Vec2 preset_object_position_px(const SceneState *scene, const PresetObject *obj) {
    return vec2(obj->position_x * (float)scene->config->window_w,
                obj->position_y * (float)scene->config->window_h);
}

static Vec2 preset_object_initial_velocity_px(const SceneState *scene, const PresetObject *obj) {
    return vec2(obj->initial_velocity_x * (float)scene->config->window_w,
                obj->initial_velocity_y * (float)scene->config->window_h);
}

void scene_objects_init(SceneState *scene) {
    if (!scene) return;
    object_manager_init(&scene->objects, 256);
    scene->objects_gravity_enabled = true;
    scene->objects_elastic = true; // default: elastic collisions
    for (size_t i = 0; i < MAX_IMPORTED_SHAPES; ++i) {
        scene->import_body_map[i] = -1;
    }
}

void scene_objects_shutdown(SceneState *scene) {
    if (!scene) return;
    object_manager_shutdown(&scene->objects);
}

void scene_objects_add_presets(SceneState *scene) {
    size_t i = 0;
    if (!scene || !scene->config || !scene->preset) return;

    for (i = 0; i < scene->preset->object_count; ++i) {
        const PresetObject *obj = &scene->preset->objects[i];
        SceneObject *runtime_obj = NULL;
        Vec2 position_px;
        Vec2 initial_velocity_px;
        if (obj->is_static) continue;

        position_px = preset_object_position_px(scene, obj);
        if (obj->type == PRESET_OBJECT_CIRCLE) {
            float radius_px = obj->size_x * (float)scene->config->window_w;
            if (radius_px <= 0.0f) continue;
            runtime_obj = object_manager_add_circle(&scene->objects, position_px, radius_px, false);
        } else {
            Vec2 half_extents_px = vec2(obj->size_x * (float)scene->config->window_w,
                                        obj->size_y * (float)scene->config->window_h);
            if (half_extents_px.x <= 0.0f || half_extents_px.y <= 0.0f) continue;
            runtime_obj = object_manager_add_box(&scene->objects, position_px, half_extents_px, false);
        }
        if (!runtime_obj) continue;

        runtime_obj->body.angle = obj->angle;
        runtime_obj->body.gravity_enabled = obj->gravity_enabled ? 1 : 0;
        initial_velocity_px = preset_object_initial_velocity_px(scene, obj);
        runtime_obj->body.velocity = initial_velocity_px;
    }
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
    scene_backend_mark_obstacles_dirty(scene);
}
