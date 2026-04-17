#include "app/sim_runtime_backend.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/scene_state.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "import/shape_import.h"
#include "physics/fluid2d/fluid2d.h"
#include "physics/fluid2d/fluid2d_boundary.h"
#include "physics/objects/physics_object_builder.h"
#include "render/import_project.h"

typedef struct SceneEmitterMask2D {
    uint8_t *mask;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
} SceneEmitterMask2D;

typedef struct SimRuntimeBackend2D {
    Fluid2D *fluid;
    uint8_t *static_mask;
    uint8_t *obstacle_mask;
    float *obstacle_vel_x;
    float *obstacle_vel_y;
    float *obstacle_distance;
    bool obstacle_mask_dirty;
    SceneEmitterMask2D emitter_masks[MAX_FLUID_EMITTERS];
    bool emitter_masks_dirty;
    int wind_ramp_steps;
} SimRuntimeBackend2D;

static const float BRUSH_DENSITY = 20.0f;
static const float BRUSH_VEL_SCALE = 35.0f;
static const float BRUSH_VELOCITY_DENSITY = 4.0f;
static const float EMITTER_POWER_BOOST = 40.0f;

static SimRuntimeBackend2D *backend_2d_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend2D *)backend->impl : NULL;
}

static const SimRuntimeBackend2D *backend_2d_state_const(const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend2D *)backend->impl : NULL;
}

static void backend_2d_free_emitter_masks(SimRuntimeBackend2D *state) {
    if (!state) return;
    for (size_t i = 0; i < MAX_FLUID_EMITTERS; ++i) {
        free(state->emitter_masks[i].mask);
        state->emitter_masks[i].mask = NULL;
        state->emitter_masks[i].min_x = 0;
        state->emitter_masks[i].min_y = 0;
        state->emitter_masks[i].max_x = -1;
        state->emitter_masks[i].max_y = -1;
    }
    state->emitter_masks_dirty = true;
}

static void backend_2d_destroy(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (state) {
        fluid2d_destroy(state->fluid);
        free(state->static_mask);
        free(state->obstacle_mask);
        free(state->obstacle_vel_x);
        free(state->obstacle_vel_y);
        free(state->obstacle_distance);
        backend_2d_free_emitter_masks(state);
        free(state);
    }
    free(backend);
}

static bool backend_2d_valid(const SimRuntimeBackend *backend) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    return state && state->fluid;
}

static void backend_2d_clear(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!state || !state->fluid) return;
    fluid2d_clear(state->fluid);
}

static void backend_2d_window_to_grid(const AppConfig *cfg,
                                      int win_x,
                                      int win_y,
                                      int *out_gx,
                                      int *out_gy) {
    float sx = (float)win_x / (float)(cfg->window_w > 0 ? cfg->window_w : 1);
    float sy = (float)win_y / (float)(cfg->window_h > 0 ? cfg->window_h : 1);

    int gx = (int)(sx * (float)cfg->grid_w);
    int gy = (int)(sy * (float)cfg->grid_h);

    if (gx < 0) gx = 0;
    if (gx >= cfg->grid_w) gx = cfg->grid_w - 1;
    if (gy < 0) gy = 0;
    if (gy >= cfg->grid_h) gy = cfg->grid_h - 1;

    *out_gx = gx;
    *out_gy = gy;
}

static bool backend_2d_apply_brush_sample(SimRuntimeBackend *backend,
                                          const AppConfig *cfg,
                                          const StrokeSample *sample) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!state || !state->fluid || !cfg || !sample) return false;

    int gx = 0;
    int gy = 0;
    backend_2d_window_to_grid(cfg, sample->x, sample->y, &gx, &gy);

    float inv_w = (float)(cfg->window_w > 0 ? cfg->window_w : 1);
    float inv_h = (float)(cfg->window_h > 0 ? cfg->window_h : 1);
    float vx = (sample->vx / inv_w) * BRUSH_VEL_SCALE;
    float vy = (sample->vy / inv_h) * BRUSH_VEL_SCALE;

    switch (sample->mode) {
    case BRUSH_MODE_VELOCITY:
        fluid2d_add_velocity(state->fluid, gx, gy, vx, vy);
        fluid2d_add_density(state->fluid, gx, gy, BRUSH_VELOCITY_DENSITY);
        break;
    case BRUSH_MODE_DENSITY:
    default:
        fluid2d_add_density(state->fluid, gx, gy, BRUSH_DENSITY);
        fluid2d_add_velocity(state->fluid, gx, gy, vx * 0.25f, vy * 0.25f);
        break;
    }

    return true;
}

static inline float backend_2d_import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static inline float backend_2d_shape_bounds_max_dim(const ShapeAssetBounds *bounds) {
    if (!bounds || !bounds->valid) return 0.0f;
    float sx = bounds->max_x - bounds->min_x;
    float sy = bounds->max_y - bounds->min_y;
    return (sx > sy) ? sx : sy;
}

static void backend_2d_apply_mask_or(uint8_t *dst, const uint8_t *src, size_t count) {
    if (!dst || !src) return;
    for (size_t i = 0; i < count; ++i) {
        if (src[i]) dst[i] = 1;
    }
}

