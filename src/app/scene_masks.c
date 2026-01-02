#include "app/scene_masks.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "app/shape_lookup.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"
#include "render/import_project.h"
#include "physics/objects/physics_object_builder.h"

static void apply_mask_or(uint8_t *dst, const uint8_t *src, size_t count) {
    if (!dst || !src) return;
    for (size_t i = 0; i < count; ++i) {
        if (src[i]) dst[i] = 1;
    }
}

static inline float import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static float shape_bounds_max_dim(const ShapeAssetBounds *b) {
    if (!b || !b->valid) return 0.0f;
    float sx = b->max_x - b->min_x;
    float sy = b->max_y - b->min_y;
    return (sx > sy) ? sx : sy;
}

static void static_mask_mark_cell(SceneState *scene, int gx, int gy) {
    if (!scene || !scene->static_mask || !scene->config) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (gx < 1 || gx >= w - 1 || gy < 1 || gy >= h - 1) return;
    scene->static_mask[(size_t)gy * (size_t)w + (size_t)gx] = 1;
}

static void static_mask_apply_circle(SceneState *scene,
                                     const PresetObject *po) {
    if (!scene || !scene->static_mask || !scene->config || !po) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    int cx = (int)lroundf(po->position_x * (float)(w - 1));
    int cy = (int)lroundf(po->position_y * (float)(h - 1));
    int radius = (int)ceilf(po->size_x * (float)w);
    if (radius < 1) radius = 1;

    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            if (dx * dx + dy * dy <= (float)(radius * radius)) {
                static_mask_mark_cell(scene, x, y);
            }
        }
    }
}

static void static_mask_apply_box(SceneState *scene,
                                  const PresetObject *po) {
    if (!scene || !scene->static_mask || !scene->config || !po) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    float center_x = po->position_x * (float)(scene->config->window_w);
    float center_y = po->position_y * (float)(scene->config->window_h);
    float half_w_world = po->size_x * (float)scene->config->window_w;
    float half_h_world = po->size_y * (float)scene->config->window_h;
    float half_diag = sqrtf(half_w_world * half_w_world +
                            half_h_world * half_h_world);

    int min_x = (int)floorf(((center_x - half_diag) /
                             (float)scene->config->window_w) * (float)(w - 1));
    int max_x = (int)ceilf(((center_x + half_diag) /
                            (float)scene->config->window_w) * (float)(w - 1));
    int min_y = (int)floorf(((center_y - half_diag) /
                             (float)scene->config->window_h) * (float)(h - 1));
    int max_y = (int)ceilf(((center_y + half_diag) /
                            (float)scene->config->window_h) * (float)(h - 1));

    if (min_x < 1) min_x = 1;
    if (max_x > w - 2) max_x = w - 2;
    if (min_y < 1) min_y = 1;
    if (max_y > h - 2) max_y = h - 2;

    float cos_a = cosf(po->angle);
    float sin_a = sinf(po->angle);
    for (int y = min_y; y <= max_y; ++y) {
        float norm_y = (float)y / (float)(h - 1);
        float world_y = norm_y * (float)scene->config->window_h;
        for (int x = min_x; x <= max_x; ++x) {
            float norm_x = (float)x / (float)(w - 1);
            float world_x = norm_x * (float)scene->config->window_w;
            float rel_x = world_x - center_x;
            float rel_y = world_y - center_y;
            float local_x =  rel_x * cos_a + rel_y * sin_a;
            float local_y = -rel_x * sin_a + rel_y * cos_a;
            if (fabsf(local_x) <= half_w_world &&
                fabsf(local_y) <= half_h_world) {
                static_mask_mark_cell(scene, x, y);
            }
        }
    }
}

static void static_mask_apply_preset(SceneState *scene,
                                     const PresetObject *po) {
    if (!scene || !po || !po->is_static) return;
    if (po->type == PRESET_OBJECT_CIRCLE) {
        static_mask_apply_circle(scene, po);
    } else {
        static_mask_apply_box(scene, po);
    }
}

