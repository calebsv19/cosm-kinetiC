#include "app/scene_imports.h"

#include <string.h>
#include <stdio.h>
#include "app/shape_lookup.h"
#include "physics/rigid/collider_builder.h"
#include "app/scene_masks.h"

void scene_import_reset_collider(ImportedShape *imp) {
    if (!imp) return;
    imp->collider_vert_count = 0;
    imp->collider_part_count = 0;
    memset(imp->collider_part_offsets, 0, sizeof(imp->collider_part_offsets));
    memset(imp->collider_part_counts, 0, sizeof(imp->collider_part_counts));
}

void scene_imports_resolve(SceneState *scene) {
    if (!scene || !scene->shape_library) return;
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled || imp->path[0] == '\0') continue;
        const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
        if (!asset) {
            imp->shape_id = -1;
            continue;
        }
        for (size_t si = 0; si < scene->shape_library->count; ++si) {
            if (&scene->shape_library->assets[si] == asset) {
                imp->shape_id = (int)si;
                break;
            }
        }
    }
}

static void add_import_body(SceneState *scene, size_t imp_index) {
    if (!scene || imp_index >= scene->import_shape_count) return;
    ImportedShape *imp = &scene->import_shapes[imp_index];
    if (!imp->enabled || !imp->gravity_enabled) return;
    // Treat gravity-enabled imports as dynamic regardless of the serialized static flag.
    imp->is_static = 0;
    if (scene->import_body_map[imp_index] >= 0) return; // already has a body

    // Build poly bodies from collider parts
    int total_parts = imp->collider_part_count;
    int vert_cursor = 0;
    for (int pi = 0; pi < total_parts; ++pi) {
        int count = imp->collider_part_counts[pi];
        int offset = imp->collider_part_offsets[pi];
        if (count < 3) continue;
        if (offset < 0 || offset + count > 64) continue;
        Vec2 verts[32];
        if (count > 32) count = 32;
        memcpy(verts, &imp->collider_parts_verts[offset], (size_t)count * sizeof(Vec2));
        SceneObject *obj = object_manager_add_poly(&scene->objects,
                                                   vec2(imp->position_x * (float)scene->config->window_w,
                                                        imp->position_y * (float)scene->config->window_h),
                                                   verts,
                                                   count,
                                                   false);
        if (obj) {
            obj->body.gravity_enabled = 1;
            obj->body.locked = 0;
            obj->body.restitution = scene->objects_elastic ? 1.0f : 0.4f;
            obj->source_import = (int)imp_index;
            if (scene->import_body_map[imp_index] < 0) {
                scene->import_body_map[imp_index] = scene->objects.count - 1;
            }
        }
    }
    (void)vert_cursor;
}

void scene_imports_rebuild_bodies(SceneState *scene) {
    if (!scene || !scene->config || !scene->shape_library) return;
    if (scene->config->collider_debug_logs) {
        fprintf(stderr, "[collider] rebuild bodies (imports=%zu)\n", scene->import_shape_count);
    }
    // Clear existing bodies for imports
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        if (scene->import_body_map[i] >= 0) {
            object_manager_remove_by_source_import(&scene->objects, (int)i);
            scene->import_body_map[i] = -1;
        }
    }
    // Rebuild colliders and add bodies for gravity-enabled imports
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled) continue;
        if (imp->gravity_enabled) imp->is_static = 0; // dynamic at runtime
        scene_import_reset_collider(imp);
        collider_build_import(scene->config, scene->shape_library, imp);
        if (imp->collider_part_count <= 0) {
            fprintf(stderr, "[collider] import %zu parts=0 (path=%s)\n", i, imp->path);
        }
        add_import_body(scene, i);
    }
    scene_masks_mark_emitters_dirty(scene);
}