static bool backend_2d_rasterize_import_to_mask(const SceneState *scene,
                                                const ImportedShape *imp,
                                                uint8_t *out_mask,
                                                size_t mask_count) {
    if (!scene || !scene->config || !imp || !out_mask || mask_count == 0) return false;

    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return false;
    memset(out_mask, 0, mask_count);

    float span_x = 1.0f;
    float span_y = 1.0f;
    import_compute_span_from_window(scene->config->window_w,
                                    scene->config->window_h,
                                    &span_x,
                                    &span_y);
    float grid_min_dim = (float)((w < h) ? w : h);
    if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
    float pos_x_unit = backend_2d_import_pos_to_unit(imp->position_x, span_x);
    float pos_y_unit = backend_2d_import_pos_to_unit(imp->position_y, span_y);

    const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
    bool raster_ok = false;
    if (asset) {
        ShapeAssetBounds bounds;
        if (shape_asset_bounds(asset, &bounds) && bounds.valid) {
            float max_dim = backend_2d_shape_bounds_max_dim(&bounds);
            if (max_dim > 0.0001f) {
                const float desired_fit = 0.25f;
                float norm = (imp->scale * desired_fit) / max_dim;
                float raster_scale = norm * grid_min_dim;
                ShapeAssetRasterOptions opts = {
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
                raster_ok = physics_object_from_asset(asset, &base, w, h, &opts, &phys);
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
        ShapeBounds bounds = {0};
        if (!shape_import_bounds(shape, &bounds) || !bounds.valid) {
            ShapeDocument_Free(&doc);
            return false;
        }

        float max_dim = fmaxf(bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y);
        if (max_dim <= 0.0001f) max_dim = 1.0f;
        {
            const float desired_fit = 0.25f;
            float norm = (imp->scale * desired_fit) / max_dim;
            float raster_scale = norm * grid_min_dim;
            ShapeRasterOptions opts = {
                .margin_cells = 1.0f,
                .stroke = 1.0f,
                .max_error = 0.5f,
                .position_x_norm = pos_x_unit,
                .position_y_norm = pos_y_unit,
                .rotation_deg = imp->rotation_deg,
                .scale = raster_scale,
                .center_fit = false,
            };
            raster_ok = shape_import_rasterize(shape, w, h, &opts, out_mask);
        }
        ShapeDocument_Free(&doc);
    }

    return raster_ok;
}

static void backend_2d_static_mask_mark_cell(const SceneState *scene,
                                             SimRuntimeBackend2D *state,
                                             int gx,
                                             int gy) {
    if (!scene || !scene->config || !state || !state->static_mask) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (gx < 1 || gx >= w - 1 || gy < 1 || gy >= h - 1) return;
    state->static_mask[(size_t)gy * (size_t)w + (size_t)gx] = 1;
}

static void backend_2d_apply_static_circle(const SceneState *scene,
                                           SimRuntimeBackend2D *state,
                                           const PresetObject *object) {
    if (!scene || !scene->config || !state || !state->static_mask || !object) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    int cx = (int)lroundf(object->position_x * (float)(w - 1));
    int cy = (int)lroundf(object->position_y * (float)(h - 1));
    int radius = (int)ceilf(object->size_x * (float)w);
    if (radius < 1) radius = 1;

    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            if (dx * dx + dy * dy <= (float)(radius * radius)) {
                backend_2d_static_mask_mark_cell(scene, state, x, y);
            }
        }
    }
}

static void backend_2d_apply_static_box(const SceneState *scene,
                                        SimRuntimeBackend2D *state,
                                        const PresetObject *object) {
    if (!scene || !scene->config || !state || !state->static_mask || !object) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    float center_x = object->position_x * (float)scene->config->window_w;
    float center_y = object->position_y * (float)scene->config->window_h;
    float half_w_world = object->size_x * (float)scene->config->window_w;
    float half_h_world = object->size_y * (float)scene->config->window_h;
    float half_diag = sqrtf(half_w_world * half_w_world + half_h_world * half_h_world);

    int min_x = (int)floorf(((center_x - half_diag) / (float)scene->config->window_w) * (float)(w - 1));
    int max_x = (int)ceilf(((center_x + half_diag) / (float)scene->config->window_w) * (float)(w - 1));
    int min_y = (int)floorf(((center_y - half_diag) / (float)scene->config->window_h) * (float)(h - 1));
    int max_y = (int)ceilf(((center_y + half_diag) / (float)scene->config->window_h) * (float)(h - 1));

    if (min_x < 1) min_x = 1;
    if (max_x > w - 2) max_x = w - 2;
    if (min_y < 1) min_y = 1;
    if (max_y > h - 2) max_y = h - 2;

    {
        float cos_a = cosf(object->angle);
        float sin_a = sinf(object->angle);
        for (int y = min_y; y <= max_y; ++y) {
            float norm_y = (float)y / (float)(h - 1);
            float world_y = norm_y * (float)scene->config->window_h;
            for (int x = min_x; x <= max_x; ++x) {
                float norm_x = (float)x / (float)(w - 1);
                float world_x = norm_x * (float)scene->config->window_w;
                float rel_x = world_x - center_x;
                float rel_y = world_y - center_y;
                float local_x = rel_x * cos_a + rel_y * sin_a;
                float local_y = -rel_x * sin_a + rel_y * cos_a;
                if (fabsf(local_x) <= half_w_world &&
                    fabsf(local_y) <= half_h_world) {
                    backend_2d_static_mask_mark_cell(scene, state, x, y);
                }
            }
        }
    }
}

static void backend_2d_apply_static_preset(const SceneState *scene,
                                           SimRuntimeBackend2D *state,
                                           const PresetObject *object) {
    if (!object || !object->is_static) return;
    if (object->type == PRESET_OBJECT_CIRCLE) {
        backend_2d_apply_static_circle(scene, state, object);
    } else {
        backend_2d_apply_static_box(scene, state, object);
    }
}

static void backend_2d_apply_static_imports(SimRuntimeBackend *backend, SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !state || !state->static_mask) return;

    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;

    bool emitter_on_import[MAX_IMPORTED_SHAPES] = {0};
    if (scene->preset) {
        for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
            int import_index = scene->preset->emitters[i].attached_import;
            if (import_index >= 0 && import_index < (int)MAX_IMPORTED_SHAPES) {
                emitter_on_import[import_index] = true;
            }
        }
    }

    size_t mask_count = (size_t)w * (size_t)h;
    uint8_t *tmp = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
    if (!tmp) return;

    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        const ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled || imp->path[0] == '\0' || !imp->is_static || emitter_on_import[i]) {
            continue;
        }
        if (backend_2d_rasterize_import_to_mask(scene, imp, tmp, mask_count)) {
            backend_2d_apply_mask_or(state->static_mask, tmp, mask_count);
        }
    }

    free(tmp);
}

static void backend_2d_mark_obstacles_dirty(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (state) {
        state->obstacle_mask_dirty = true;
    }
}

static void backend_2d_build_static_obstacles(SimRuntimeBackend *backend,
                                              SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !state || !state->static_mask) return;

    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;

    size_t mask_count = (size_t)w * (size_t)h;
    memset(state->static_mask, 0, mask_count);

    if (scene->preset) {
        bool emitter_on_object[MAX_PRESET_OBJECTS] = {0};
        for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
            int object_index = scene->preset->emitters[i].attached_object;
            if (object_index >= 0 && object_index < (int)MAX_PRESET_OBJECTS) {
                emitter_on_object[object_index] = true;
            }
        }
        for (size_t i = 0; i < scene->preset->object_count && i < MAX_PRESET_OBJECTS; ++i) {
            if (emitter_on_object[i]) continue;
            backend_2d_apply_static_preset(scene, state, &scene->preset->objects[i]);
        }
    }

    backend_2d_apply_static_imports(backend, scene);
    state->obstacle_mask_dirty = true;
}