static bool rasterize_import_to_mask(SceneState *scene,
                                     const ImportedShape *imp,
                                     uint8_t *out_mask,
                                     size_t mask_count) {
    if (!scene || !imp || !out_mask || !scene->config || mask_count == 0) return false;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return false;
    memset(out_mask, 0, mask_count);

    float span_x = 1.0f, span_y = 1.0f;
    import_compute_span_from_window(scene->config->window_w, scene->config->window_h, &span_x, &span_y);
    float grid_min_dim = (float)((w < h) ? w : h);
    if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
    float pos_x_unit = import_pos_to_unit(imp->position_x, span_x);
    float pos_y_unit = import_pos_to_unit(imp->position_y, span_y);

    const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);

    bool raster_ok = false;
    if (asset) {
        ShapeAssetBounds bnds;
        if (shape_asset_bounds(asset, &bnds) && bnds.valid) {
            float max_dim = shape_bounds_max_dim(&bnds);
            if (max_dim > 0.0001f) {
                const float desired_fit = 0.25f;
                float norm = (imp->scale * desired_fit) / max_dim;
                float raster_scale = norm * grid_min_dim;
                ShapeAssetRasterOptions ropts = {
                    .margin_cells = 1.0f,
                    .stroke = 1.0f,
                    .position_x_norm = pos_x_unit,
                    .position_y_norm = pos_y_unit,
                    .rotation_deg = imp->rotation_deg,
                    .scale = raster_scale,
                    .center_fit = false,
                };
                PhysicsObject phys = {0};
                SceneObjectBase base = {
                    .shape_id = (imp->shape_id >= 0) ? imp->shape_id : 0,
                    .position = vec2(pos_x_unit, pos_y_unit),
                    .rotation = imp->rotation_deg * (float)M_PI / 180.0f,
                    .scale = vec2(raster_scale, raster_scale),
                    .flags = 0,
                };
                raster_ok = physics_object_from_asset(asset, &base, w, h, &ropts, &phys);
                if (raster_ok && phys.mask) {
                    memcpy(out_mask, phys.mask, mask_count);
                }
                physics_object_free(&phys);
            }
        }
    }

    if (!raster_ok) {
        ShapeDocument doc;
        if (!shape_import_load(imp->path, &doc) || doc.shapeCount == 0) {
            return false;
        }

        const Shape *shape = &doc.shapes[0];
        ShapeBounds sb = {0};
        if (!shape_import_bounds(shape, &sb) || !sb.valid) {
            ShapeDocument_Free(&doc);
            return false;
        }
        float max_dim = fmaxf(sb.max_x - sb.min_x, sb.max_y - sb.min_y);
        if (max_dim <= 0.0001f) max_dim = 1.0f;
        const float desired_fit = 0.25f;
        float norm = (imp->scale * desired_fit) / max_dim;
        float raster_scale = norm * grid_min_dim;
        ShapeRasterOptions ropts = {
            .margin_cells = 1.0f,
            .stroke = 1.0f,
            .max_error = 0.5f,
            .position_x_norm = pos_x_unit,
            .position_y_norm = pos_y_unit,
            .rotation_deg = imp->rotation_deg,
            .scale = raster_scale,
            .center_fit = false,
        };

        raster_ok = shape_import_rasterize(shape, w, h, &ropts, out_mask);
        ShapeDocument_Free(&doc);
    }

    return raster_ok;
}

void scene_masks_free_emitter_masks(SceneState *scene) {
    if (!scene) return;
    for (size_t i = 0; i < MAX_FLUID_EMITTERS; ++i) {
        free(scene->emitter_masks[i].mask);
        scene->emitter_masks[i].mask = NULL;
        scene->emitter_masks[i].min_x = scene->emitter_masks[i].min_y = 0;
        scene->emitter_masks[i].max_x = scene->emitter_masks[i].max_y = -1;
    }
    scene->emitter_masks_dirty = true;
}

