#include "app/scene_objects.h"

#include <string.h>
#include <math.h>

#include "physics/objects/physics_object_builder.h"
#include "app/shape_lookup.h"
#include "app/scene_imports.h"

void scene_objects_init(SceneState *scene) {
    if (!scene) return;
    object_manager_init(&scene->objects, 8);
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
    if (!scene || !scene->config || !scene->preset) return;
    const AppConfig *cfg = scene->config;
    bool emitter_on_obj[MAX_PRESET_OBJECTS] = {0};
    for (size_t ei = 0; ei < scene->preset->emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
        int ao = scene->preset->emitters[ei].attached_object;
        if (ao >= 0 && ao < (int)MAX_PRESET_OBJECTS) emitter_on_obj[ao] = true;
    }

    for (size_t i = 0; i < scene->preset->object_count && i < MAX_PRESET_OBJECTS; ++i) {
        const PresetObject *po = &scene->preset->objects[i];
        Vec2 position = vec2(po->position_x * (float)cfg->window_w,
                             po->position_y * (float)cfg->window_h);
        if (po->type == PRESET_OBJECT_CIRCLE) {
            float radius = po->size_x * (float)cfg->window_w;
            SceneObject *obj = object_manager_add_circle(&scene->objects,
                                                         position,
                                                         radius,
                                                         po->is_static);
            if (obj) {
                obj->body.angle = po->angle;
            }
        } else {
            Vec2 half_extents = vec2(po->size_x * (float)cfg->window_w,
                                     po->size_y * (float)cfg->window_h);
            SceneObject *obj = object_manager_add_box(&scene->objects,
                                                      position,
                                                      half_extents,
                                                      po->is_static);
            if (obj) {
                obj->body.angle = po->angle;
            }
        }
        // Static mask application is handled by scene_masks module.
        (void)emitter_on_obj;
    }
}

void scene_objects_rebuild_import_bodies(SceneState *scene) {
    if (!scene || !scene->config) return;
    // Clear existing import bodies
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        if (scene->import_body_map[i] >= 0) {
            object_manager_remove_by_source_import(&scene->objects, (int)i);
            scene->import_body_map[i] = -1;
        }
    }
    // Add new bodies for gravity-enabled imports
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled || !imp->gravity_enabled || imp->is_static) continue;
        int body_index = -1;
        float pos_px = imp->position_x * (float)scene->config->window_w;
        float pos_py = imp->position_y * (float)scene->config->window_h;
        float sx = (float)scene->config->window_w / (float)(scene->config->grid_w > 1 ? (scene->config->grid_w - 1) : 1);
        float sy = (float)scene->config->window_h / (float)(scene->config->grid_h > 1 ? (scene->config->grid_h - 1) : 1);
        for (int pi = 0; pi < imp->collider_part_count; ++pi) {
            int count = imp->collider_part_counts[pi];
            int offset = imp->collider_part_offsets[pi];
            if (count < 3) continue;
            if (offset < 0 || offset + count > 64) continue;
            Vec2 verts[32];
            if (count > 32) count = 32;
            for (int vi = 0; vi < count; ++vi) {
                Vec2 g = imp->collider_parts_verts[offset + vi];
                float wx = g.x * sx;
                float wy = g.y * sy;
                verts[vi].x = wx - pos_px;
                verts[vi].y = wy - pos_py;
            }
            SceneObject *obj = object_manager_add_poly(&scene->objects,
                                                       vec2(pos_px, pos_py),
                                                       verts,
                                                       count,
                                                       false);
            if (obj) {
                obj->body.gravity_enabled = 1;
                obj->body.locked = 0;
                obj->body.restitution = scene->objects_elastic ? 1.0f : 0.4f;
                obj->source_import = (int)i;
                if (body_index < 0) {
                    body_index = scene->objects.count - 1;
                }
            }
        }
        scene->import_body_map[i] = body_index;
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
    scene->obstacle_mask_dirty = true;
}