static void backend_2d_build_emitter_masks(SimRuntimeBackend *backend,
                                           SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !scene->preset || !state) return;

    backend_2d_free_emitter_masks(state);
    {
        int w = scene->config->grid_w;
        int h = scene->config->grid_h;
        if (w <= 1 || h <= 1) return;
        size_t mask_count = (size_t)w * (size_t)h;
        for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
            const FluidEmitter *emitter = &scene->preset->emitters[i];
            int import_index = emitter->attached_import;
            if (import_index < 0 || import_index >= (int)scene->import_shape_count) continue;
            const ImportedShape *imp = &scene->import_shapes[import_index];
            if (!imp->enabled) continue;

            uint8_t *mask = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
            if (!mask) continue;
            if (!backend_2d_rasterize_import_to_mask(scene, imp, mask, mask_count)) {
                free(mask);
                continue;
            }

            int min_x = w;
            int min_y = h;
            int max_x = -1;
            int max_y = -1;
            size_t hit = 0;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (!mask[(size_t)y * (size_t)w + (size_t)x]) continue;
                    ++hit;
                    if (x < min_x) min_x = x;
                    if (x > max_x) max_x = x;
                    if (y < min_y) min_y = y;
                    if (y > max_y) max_y = y;
                }
            }
            if (hit == 0 || max_x < min_x || max_y < min_y) {
                free(mask);
                continue;
            }

            state->emitter_masks[i].mask = mask;
            state->emitter_masks[i].min_x = min_x;
            state->emitter_masks[i].max_x = max_x;
            state->emitter_masks[i].min_y = min_y;
            state->emitter_masks[i].max_y = max_y;
        }
    }
    state->emitter_masks_dirty = false;
}

static void backend_2d_mark_emitters_dirty(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (state) {
        state->emitter_masks_dirty = true;
    }
}

static void backend_2d_compute_obstacle_distance(const SceneState *scene,
                                                 SimRuntimeBackend2D *state) {
    if (!scene || !scene->config || !state || !state->obstacle_distance) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return;

    size_t count = (size_t)w * (size_t)h;
    float *distance = state->obstacle_distance;
    const uint8_t *mask = state->obstacle_mask;
    if (!mask) {
        for (size_t i = 0; i < count; ++i) {
            distance[i] = 1.0f;
        }
        return;
    }

    {
        const float inf = (float)(w + h + 10);
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
                    distance[id] = inf;
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

        static const int offsets[4][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}
        };

        while (head < tail) {
            int x = queue_x[head];
            int y = queue_y[head];
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float current = distance[id];
            ++head;

            for (int i = 0; i < 4; ++i) {
                int nx = x + offsets[i][0];
                int ny = y + offsets[i][1];
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

        {
            float max_d = 0.0f;
            for (size_t i = 0; i < count; ++i) {
                float d = distance[i];
                if (d > max_d && d < inf) {
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
                    if (d >= inf) d = max_d;
                    distance[i] = d / max_d;
                }
            }
        }

        free(queue_x);
        free(queue_y);
    }
}

static void backend_2d_build_obstacles(SimRuntimeBackend *backend, SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !state) return;

    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    size_t count = (size_t)w * (size_t)h;
    if (!state->obstacle_mask) {
        if (state->obstacle_distance) {
            for (size_t i = 0; i < count; ++i) {
                state->obstacle_distance[i] = 1.0f;
            }
        }
        state->obstacle_mask_dirty = false;
        return;
    }

    if (state->static_mask) {
        memcpy(state->obstacle_mask, state->static_mask, count);
    } else {
        memset(state->obstacle_mask, 0, count);
    }
    if (state->obstacle_vel_x) memset(state->obstacle_vel_x, 0, count * sizeof(float));
    if (state->obstacle_vel_y) memset(state->obstacle_vel_y, 0, count * sizeof(float));

    backend_2d_compute_obstacle_distance(scene, state);
    state->obstacle_mask_dirty = false;
}