static void static_mask_apply_imports(SceneState *scene) {
    if (!scene || !scene->config || !scene->static_mask) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;
    bool emitter_on_imp[MAX_IMPORTED_SHAPES] = {0};
    if (scene->preset) {
        for (size_t ei = 0; ei < scene->preset->emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
            int ai = scene->preset->emitters[ei].attached_import;
            if (ai >= 0 && ai < (int)MAX_IMPORTED_SHAPES) emitter_on_imp[ai] = true;
        }
    }
    size_t mask_count = (size_t)w * (size_t)h;
    uint8_t *tmp = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
    if (!tmp) return;

    float span_x = 1.0f, span_y = 1.0f;
    import_compute_span_from_window(scene->config->window_w, scene->config->window_h, &span_x, &span_y);
    float grid_min_dim = (float)((w < h) ? w : h);
    if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;

    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        const ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled || imp->path[0] == '\0') continue;
        if (!imp->is_static) continue; // skip dynamic shapes for now
        if (emitter_on_imp[i]) continue; // leave cells free for emitter coupling

        float pos_x_unit = import_pos_to_unit(imp->position_x, span_x);
        float pos_y_unit = import_pos_to_unit(imp->position_y, span_y);

        const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
        bool raster_ok = false;
        if (asset) {
            ShapeAssetBounds bnds;
            if (!shape_asset_bounds(asset, &bnds) || !bnds.valid) continue;
            float max_dim = shape_bounds_max_dim(&bnds);
            if (max_dim <= 0.0001f) continue;
            const float desired_fit = 0.25f;
            float norm = (imp->scale * desired_fit) / max_dim;
            float raster_scale = norm * grid_min_dim;
            ShapeAssetRasterOptions ropts = {
                .margin_cells = 1.0f,
                .stroke = 1.0f,
                .position_x_norm = pos_x_unit,
                .position_y_norm = pos_y_unit,
                .rotation_deg = imp->rotation_deg,
                .scale = raster_scale,
                .center_fit = false,
            };
            memset(tmp, 0, mask_count);
            int sid = (imp->shape_id >= 0) ? imp->shape_id : (int)i;
            SceneObjectBase base = {
                .shape_id = sid,
                .position = vec2(pos_x_unit, pos_y_unit),
                .rotation = imp->rotation_deg * (float)M_PI / 180.0f,
                .scale = vec2(raster_scale, raster_scale),
                .flags = 0,
            };
            PhysicsObject phys = {0};
            raster_ok = physics_object_from_asset(asset, &base, w, h, &ropts, &phys);
            if (raster_ok && phys.mask) {
                memcpy(tmp, phys.mask, mask_count);
            }
            physics_object_free(&phys);
        }

        if (!raster_ok) {
            ShapeDocument doc;
            if (!shape_import_load(imp->path, &doc) || doc.shapeCount == 0) {
                continue;
            }

            const Shape *shape = &doc.shapes[0];
            ShapeBounds sb = {0};
            if (!shape_import_bounds(shape, &sb) || !sb.valid) {
                ShapeDocument_Free(&doc);
                continue;
            }
            float max_dim = fmaxf(sb.max_x - sb.min_x, sb.max_y - sb.min_y);
            if (max_dim <= 0.0001f) max_dim = 1.0f;
            const float desired_fit = 0.25f;
            float norm = (imp->scale * desired_fit) / max_dim;
            float raster_scale = norm * grid_min_dim;
            ShapeRasterOptions ropts = {
                .margin_cells = 1.0f,
                .stroke = 1.0f,
                .max_error = 0.5f,
                .position_x_norm = pos_x_unit,
                .position_y_norm = pos_y_unit,
                .rotation_deg = imp->rotation_deg,
                .scale = raster_scale,
                .center_fit = false,
            };

            memset(tmp, 0, mask_count);
            raster_ok = shape_import_rasterize(shape, w, h, &ropts, tmp);
            if (!raster_ok) {
                fprintf(stderr, "[import] Rasterization failed for %s\n", imp->path);
            }
            ShapeDocument_Free(&doc);
        }

        if (raster_ok) {
            apply_mask_or(scene->static_mask, tmp, mask_count);
        }
    }

    free(tmp);
}

