#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_2d_internal.h"

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

static const float BRUSH_DENSITY = 20.0f;
static const float BRUSH_VEL_SCALE = 35.0f;
static const float BRUSH_VELOCITY_DENSITY = 4.0f;

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

float backend_2d_import_pos_to_unit(float pos, float span) {
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

void backend_2d_apply_mask_or(uint8_t *dst, const uint8_t *src, size_t count) {
    if (!dst || !src) return;
    for (size_t i = 0; i < count; ++i) {
        if (src[i]) dst[i] = 1;
    }
}

bool backend_2d_rasterize_import_to_mask(const SceneState *scene,
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

static void backend_2d_mark_emitters_dirty(SimRuntimeBackend *backend) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    if (state) {
        state->emitter_masks_dirty = true;
    }
}

void backend_2d_compute_obstacle_distance(const SceneState *scene,
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

static bool backend_2d_get_debug_volume_view_3d(const SimRuntimeBackend *backend,
                                                SceneDebugVolumeView3D *out_view) {
    (void)backend;
    (void)out_view;
    return false;
}

static bool backend_2d_get_volume_export_view_3d(const SimRuntimeBackend *backend,
                                                 SceneFluidVolumeExportView3D *out_view) {
    (void)backend;
    (void)out_view;
    return false;
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
        .scene_up_valid = false,
        .scene_up_source = PHYSICS_SIM_RUNTIME_SCENE_UP_NONE,
        .compatibility_view_2d_available = true,
        .compatibility_view_2d_derived = false,
        .compatibility_slice_z = 0,
        .secondary_debug_slice_stack_live = false,
        .secondary_debug_slice_stack_radius = 0,
        .debug_volume_view_3d_available = false,
        .debug_volume_active_density_cells = 0,
        .debug_volume_solid_cells = 0,
        .debug_volume_max_density = 0.0f,
        .debug_volume_max_velocity_magnitude = 0.0f,
        .debug_volume_scene_up_velocity_valid = false,
        .debug_volume_scene_up_velocity_avg = 0.0f,
        .debug_volume_scene_up_velocity_peak = 0.0f,
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
    .get_debug_volume_view_3d = backend_2d_get_debug_volume_view_3d,
    .get_volume_export_view_3d = backend_2d_get_volume_export_view_3d,
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