static void backend_2d_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                   SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !state || !state->obstacle_mask) return;

    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;
    size_t count = (size_t)w * (size_t)h;

    if (state->static_mask) {
        memcpy(state->obstacle_mask, state->static_mask, count);
    } else {
        memset(state->obstacle_mask, 0, count);
    }
    if (state->obstacle_vel_x) memset(state->obstacle_vel_x, 0, count * sizeof(float));
    if (state->obstacle_vel_y) memset(state->obstacle_vel_y, 0, count * sizeof(float));

    ObjectManager *mgr = &scene->objects;
    if (!mgr || mgr->count <= 0) {
        backend_2d_compute_obstacle_distance(scene, state);
        state->obstacle_mask_dirty = false;
        return;
    }

    {
        float sx = (float)(w - 1) / (float)(scene->config->window_w > 0 ? scene->config->window_w : 1);
        float sy = (float)(h - 1) / (float)(scene->config->window_h > 0 ? scene->config->window_h : 1);

        for (int i = 0; i < mgr->count; ++i) {
            SceneObject *obj = &mgr->objects[i];
            RigidBody2D *body = &obj->body;
            if (body->is_static) continue;

            float gx_min = 0.0f;
            float gx_max = 0.0f;
            float gy_min = 0.0f;
            float gy_max = 0.0f;
            const ImportedShape *imp = NULL;
            ImportedShape temp_imp = {0};

            if (obj->source_import >= 0 && obj->source_import < (int)scene->import_shape_count) {
                imp = &scene->import_shapes[obj->source_import];
                temp_imp = *imp;
                temp_imp.position_x = body->position.x / (float)scene->config->window_w;
                temp_imp.position_y = body->position.y / (float)scene->config->window_h;
                temp_imp.rotation_deg = body->angle * 180.0f / (float)M_PI;
                imp = &temp_imp;
            }

            if (imp) {
                float span_x = 1.0f;
                float span_y = 1.0f;
                import_compute_span_from_window(scene->config->window_w,
                                                scene->config->window_h,
                                                &span_x,
                                                &span_y);
                {
                    float pos_unit_x = backend_2d_import_pos_to_unit(imp->position_x, span_x);
                    float pos_unit_y = backend_2d_import_pos_to_unit(imp->position_y, span_y);
                    float grid_min_dim = (float)((w < h) ? w : h);
                    if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
                    const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
                    if (asset) {
                        ShapeAssetBounds bounds;
                        if (shape_asset_bounds(asset, &bounds) && bounds.valid) {
                            float max_dim = fmaxf(bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y);
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
            }

            if (gx_max <= gx_min || gy_max <= gy_min) {
                if (body->shape == RIGID2D_SHAPE_CIRCLE) {
                    float rr = body->radius * ((sx + sy) * 0.5f);
                    float cx = body->position.x * sx;
                    float cy = body->position.y * sy;
                    gx_min = cx - rr;
                    gx_max = cx + rr;
                    gy_min = cy - rr;
                    gy_max = cy + rr;
                } else if (body->shape == RIGID2D_SHAPE_POLY && body->poly.count >= 3) {
                    gx_min = gx_max = body->poly.verts[0].x * sx;
                    gy_min = gy_max = body->poly.verts[0].y * sy;
                    for (int v = 1; v < body->poly.count; ++v) {
                        float x = body->poly.verts[v].x * sx;
                        float y = body->poly.verts[v].y * sy;
                        if (x < gx_min) gx_min = x;
                        if (x > gx_max) gx_max = x;
                        if (y < gy_min) gy_min = y;
                        if (y > gy_max) gy_max = y;
                    }
                } else {
                    continue;
                }
            }

            {
                int min_x = (int)floorf(gx_min);
                int max_x = (int)ceilf(gx_max);
                int min_y = (int)floorf(gy_min);
                int max_y = (int)ceilf(gy_max);
                if (min_x < 1) min_x = 1;
                if (max_x > w - 2) max_x = w - 2;
                if (min_y < 1) min_y = 1;
                if (max_y > h - 2) max_y = h - 2;
                if (min_x > max_x || min_y > max_y) continue;

                float vel_x = body->velocity.x * sx;
                float vel_y = body->velocity.y * sy;
                bool rastered = false;

                if (imp) {
                    size_t mask_count = (size_t)w * (size_t)h;
                    uint8_t *tmp = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
                    if (tmp) {
                        ImportedShape imp_pose = *imp;
                        imp_pose.position_x = body->position.x / (float)scene->config->window_w;
                        imp_pose.position_y = body->position.y / (float)scene->config->window_h;
                        imp_pose.rotation_deg = body->angle * 180.0f / (float)M_PI;
                        if (backend_2d_rasterize_import_to_mask(scene, &imp_pose, tmp, mask_count)) {
                            int bminx = w;
                            int bmaxx = -1;
                            int bminy = h;
                            int bmaxy = -1;
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
                                backend_2d_apply_mask_or(state->obstacle_mask, tmp, mask_count);
                                for (int y = bminy; y <= bmaxy; ++y) {
                                    for (int x = bminx; x <= bmaxx; ++x) {
                                        size_t id = (size_t)y * (size_t)w + (size_t)x;
                                        if (!tmp[id]) continue;
                                        if (state->obstacle_vel_x) state->obstacle_vel_x[id] = vel_x;
                                        if (state->obstacle_vel_y) state->obstacle_vel_y[id] = vel_y;
                                    }
                                }
                                rastered = true;
                            }
                        }
                        free(tmp);
                    }
                }
                if (rastered) continue;

                if (body->shape == RIGID2D_SHAPE_CIRCLE) {
                    float cx = body->position.x * sx;
                    float cy = body->position.y * sy;
                    float rr = body->radius * ((sx + sy) * 0.5f);
                    float r2 = rr * rr;
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            float dx = (float)x - cx;
                            float dy = (float)y - cy;
                            if (dx * dx + dy * dy > r2) continue;
                            size_t id = (size_t)y * (size_t)w + (size_t)x;
                            state->obstacle_mask[id] = 1;
                            if (state->obstacle_vel_x) state->obstacle_vel_x[id] = vel_x;
                            if (state->obstacle_vel_y) state->obstacle_vel_y[id] = vel_y;
                        }
                    }
                } else if (body->shape == RIGID2D_SHAPE_POLY) {
                    Vec2 verts_scaled[32];
                    int vc = body->poly.count;
                    if (vc > 32) vc = 32;
                    {
                        float cos_a = cosf(body->angle);
                        float sin_a = sinf(body->angle);
                        for (int v = 0; v < vc; ++v) {
                            float lx = body->poly.verts[v].x;
                            float ly = body->poly.verts[v].y;
                            float rx = lx * cos_a - ly * sin_a;
                            float ry = lx * sin_a + ly * cos_a;
                            float wx = body->position.x + rx;
                            float wy = body->position.y + ry;
                            verts_scaled[v].x = wx * sx;
                            verts_scaled[v].y = wy * sy;
                        }
                    }
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            float px = (float)x + 0.5f;
                            float py = (float)y + 0.5f;
                            bool inside = false;
                            for (int a = 0, b = vc - 1; a < vc; b = a++) {
                                float ax = verts_scaled[a].x;
                                float ay = verts_scaled[a].y;
                                float bx = verts_scaled[b].x;
                                float by = verts_scaled[b].y;
                                bool cond = ((ay > py) != (by > py)) &&
                                            (px < (bx - ax) * (py - ay) / ((by - ay) + 1e-6f) + ax);
                                if (cond) inside = !inside;
                            }
                            if (!inside) continue;
                            {
                                size_t id = (size_t)y * (size_t)w + (size_t)x;
                                state->obstacle_mask[id] = 1;
                                if (state->obstacle_vel_x) state->obstacle_vel_x[id] = vel_x;
                                if (state->obstacle_vel_y) state->obstacle_vel_y[id] = vel_y;
                            }
                        }
                    }
                }
            }
        }
    }

    backend_2d_compute_obstacle_distance(scene, state);
    state->obstacle_mask_dirty = false;
}

static inline void backend_2d_emitter_normalize_dir(float *x, float *y) {
    float len = sqrtf((*x) * (*x) + (*y) * (*y));
    if (len > 0.0001f) {
        *x /= len;
        *y /= len;
    } else {
        *x = 0.0f;
        *y = -1.0f;
    }
}