void scene_masks_build_static(SceneState *scene) {
    if (!scene || !scene->config || !scene->static_mask) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;
    size_t mask_count = (size_t)w * (size_t)h;
    memset(scene->static_mask, 0, mask_count);

    // Apply preset objects (static only).
    if (scene->preset) {
        bool emitter_on_obj[MAX_PRESET_OBJECTS] = {0};
        for (size_t ei = 0; ei < scene->preset->emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
            int ao = scene->preset->emitters[ei].attached_object;
            if (ao >= 0 && ao < (int)MAX_PRESET_OBJECTS) emitter_on_obj[ao] = true;
        }
        for (size_t i = 0; i < scene->preset->object_count && i < MAX_PRESET_OBJECTS; ++i) {
            const PresetObject *po = &scene->preset->objects[i];
            if (!po->is_static) continue;
            if (emitter_on_obj[i]) continue;
            static_mask_apply_preset(scene, po);
        }
    }

    // Apply static imports (non-gravity)
    static_mask_apply_imports(scene);

    scene->obstacle_mask_dirty = true;
}

void scene_masks_build_emitter(SceneState *scene) {
    if (!scene || !scene->config || !scene->preset) return;
    scene_masks_free_emitter_masks(scene);
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;
    size_t mask_count = (size_t)w * (size_t)h;
    (void)mask_count; // silence if unused in release

    for (size_t ei = 0; ei < scene->preset->emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
        const FluidEmitter *em = &scene->preset->emitters[ei];
        int imp_idx = em->attached_import;
        if (imp_idx < 0 || imp_idx >= (int)scene->import_shape_count) continue;
        const ImportedShape *imp = &scene->import_shapes[imp_idx];
        if (!imp->enabled) continue;

        uint8_t *buf = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
        if (!buf) continue;
        if (!rasterize_import_to_mask(scene, imp, buf, (size_t)w * (size_t)h)) {
            free(buf);
            continue;
        }
        int min_x = w, min_y = h, max_x = -1, max_y = -1;
        size_t hit = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (buf[(size_t)y * (size_t)w + (size_t)x]) {
                    ++hit;
                    if (x < min_x) min_x = x;
                    if (x > max_x) max_x = x;
                    if (y < min_y) min_y = y;
                    if (y > max_y) max_y = y;
                }
            }
        }
        if (hit == 0 || max_x < min_x || max_y < min_y) {
            fprintf(stderr, "[emitter] mask empty for emitter %zu import %d (%s)\n",
                    ei, imp_idx, imp->path);
            free(buf);
            continue;
        }
        scene->emitter_masks[ei].mask = buf;
        scene->emitter_masks[ei].min_x = min_x;
        scene->emitter_masks[ei].max_x = max_x;
        scene->emitter_masks[ei].min_y = min_y;
        scene->emitter_masks[ei].max_y = max_y;
    }
    scene->emitter_masks_dirty = false;
}

static void scene_compute_obstacle_distance(SceneState *scene) {
    if (!scene || !scene->config || !scene->obstacle_distance) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;
    size_t count = (size_t)w * (size_t)h;
    float *distance = scene->obstacle_distance;
    const uint8_t *mask = scene->obstacle_mask;
    if (!mask) {
        for (size_t i = 0; i < count; ++i) {
            distance[i] = 1.0f;
        }
        return;
    }

    const float INF = (float)(w + h + 10);
    int *queue_x = (int *)malloc(count * sizeof(int));
    int *queue_y = (int *)malloc(count * sizeof(int));
    if (!queue_x || !queue_y) {
        for (size_t i = 0; i < count; ++i) {
            distance[i] = mask[i] ? 0.0f : 1.0f;
        }
        free(queue_x);
        free(queue_y);
        return;
    }

    size_t head = 0;
    size_t tail = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            if (mask[id]) {
                distance[id] = 0.0f;
                queue_x[tail] = x;
                queue_y[tail] = y;
                ++tail;
            } else {
                distance[id] = INF;
            }
        }
    }

    if (tail == 0) {
        for (size_t i = 0; i < count; ++i) {
            distance[i] = 1.0f;
        }
        free(queue_x);
        free(queue_y);
        return;
    }

    static const int OFFSETS[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head];
        size_t id = (size_t)y * (size_t)w + (size_t)x;
        ++head;
        float current = distance[id];
        for (int k = 0; k < 4; ++k) {
            int nx = x + OFFSETS[k][0];
            int ny = y + OFFSETS[k][1];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            size_t nid = (size_t)ny * (size_t)w + (size_t)nx;
            float candidate = current + 1.0f;
            if (distance[nid] > candidate) {
                distance[nid] = candidate;
                queue_x[tail] = nx;
                queue_y[tail] = ny;
                ++tail;
            }
        }
    }

    float max_d = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        float d = distance[i];
        if (d > max_d && d < INF) {
            max_d = d;
        }
    }
    if (max_d <= 0.0f) {
        for (size_t i = 0; i < count; ++i) {
            distance[i] = mask[i] ? 0.0f : 1.0f;
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            float d = distance[i];
            if (d >= INF) d = max_d;
            distance[i] = d / max_d;
        }
    }

    free(queue_x);
    free(queue_y);
}