static float backend_2d_emitter_strength_scale(const SceneState *scene,
                                               FluidEmitterType type) {
    if (!scene || !scene->config) return 1.0f;
    switch (type) {
    case EMITTER_DENSITY_SOURCE:
        return scene->config->emitter_density_multiplier;
    case EMITTER_VELOCITY_JET:
        return scene->config->emitter_velocity_multiplier;
    case EMITTER_SINK:
        return scene->config->emitter_sink_multiplier;
    default:
        return 1.0f;
    }
}

static float backend_2d_emitter_total_strength(const SceneState *scene,
                                               const FluidEmitter *emitter,
                                               float dt,
                                               float area_scale) {
    float scale = backend_2d_emitter_strength_scale(scene, emitter->type);
    return emitter->strength * scale * EMITTER_POWER_BOOST * dt * area_scale;
}

static void backend_2d_emitter_apply_mask(SimRuntimeBackend2D *state,
                                          const FluidEmitter *emitter,
                                          const uint8_t *mask,
                                          int min_x,
                                          int max_x,
                                          int min_y,
                                          int max_y,
                                          float vx_dir,
                                          float vy_dir,
                                          float total_strength,
                                          int w,
                                          int h) {
    if (!state || !state->fluid || !mask) return;
    if (w <= 1 || h <= 1) return;
    if (min_x < 1) min_x = 1;
    if (max_x > w - 2) max_x = w - 2;
    if (min_y < 1) min_y = 1;
    if (max_y > h - 2) max_y = h - 2;

    {
        size_t cells = 0;
        for (int y = min_y; y <= max_y; ++y) {
            size_t row = (size_t)y * (size_t)w;
            for (int x = min_x; x <= max_x; ++x) {
                if (mask[row + (size_t)x]) ++cells;
            }
        }
        {
            float per_cell = (cells > 0) ? total_strength / (float)cells : 0.0f;
            if (per_cell <= 0.0f) return;
            for (int y = min_y; y <= max_y; ++y) {
                size_t row = (size_t)y * (size_t)w;
                for (int x = min_x; x <= max_x; ++x) {
                    if (!mask[row + (size_t)x]) continue;
                    switch (emitter->type) {
                    case EMITTER_DENSITY_SOURCE:
                        fluid2d_add_density(state->fluid, x, y, per_cell);
                        fluid2d_add_velocity(state->fluid,
                                             x,
                                             y,
                                             vx_dir * per_cell * 0.25f,
                                             vy_dir * per_cell * 0.25f);
                        break;
                    case EMITTER_VELOCITY_JET:
                        fluid2d_add_velocity(state->fluid, x, y, vx_dir * per_cell, vy_dir * per_cell);
                        fluid2d_add_density(state->fluid, x, y, per_cell * 0.3f);
                        break;
                    case EMITTER_SINK:
                        fluid2d_add_density(state->fluid, x, y, -per_cell * 0.5f);
                        fluid2d_add_velocity(state->fluid,
                                             x,
                                             y,
                                             -vx_dir * per_cell * 0.4f,
                                             -vy_dir * per_cell * 0.4f);
                        break;
                    }
                }
            }
        }
    }
}

static void backend_2d_apply_emitters(SimRuntimeBackend *backend,
                                      SceneState *scene,
                                      double dt) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->preset || !state || !state->fluid) return;
    if (!scene->emitters_enabled) return;
    if (state->emitter_masks_dirty) {
        backend_2d_build_emitter_masks(backend, scene);
    }

    {
        int w = state->fluid->w;
        int h = state->fluid->h;
        const float ref_grid_area = 256.0f * 256.0f;
        float grid_area = (float)w * (float)h;
        if (grid_area < 1.0f) grid_area = 1.0f;
        {
            float area_scale = grid_area / ref_grid_area;
            if (area_scale < 0.25f) area_scale = 0.25f;
            if (area_scale > 4.0f) area_scale = 4.0f;

            for (size_t i = 0; i < scene->preset->emitter_count; ++i) {
                const FluidEmitter *emitter = &scene->preset->emitters[i];
                float vx_dir = emitter->dir_x;
                float vy_dir = emitter->dir_y;
                backend_2d_emitter_normalize_dir(&vx_dir, &vy_dir);
                {
                    float total_strength =
                        backend_2d_emitter_total_strength(scene, emitter, (float)dt, area_scale);
                    int attached_obj = emitter->attached_object;
                    int attached_imp = emitter->attached_import;

                    if (attached_imp >= 0 && attached_imp < (int)scene->import_shape_count) {
                        const ImportedShape *imp = &scene->import_shapes[attached_imp];
                        if (imp) {
                            float rad = imp->rotation_deg * (float)M_PI / 180.0f;
                            float c = cosf(rad);
                            float s = sinf(rad);
                            float rx = vx_dir * c - vy_dir * s;
                            float ry = vx_dir * s + vy_dir * c;
                            vx_dir = rx;
                            vy_dir = ry;
                            backend_2d_emitter_normalize_dir(&vx_dir, &vy_dir);
                        }

                        if (state->emitter_masks[i].mask &&
                            state->emitter_masks[i].max_x >= state->emitter_masks[i].min_x &&
                            state->emitter_masks[i].max_y >= state->emitter_masks[i].min_y) {
                            backend_2d_emitter_apply_mask(state,
                                                          emitter,
                                                          state->emitter_masks[i].mask,
                                                          state->emitter_masks[i].min_x,
                                                          state->emitter_masks[i].max_x,
                                                          state->emitter_masks[i].min_y,
                                                          state->emitter_masks[i].max_y,
                                                          vx_dir,
                                                          vy_dir,
                                                          total_strength,
                                                          w,
                                                          h);
                        } else {
                            const ImportedShape *fallback_imp = &scene->import_shapes[attached_imp];
                            float span_x = 1.0f;
                            float span_y = 1.0f;
                            import_compute_span_from_window(scene->config->window_w,
                                                            scene->config->window_h,
                                                            &span_x,
                                                            &span_y);
                            {
                                float grid_min_dim = (float)((w < h) ? w : h);
                                if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
                                float pos_x_unit = backend_2d_import_pos_to_unit(fallback_imp->position_x, span_x);
                                float pos_y_unit = backend_2d_import_pos_to_unit(fallback_imp->position_y, span_y);
                                int cx = (int)lroundf(pos_x_unit * (float)(w - 1));
                                int cy = (int)lroundf(pos_y_unit * (float)(h - 1));
                                float radius_cells = 10.0f;
                                if (scene->shape_library) {
                                    const ShapeAsset *asset =
                                        shape_lookup_from_path(scene->shape_library, fallback_imp->path);
                                    ShapeAssetBounds bounds;
                                    if (asset && shape_asset_bounds(asset, &bounds) && bounds.valid) {
                                        float max_dim = fmaxf(bounds.max_x - bounds.min_x,
                                                              bounds.max_y - bounds.min_y);
                                        if (max_dim > 0.0001f) {
                                            const float desired_fit = 0.25f;
                                            float norm = (fallback_imp->scale * desired_fit) / max_dim;
                                            radius_cells = norm * grid_min_dim;
                                        }
                                    }
                                }
                                if (radius_cells < 1.0f) radius_cells = 1.0f;
                                {
                                    int radius = (int)ceilf(radius_cells);
                                    int min_x = (cx - radius < 1) ? 1 : cx - radius;
                                    int max_x = (cx + radius > w - 2) ? w - 2 : cx + radius;
                                    int min_y = (cy - radius < 1) ? 1 : cy - radius;
                                    int max_y = (cy + radius > h - 2) ? h - 2 : cy + radius;
                                    uint8_t *tmp = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
                                    if (tmp) {
                                        for (int y = min_y; y <= max_y; ++y) {
                                            for (int x = min_x; x <= max_x; ++x) {
                                                float dx = (float)x - (float)cx;
                                                float dy = (float)y - (float)cy;
                                                if ((dx * dx + dy * dy) <= (float)(radius * radius)) {
                                                    tmp[(size_t)y * (size_t)w + (size_t)x] = 1;
                                                }
                                            }
                                        }
                                        backend_2d_emitter_apply_mask(state,
                                                                      emitter,
                                                                      tmp,
                                                                      min_x,
                                                                      max_x,
                                                                      min_y,
                                                                      max_y,
                                                                      vx_dir,
                                                                      vy_dir,
                                                                      total_strength,
                                                                      w,
                                                                      h);
                                        free(tmp);
                                    }
                                }
                            }
                        }
                        continue;
                    }

                    if (attached_obj >= 0 && attached_obj < (int)scene->preset->object_count) {
                        const PresetObject *obj = &scene->preset->objects[attached_obj];
                        int cx = (int)lroundf(obj->position_x * (float)(w - 1));
                        int cy = (int)lroundf(obj->position_y * (float)(h - 1));
                        float cos_a = cosf(obj->angle);
                        float sin_a = sinf(obj->angle);
                        if (obj->type == PRESET_OBJECT_CIRCLE) {
                            int radius = (int)ceilf(obj->size_x * (float)w);
                            if (radius < 1) radius = 1;
                            {
                                int min_x = (cx - radius - 1 < 1) ? 1 : cx - radius - 1;
                                int max_x = (cx + radius + 1 > w - 2) ? w - 2 : cx + radius + 1;
                                int min_y = (cy - radius - 1 < 1) ? 1 : cy - radius - 1;
                                int max_y = (cy + radius + 1 > h - 2) ? h - 2 : cy + radius + 1;
                                uint8_t *tmp = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
                                if (tmp) {
                                    for (int y = min_y; y <= max_y; ++y) {
                                        for (int x = min_x; x <= max_x; ++x) {
                                            float dx = (float)x - (float)cx;
                                            float dy = (float)y - (float)cy;
                                            if ((dx * dx + dy * dy) <= (float)(radius * radius)) {
                                                tmp[(size_t)y * (size_t)w + (size_t)x] = 1;
                                            }
                                        }
                                    }
                                    backend_2d_emitter_apply_mask(state,
                                                                  emitter,
                                                                  tmp,
                                                                  min_x,
                                                                  max_x,
                                                                  min_y,
                                                                  max_y,
                                                                  vx_dir,
                                                                  vy_dir,
                                                                  total_strength,
                                                                  w,
                                                                  h);
                                    free(tmp);
                                }
                            }
                        } else {
                            float half_w = obj->size_x * (float)w;
                            float half_h = obj->size_y * (float)h;
                            float bound = fmaxf(half_w, half_h);
                            int min_x = (int)fmaxf((float)cx - bound - 1.0f, 1.0f);
                            int max_x = (int)fminf((float)cx + bound + 1.0f, (float)(w - 2));
                            int min_y = (int)fmaxf((float)cy - bound - 1.0f, 1.0f);
                            int max_y = (int)fminf((float)cy + bound + 1.0f, (float)(h - 2));
                            uint8_t *tmp = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
                            if (tmp) {
                                for (int y = min_y; y <= max_y; ++y) {
                                    for (int x = min_x; x <= max_x; ++x) {
                                        float dx = (float)x - (float)cx;
                                        float dy = (float)y - (float)cy;
                                        float local_x = dx * cos_a + dy * sin_a;
                                        float local_y = -dx * sin_a + dy * cos_a;
                                        if (fabsf(local_x) <= half_w && fabsf(local_y) <= half_h) {
                                            tmp[(size_t)y * (size_t)w + (size_t)x] = 1;
                                        }
                                    }
                                }
                                backend_2d_emitter_apply_mask(state,
                                                              emitter,
                                                              tmp,
                                                              min_x,
                                                              max_x,
                                                              min_y,
                                                              max_y,
                                                              vx_dir,
                                                              vy_dir,
                                                              total_strength,
                                                              w,
                                                              h);
                                free(tmp);
                            }
                        }
                        continue;
                    }

                    {
                        int cx = (int)(emitter->position_x * (float)(w - 1));
                        int cy = (int)(emitter->position_y * (float)(h - 1));
                        int radius = (int)(emitter->radius * (float)w);
                        if (radius < 1) radius = 1;
                        for (int y = cy - radius; y <= cy + radius; ++y) {
                            for (int x = cx - radius; x <= cx + radius; ++x) {
                                if (x < 1 || x >= w - 1 || y < 1 || y >= h - 1) continue;
                                {
                                    float dx = (float)x - (float)cx;
                                    float dy = (float)y - (float)cy;
                                    float dist = sqrtf(dx * dx + dy * dy);
                                    if (dist > (float)radius) continue;
                                    {
                                        float falloff = 1.0f - (dist / (float)radius);
                                        if (falloff <= 0.0f) continue;
                                        {
                                            float fall_scaled = total_strength * falloff;
                                            switch (emitter->type) {
                                            case EMITTER_DENSITY_SOURCE:
                                                fluid2d_add_density(state->fluid, x, y, fall_scaled);
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     vx_dir * fall_scaled * 0.25f,
                                                                     vy_dir * fall_scaled * 0.25f);
                                                break;
                                            case EMITTER_VELOCITY_JET:
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     vx_dir * fall_scaled,
                                                                     vy_dir * fall_scaled);
                                                fluid2d_add_density(state->fluid, x, y, fall_scaled * 0.3f);
                                                break;
                                            case EMITTER_SINK:
                                                fluid2d_add_density(state->fluid, x, y, -fall_scaled * 0.5f);
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     -vx_dir * fall_scaled * 0.4f,
                                                                     -vy_dir * fall_scaled * 0.4f);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void backend_2d_apply_boundary_flows(SimRuntimeBackend *backend,
                                            SceneState *scene,
                                            double dt) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->preset || !state || !state->fluid) return;
    if (scene->config && scene->config->sim_mode == SIM_MODE_WIND_TUNNEL) {
        static const int wind_ramp_steps_max = 200;
        float ramp = 1.0f;
        if (state->wind_ramp_steps < wind_ramp_steps_max) {
            ramp = (float)state->wind_ramp_steps / (float)wind_ramp_steps_max;
            if (ramp > 1.0f) ramp = 1.0f;
        }
        fluid2d_boundary_apply_wind(scene->config, scene->preset, state->fluid, dt, ramp);
        if (state->wind_ramp_steps < wind_ramp_steps_max) {
            state->wind_ramp_steps++;
        }
    } else {
        fluid2d_boundary_apply(scene->preset->boundary_flows, state->fluid, dt);
    }
}

static void backend_2d_enforce_boundary_flows(SimRuntimeBackend *backend,
                                              SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->preset || !state || !state->fluid) return;
    if (scene->config && scene->config->sim_mode == SIM_MODE_WIND_TUNNEL) {
        fluid2d_boundary_enforce_wind(scene->config, scene->preset, state->fluid);
    } else {
        fluid2d_boundary_enforce(scene->preset->boundary_flows, state->fluid);
    }
}

static void backend_2d_enforce_obstacles(SimRuntimeBackend *backend,
                                         SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !state || !state->fluid) return;
    backend_2d_rasterize_dynamic_obstacles(backend, scene);
    fluid2d_enforce_solid_mask(state->fluid,
                               state->obstacle_mask,
                               state->obstacle_vel_x,
                               state->obstacle_vel_y);
}

static void backend_2d_step(SimRuntimeBackend *backend,
                            SceneState *scene,
                            const AppConfig *cfg,
                            double dt) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    const BoundaryFlow *flows = NULL;
    if (!scene || !cfg || !state || !state->fluid) return;
    flows = scene->preset ? scene->preset->boundary_flows : NULL;
    fluid2d_step(state->fluid,
                 dt,
                 cfg,
                 flows,
                 state->obstacle_mask,
                 state->obstacle_vel_x,
                 state->obstacle_vel_y);
}

static void backend_2d_inject_object_motion(SimRuntimeBackend *backend,
                                            const SceneState *scene) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!scene || !scene->config || !state || !state->fluid) return;

    const AppConfig *cfg = scene->config;
    if (cfg->window_w <= 0 || cfg->window_h <= 0 || cfg->grid_w <= 0 || cfg->grid_h <= 0) return;

    {
        const float vel_scale = 0.01f;
        for (int i = 0; i < scene->objects.count; ++i) {
            SceneObject *obj = &scene->objects.objects[i];
            if (!obj) continue;
            if (obj->body.is_static || obj->body.locked) continue;

            float sx = obj->body.position.x / (float)cfg->window_w;
            float sy = obj->body.position.y / (float)cfg->window_h;
            int gx = (int)lroundf(sx * (float)cfg->grid_w);
            int gy = (int)lroundf(sy * (float)cfg->grid_h);
            if (gx < 0) gx = 0;
            if (gx >= cfg->grid_w) gx = cfg->grid_w - 1;
            if (gy < 0) gy = 0;
            if (gy >= cfg->grid_h) gy = cfg->grid_h - 1;

            fluid2d_add_velocity(state->fluid,
                                 gx,
                                 gy,
                                 obj->body.velocity.x * vel_scale,
                                 obj->body.velocity.y * vel_scale);
        }
    }
}

static void backend_2d_reset_transient_state(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!state) return;
    state->wind_ramp_steps = 0;
}