static void obstacle_mask_apply(SceneState *scene) {
    if (!scene || !scene->config) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    size_t count = (size_t)w * (size_t)h;
    if (!scene->obstacle_mask) {
        if (scene->obstacle_distance) {
            for (size_t i = 0; i < count; ++i) {
                scene->obstacle_distance[i] = 1.0f;
            }
        }
        scene->obstacle_mask_dirty = false;
        return;
    }
    if (scene->static_mask) {
        memcpy(scene->obstacle_mask, scene->static_mask, count);
    } else {
        memset(scene->obstacle_mask, 0, count);
    }
    if (scene->obstacle_velX) {
        memset(scene->obstacle_velX, 0, count * sizeof(float));
    }
    if (scene->obstacle_velY) {
        memset(scene->obstacle_velY, 0, count * sizeof(float));
    }

    // TODO: dynamic bodies rasterization can be added here

    scene_compute_obstacle_distance(scene);
    scene->obstacle_mask_dirty = false;
}

void scene_masks_build_obstacle(SceneState *scene) {
    obstacle_mask_apply(scene);
}

void scene_masks_mark_emitters_dirty(SceneState *scene) {
    if (!scene) return;
    scene->emitter_masks_dirty = true;
}

// Rasterize dynamic bodies into obstacle_mask/vel buffers.
void scene_masks_rasterize_dynamic(SceneState *scene) {
    if (!scene || !scene->config || !scene->obstacle_mask) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;
    size_t count = (size_t)w * (size_t)h;

    // Start from static mask already in obstacle_mask; zero vel buffers.
    if (scene->static_mask) {
        memcpy(scene->obstacle_mask, scene->static_mask, count);
    } else {
        memset(scene->obstacle_mask, 0, count);
    }
    if (scene->obstacle_velX) memset(scene->obstacle_velX, 0, count * sizeof(float));
    if (scene->obstacle_velY) memset(scene->obstacle_velY, 0, count * sizeof(float));

    // Rasterize each dynamic body (gravity-enabled imports or other non-static objects).
    ObjectManager *mgr = &scene->objects;
    if (!mgr || mgr->count <= 0) {
        scene_compute_obstacle_distance(scene);
        scene->obstacle_mask_dirty = false;
        return;
    }

    float sx = (float)(w - 1) / (float)(scene->config->window_w > 0 ? scene->config->window_w : 1);
    float sy = (float)(h - 1) / (float)(scene->config->window_h > 0 ? scene->config->window_h : 1);

    for (int i = 0; i < mgr->count; ++i) {
        SceneObject *obj = &mgr->objects[i];
        RigidBody2D *b = &obj->body;
        if (b->is_static) continue;

        // Compute AABB in grid space.
        float gx_min = 0.0f, gx_max = 0.0f, gy_min = 0.0f, gy_max = 0.0f;
        const ImportedShape *imp = NULL;
        ImportedShape temp_imp = {0};
        if (obj->source_import >= 0 && obj->source_import < (int)scene->import_shape_count) {
            imp = &scene->import_shapes[obj->source_import];
            // Override with current body pose so raster reflects runtime transform.
            temp_imp = *imp;
            temp_imp.position_x = b->position.x / (float)scene->config->window_w;
            temp_imp.position_y = b->position.y / (float)scene->config->window_h;
            temp_imp.rotation_deg = b->angle * 180.0f / (float)M_PI;
            imp = &temp_imp;
        }

        // Compute AABB in grid space using either collider verts or asset bounds if available.
        if (imp) {
            float span_x = 1.0f, span_y = 1.0f;
            import_compute_span_from_window(scene->config->window_w, scene->config->window_h, &span_x, &span_y);
            float pos_unit_x = import_pos_to_unit(imp->position_x, span_x);
            float pos_unit_y = import_pos_to_unit(imp->position_y, span_y);
            float grid_min_dim = (float)((w < h) ? w : h);
            if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
            const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
            if (asset) {
                ShapeAssetBounds bnds;
                if (shape_asset_bounds(asset, &bnds) && bnds.valid) {
                    float max_dim = fmaxf(bnds.max_x - bnds.min_x, bnds.max_y - bnds.min_y);
                    if (max_dim > 0.0001f) {
                        const float desired_fit = 0.25f;
                        float norm = (imp->scale * desired_fit) / max_dim;
                        float raster_scale = norm * grid_min_dim;
                        float half_span = raster_scale * 0.5f;
                        gx_min = (pos_unit_x * (float)(w - 1)) - half_span;
                        gx_max = (pos_unit_x * (float)(w - 1)) + half_span;
                        gy_min = (pos_unit_y * (float)(h - 1)) - half_span;
                        gy_max = (pos_unit_y * (float)(h - 1)) + half_span;
                    }
                }
            }
        }

        // Fallback to collider-based bounds if not set
        if (gx_max <= gx_min || gy_max <= gy_min) {
            if (b->shape == RIGID2D_SHAPE_CIRCLE) {
                float r = b->radius;
                float cx = b->position.x * sx;
                float cy = b->position.y * sy;
                float rr = r * ((sx + sy) * 0.5f);
                gx_min = cx - rr;
                gx_max = cx + rr;
                gy_min = cy - rr;
                gy_max = cy + rr;
            } else if (b->shape == RIGID2D_SHAPE_POLY && b->poly.count >= 3) {
                gx_min = gx_max = b->poly.verts[0].x * sx;
                gy_min = gy_max = b->poly.verts[0].y * sy;
                for (int v = 1; v < b->poly.count; ++v) {
                    float x = b->poly.verts[v].x * sx;
                    float y = b->poly.verts[v].y * sy;
                    if (x < gx_min) gx_min = x;
                    if (x > gx_max) gx_max = x;
                    if (y < gy_min) gy_min = y;
                    if (y > gy_max) gy_max = y;
                }
            } else {
                continue;
            }
        }

        int min_x = (int)floorf(gx_min);
        int max_x = (int)ceilf(gx_max);
        int min_y = (int)floorf(gy_min);
        int max_y = (int)ceilf(gy_max);
        if (min_x < 1) min_x = 1;
        if (max_x > w - 2) max_x = w - 2;
        if (min_y < 1) min_y = 1;
        if (max_y > h - 2) max_y = h - 2;
        if (min_x > max_x || min_y > max_y) continue;

        float vel_x = b->velocity.x * sx;
        float vel_y = b->velocity.y * sy;

        // If we have the asset, rasterize the true shape at current transform for a perfect mask.
        bool rastered = false;
        if (imp) {
            size_t mask_count = (size_t)w * (size_t)h;
            uint8_t *tmp = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
            if (tmp) {
                // Use same raster method as emitter mask to avoid code duplication, but with live pose.
                ImportedShape imp_pose = *imp;
                imp_pose.position_x = b->position.x / (float)scene->config->window_w;
                imp_pose.position_y = b->position.y / (float)scene->config->window_h;
                imp_pose.rotation_deg = b->angle * 180.0f / (float)M_PI;
                bool ok = rasterize_import_to_mask(scene, &imp_pose, tmp, mask_count);
                if (scene->config && scene->config->collider_debug_logs) {
                    fprintf(stderr,
                            "[dynmask] imp=%d body=%d angle=%.2f rot_deg=%.2f raster=%d\n",
                            obj->source_import,
                            i,
                            b->angle * 180.0f / (float)M_PI,
                            imp_pose.rotation_deg,
                            ok ? 1 : 0);
                }
                if (ok) {
                    int bminx = w, bmaxx = -1, bminy = h, bmaxy = -1;
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            if (!tmp[(size_t)y * (size_t)w + (size_t)x]) continue;
                            if (x < bminx) bminx = x;
                            if (x > bmaxx) bmaxx = x;
                            if (y < bminy) bminy = y;
                            if (y > bmaxy) bmaxy = y;
                        }
                    }
                    if (bmaxx >= bminx && bmaxy >= bminy) {
                        apply_mask_or(scene->obstacle_mask, tmp, mask_count);
                        if (scene->obstacle_velX || scene->obstacle_velY) {
                            for (int y = bminy; y <= bmaxy; ++y) {
                                for (int x = bminx; x <= bmaxx; ++x) {
                                    size_t id = (size_t)y * (size_t)w + (size_t)x;
                                    if (!tmp[id]) continue;
                                    if (scene->obstacle_velX) scene->obstacle_velX[id] = vel_x;
                                    if (scene->obstacle_velY) scene->obstacle_velY[id] = vel_y;
                                }
                            }
                        }
                        rastered = true;
                    }
                }
                free(tmp);
            }
        }
        if (rastered) continue;

        if (b->shape == RIGID2D_SHAPE_CIRCLE) {
            float cx = b->position.x * sx;
            float cy = b->position.y * sy;
            float rr = b->radius * ((sx + sy) * 0.5f);
            float r2 = rr * rr;
            if (scene->config && scene->config->collider_debug_logs) {
                fprintf(stderr, "[dynmask] imp=%d body=%d fallback=circle\n", obj->source_import, i);
            }
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float dx = (float)x - cx;
                    float dy = (float)y - cy;
                    if (dx * dx + dy * dy > r2) continue;
                    size_t id = (size_t)y * (size_t)w + (size_t)x;
                    scene->obstacle_mask[id] = 1;
                    if (scene->obstacle_velX) scene->obstacle_velX[id] = vel_x;
                    if (scene->obstacle_velY) scene->obstacle_velY[id] = vel_y;
                }
            }
        } else if (b->shape == RIGID2D_SHAPE_POLY) {
            Vec2 verts_scaled[32];
            int vc = b->poly.count;
            if (vc > 32) vc = 32;
            float cos_a = cosf(b->angle);
            float sin_a = sinf(b->angle);
            for (int v = 0; v < vc; ++v) {
                // Transform body-local verts into world, then to grid units.
                float lx = b->poly.verts[v].x;
                float ly = b->poly.verts[v].y;
                float rx = lx * cos_a - ly * sin_a;
                float ry = lx * sin_a + ly * cos_a;
                float wx = b->position.x + rx;
                float wy = b->position.y + ry;
                verts_scaled[v].x = wx * sx;
                verts_scaled[v].y = wy * sy;
            }
            if (scene->config && scene->config->collider_debug_logs) {
                fprintf(stderr, "[dynmask] imp=%d body=%d fallback=poly rot_deg=%.2f\n",
                        obj->source_import,
                        i,
                        b->angle * 180.0f / (float)M_PI);
            }
            // Point-in-polygon raster: transform verts are already in world units.
            // Use winding test per cell center.
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float px = (float)x + 0.5f;
                    float py = (float)y + 0.5f;
                    bool inside = false;
                    for (int a = 0, bidx = vc - 1; a < vc; bidx = a++) {
                        float ax = verts_scaled[a].x;
                        float ay = verts_scaled[a].y;
                        float bx = verts_scaled[bidx].x;
                        float by = verts_scaled[bidx].y;
                        bool cond = ((ay > py) != (by > py)) &&
                                    (px < (bx - ax) * (py - ay) / ((by - ay) + 1e-6f) + ax);
                        if (cond) inside = !inside;
                    }
                    if (!inside) continue;
                    size_t id = (size_t)y * (size_t)w + (size_t)x;
                    scene->obstacle_mask[id] = 1;
                    if (scene->obstacle_velX) scene->obstacle_velX[id] = vel_x;
                    if (scene->obstacle_velY) scene->obstacle_velY[id] = vel_y;
                }
            }
        }
    }

    scene_compute_obstacle_distance(scene);
    scene->obstacle_mask_dirty = false;
}