static void backend_2d_seed_uniform_velocity_2d(SimRuntimeBackend *backend,
                                                float velocity_x,
                                                float velocity_y) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (!state || !state->fluid) return;

    size_t count = (size_t)state->fluid->w * (size_t)state->fluid->h;
    for (size_t i = 0; i < count; ++i) {
        state->fluid->velX[i] = velocity_x;
        state->fluid->velY[i] = velocity_y;
    }
}

static bool backend_2d_export_snapshot(const SimRuntimeBackend *backend,
                                       double time,
                                       const char *path) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    if (!state || !state->fluid || !path) return false;

    {
        FILE *f = fopen(path, "wb");
        if (!f) {
            perror("fopen snapshot");
            return false;
        }

        {
            uint32_t magic = ('P' << 24) | ('S' << 16) | ('2' << 8) | ('D');
            uint32_t version = 1;
            uint32_t grid_w = (uint32_t)state->fluid->w;
            uint32_t grid_h = (uint32_t)state->fluid->h;
            size_t count = (size_t)grid_w * (size_t)grid_h;

            if (fwrite(&magic, sizeof(magic), 1, f) != 1 ||
                fwrite(&version, sizeof(version), 1, f) != 1 ||
                fwrite(&grid_w, sizeof(grid_w), 1, f) != 1 ||
                fwrite(&grid_h, sizeof(grid_h), 1, f) != 1 ||
                fwrite(&time, sizeof(time), 1, f) != 1 ||
                fwrite(state->fluid->density, sizeof(float), count, f) != count ||
                fwrite(state->fluid->velX, sizeof(float), count, f) != count ||
                fwrite(state->fluid->velY, sizeof(float), count, f) != count) {
                fclose(f);
                return false;
            }
        }

        fclose(f);
    }
    return true;
}

static bool backend_2d_get_fluid_view_2d(const SimRuntimeBackend *backend,
                                         SceneFluidFieldView2D *out_view) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    if (!state || !state->fluid || !out_view) return false;

    out_view->width = state->fluid->w;
    out_view->height = state->fluid->h;
    out_view->cell_count = (size_t)state->fluid->w * (size_t)state->fluid->h;
    out_view->density = state->fluid->density;
    out_view->velocity_x = state->fluid->velX;
    out_view->velocity_y = state->fluid->velY;
    out_view->pressure = state->fluid->pressure;
    return true;
}

static bool backend_2d_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                            SceneObstacleFieldView2D *out_view) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    if (!state || !state->fluid || !out_view) return false;

    out_view->width = state->fluid->w;
    out_view->height = state->fluid->h;
    out_view->cell_count = (size_t)state->fluid->w * (size_t)state->fluid->h;
    out_view->solid_mask = state->obstacle_mask;
    out_view->velocity_x = state->obstacle_vel_x;
    out_view->velocity_y = state->obstacle_vel_y;
    out_view->distance = state->obstacle_distance;
    return true;
}

static bool backend_2d_get_report(const SimRuntimeBackend *backend,
                                  SimRuntimeBackendReport *out_report) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    if (!state || !state->fluid || !out_report) return false;

    *out_report = (SimRuntimeBackendReport){
        .kind = SIM_RUNTIME_BACKEND_KIND_FLUID_2D,
        .domain_w = state->fluid->w,
        .domain_h = state->fluid->h,
        .domain_d = 1,
        .cell_count = (size_t)state->fluid->w * (size_t)state->fluid->h,
        .volumetric_emitters_free_live = false,
        .volumetric_emitters_attached_live = false,
        .volumetric_obstacles_live = false,
        .full_3d_solver_live = false,
        .world_bounds_valid = false,
        .voxel_size = 0.0f,
        .compatibility_view_2d_available = true,
        .compatibility_view_2d_derived = false,
        .compatibility_slice_z = 0,
        .secondary_debug_slice_stack_live = false,
        .secondary_debug_slice_stack_radius = 0,
    };
    return true;
}

static bool backend_2d_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                        int slice_z,
                                                        bool *out_has_fluid,
                                                        bool *out_has_obstacles) {
    const SimRuntimeBackend2D *state = backend_2d_state_const(backend);
    size_t cell_count = 0;
    if (!state || !state->fluid || slice_z != 0) return false;
    if (out_has_fluid) *out_has_fluid = false;
    if (out_has_obstacles) *out_has_obstacles = false;

    cell_count = (size_t)state->fluid->w * (size_t)state->fluid->h;
    for (size_t i = 0; i < cell_count; ++i) {
        if (out_has_fluid && !*out_has_fluid && state->fluid->density[i] > 0.0001f) {
            *out_has_fluid = true;
        }
        if (out_has_obstacles && !*out_has_obstacles && state->obstacle_mask &&
            state->obstacle_mask[i]) {
            *out_has_obstacles = true;
        }
        if ((!out_has_fluid || *out_has_fluid) &&
            (!out_has_obstacles || *out_has_obstacles)) {
            break;
        }
    }
    return true;
}

static const SimRuntimeBackendOps g_backend_2d_ops = {
    .destroy = backend_2d_destroy,
    .valid = backend_2d_valid,
    .clear = backend_2d_clear,
    .apply_brush_sample = backend_2d_apply_brush_sample,
    .build_static_obstacles = backend_2d_build_static_obstacles,
    .build_emitter_masks = backend_2d_build_emitter_masks,
    .mark_emitters_dirty = backend_2d_mark_emitters_dirty,
    .build_obstacles = backend_2d_build_obstacles,
    .mark_obstacles_dirty = backend_2d_mark_obstacles_dirty,
    .rasterize_dynamic_obstacles = backend_2d_rasterize_dynamic_obstacles,
    .apply_emitters = backend_2d_apply_emitters,
    .apply_boundary_flows = backend_2d_apply_boundary_flows,
    .enforce_boundary_flows = backend_2d_enforce_boundary_flows,
    .enforce_obstacles = backend_2d_enforce_obstacles,
    .step = backend_2d_step,
    .inject_object_motion = backend_2d_inject_object_motion,
    .reset_transient_state = backend_2d_reset_transient_state,
    .seed_uniform_velocity_2d = backend_2d_seed_uniform_velocity_2d,
    .export_snapshot = backend_2d_export_snapshot,
    .get_fluid_view_2d = backend_2d_get_fluid_view_2d,
    .get_obstacle_view_2d = backend_2d_get_obstacle_view_2d,
    .get_report = backend_2d_get_report,
    .get_compatibility_slice_activity = backend_2d_get_compatibility_slice_activity,
};

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend2D *state = NULL;
    size_t mask_count = 0;

    (void)preset;
    (void)mode_route;
    (void)runtime_visual;

    if (!cfg) return NULL;

    backend = (SimRuntimeBackend *)calloc(1, sizeof(*backend));
    state = (SimRuntimeBackend2D *)calloc(1, sizeof(*state));
    if (!backend || !state) {
        free(state);
        free(backend);
        return NULL;
    }

    state->fluid = fluid2d_create(cfg->grid_w, cfg->grid_h);
    if (!state->fluid) {
        free(state);
        free(backend);
        return NULL;
    }

    mask_count = (size_t)cfg->grid_w * (size_t)cfg->grid_h;
    if (mask_count > 0) {
        state->static_mask = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
        state->obstacle_mask = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
        state->obstacle_vel_x = (float *)calloc(mask_count, sizeof(float));
        state->obstacle_vel_y = (float *)calloc(mask_count, sizeof(float));
        state->obstacle_distance = (float *)calloc(mask_count, sizeof(float));
    }
    state->obstacle_mask_dirty = true;
    state->emitter_masks_dirty = true;
    state->wind_ramp_steps = 0;
    backend_2d_free_emitter_masks(state);

    backend->kind = SIM_RUNTIME_BACKEND_KIND_FLUID_2D;
    backend->impl = state;
    backend->ops = &g_backend_2d_ops;
    return backend;
}
