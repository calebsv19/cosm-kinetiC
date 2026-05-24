#include "app/sim_runtime_backend_2d_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "import/shape_import.h"
#include "render/import_project.h"

static const float EMITTER_POWER_BOOST = 40.0f;

static bool backend_2d_emitter_mask_bounds(const uint8_t *mask,
                                           int w,
                                           int h,
                                           int *out_min_x,
                                           int *out_max_x,
                                           int *out_min_y,
                                           int *out_max_y);

static void backend_2d_emitter_density_source_velocity(
    float vx_dir,
    float vy_dir,
    float scalar_strength,
    float *out_vx,
    float *out_vy) {
    float emitted_velocity_x [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        vx_dir * scalar_strength * 0.25f;
    float emitted_velocity_y [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        vy_dir * scalar_strength * 0.25f;
    if (out_vx) *out_vx = emitted_velocity_x;
    if (out_vy) *out_vy = emitted_velocity_y;
}

static void backend_2d_emitter_jet_velocity(float vx_dir,
                                            float vy_dir,
                                            float scalar_strength,
                                            float *out_vx,
                                            float *out_vy) {
    float emitted_velocity_x [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        vx_dir * scalar_strength;
    float emitted_velocity_y [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        vy_dir * scalar_strength;
    if (out_vx) *out_vx = emitted_velocity_x;
    if (out_vy) *out_vy = emitted_velocity_y;
}

static void backend_2d_emitter_sink_velocity(float vx_dir,
                                             float vy_dir,
                                             float scalar_strength,
                                             float *out_vx,
                                             float *out_vy) {
    float emitted_velocity_x [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        -vx_dir * scalar_strength * 0.4f;
    float emitted_velocity_y [[fisics::dim(velocity)]]
                             [[fisics::unit(meter_per_second)]] =
        -vy_dir * scalar_strength * 0.4f;
    if (out_vx) *out_vx = emitted_velocity_x;
    if (out_vy) *out_vy = emitted_velocity_y;
}

static void backend_2d_apply_obstacle_velocity_mask(SimRuntimeBackend2D *state,
                                                    const uint8_t *mask,
                                                    int min_x,
                                                    int max_x,
                                                    int min_y,
                                                    int max_y,
                                                    float velocity_x,
                                                    float velocity_y,
                                                    int w) {
    if (!state || !state->obstacle_mask || !mask || w <= 0) return;
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            if (!mask[id]) continue;
            state->obstacle_mask[id] = 1;
            if (state->obstacle_vel_x) state->obstacle_vel_x[id] = velocity_x;
            if (state->obstacle_vel_y) state->obstacle_vel_y[id] = velocity_y;
        }
    }
}

static void backend_2d_write_obstacle_velocity_cell(SimRuntimeBackend2D *state,
                                                    int x,
                                                    int y,
                                                    float velocity_x,
                                                    float velocity_y,
                                                    int w) {
    size_t id = 0;
    if (!state || !state->obstacle_mask || w <= 0) return;
    id = (size_t)y * (size_t)w + (size_t)x;
    state->obstacle_mask[id] = 1;
    if (state->obstacle_vel_x) state->obstacle_vel_x[id] = velocity_x;
    if (state->obstacle_vel_y) state->obstacle_vel_y[id] = velocity_y;
}

static bool backend_2d_rasterize_import_obstacle_velocity(SimRuntimeBackend2D *state,
                                                          const SceneState *scene,
                                                          const ImportedShape *imp,
                                                          float velocity_x,
                                                          float velocity_y,
                                                          int w,
                                                          int h) {
    size_t mask_count = 0;
    uint8_t *tmp = NULL;
    int min_x = 0;
    int max_x = -1;
    int min_y = 0;
    int max_y = -1;
    if (!state || !scene || !imp) return false;
    mask_count = (size_t)w * (size_t)h;
    tmp = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
    if (!tmp) return false;
    if (!backend_2d_rasterize_import_to_mask(scene, imp, tmp, mask_count) ||
        !backend_2d_emitter_mask_bounds(tmp, w, h, &min_x, &max_x, &min_y, &max_y)) {
        free(tmp);
        return false;
    }
    backend_2d_apply_mask_or(state->obstacle_mask, tmp, mask_count);
    backend_2d_apply_obstacle_velocity_mask(state,
                                            tmp,
                                            min_x,
                                            max_x,
                                            min_y,
                                            max_y,
                                            velocity_x,
                                            velocity_y,
                                            w);
    free(tmp);
    return true;
}

static bool backend_2d_emitter_mask_bounds(const uint8_t *mask,
                                           int w,
                                           int h,
                                           int *out_min_x,
                                           int *out_max_x,
                                           int *out_min_y,
                                           int *out_max_y);

void backend_2d_free_emitter_masks(SimRuntimeBackend2D *state) {
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

void backend_2d_build_emitter_masks(SimRuntimeBackend *backend,
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

            int min_x = 0;
            int min_y = 0;
            int max_x = -1;
            int max_y = -1;
            if (!backend_2d_emitter_mask_bounds(mask, w, h, &min_x, &max_x, &min_y, &max_y)) {
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

void backend_2d_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
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
                    ImportedShape imp_pose = *imp;
                    imp_pose.position_x = body->position.x / (float)scene->config->window_w;
                    imp_pose.position_y = body->position.y / (float)scene->config->window_h;
                    imp_pose.rotation_deg = body->angle * 180.0f / (float)M_PI;
                    rastered = backend_2d_rasterize_import_obstacle_velocity(state,
                                                                             scene,
                                                                             &imp_pose,
                                                                             vel_x,
                                                                             vel_y,
                                                                             w,
                                                                             h);
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
                            backend_2d_write_obstacle_velocity_cell(state, x, y, vel_x, vel_y, w);
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
                            backend_2d_write_obstacle_velocity_cell(state, x, y, vel_x, vel_y, w);
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
                                               float dt [[fisics::dim(time)]] [[fisics::unit(second)]],
                                               float area_scale) {
    float scale = backend_2d_emitter_strength_scale(scene, emitter->type);
    return emitter->strength * scale * EMITTER_POWER_BOOST * dt * area_scale;
}

static bool backend_2d_emitter_mask_bounds(const uint8_t *mask,
                                           int w,
                                           int h,
                                           int *out_min_x,
                                           int *out_max_x,
                                           int *out_min_y,
                                           int *out_max_y) {
    int min_x = w;
    int max_x = -1;
    int min_y = h;
    int max_y = -1;
    if (!mask || w <= 0 || h <= 0) return false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[(size_t)y * (size_t)w + (size_t)x]) continue;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (max_x < min_x || max_y < min_y) return false;
    if (out_min_x) *out_min_x = min_x;
    if (out_max_x) *out_max_x = max_x;
    if (out_min_y) *out_min_y = min_y;
    if (out_max_y) *out_max_y = max_y;
    return true;
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
                        {
                            float emitted_velocity_x = 0.0f;
                            float emitted_velocity_y = 0.0f;
                            backend_2d_emitter_density_source_velocity(vx_dir,
                                                                       vy_dir,
                                                                       per_cell,
                                                                       &emitted_velocity_x,
                                                                       &emitted_velocity_y);
                        fluid2d_add_density(state->fluid, x, y, per_cell);
                        fluid2d_add_velocity(state->fluid,
                                             x,
                                             y,
                                             emitted_velocity_x,
                                             emitted_velocity_y);
                        }
                        break;
                    case EMITTER_VELOCITY_JET:
                        {
                            float emitted_velocity_x = 0.0f;
                            float emitted_velocity_y = 0.0f;
                            backend_2d_emitter_jet_velocity(vx_dir,
                                                            vy_dir,
                                                            per_cell,
                                                            &emitted_velocity_x,
                                                            &emitted_velocity_y);
                            fluid2d_add_velocity(state->fluid,
                                                 x,
                                                 y,
                                                 emitted_velocity_x,
                                                 emitted_velocity_y);
                        }
                        fluid2d_add_density(state->fluid, x, y, per_cell * 0.3f);
                        break;
                    case EMITTER_SINK:
                        {
                            float emitted_velocity_x = 0.0f;
                            float emitted_velocity_y = 0.0f;
                            backend_2d_emitter_sink_velocity(vx_dir,
                                                             vy_dir,
                                                             per_cell,
                                                             &emitted_velocity_x,
                                                             &emitted_velocity_y);
                        fluid2d_add_density(state->fluid, x, y, -per_cell * 0.5f);
                        fluid2d_add_velocity(state->fluid,
                                             x,
                                             y,
                                             emitted_velocity_x,
                                             emitted_velocity_y);
                        }
                        break;
                    }
                }
            }
        }
    }
}

static void backend_2d_emitter_apply_import_fallback(SimRuntimeBackend2D *state,
                                                     const SceneState *scene,
                                                     const FluidEmitter *emitter,
                                                     const ImportedShape *fallback_imp,
                                                     float vx_dir,
                                                     float vy_dir,
                                                     float total_strength,
                                                     int w,
                                                     int h) {
    float span_x = 1.0f;
    float span_y = 1.0f;
    float grid_min_dim = (float)((w < h) ? w : h);
    float pos_x_unit = 0.0f;
    float pos_y_unit = 0.0f;
    int cx = 0;
    int cy = 0;
    float radius_cells = 10.0f;
    if (!state || !scene || !emitter || !fallback_imp) return;
    if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
    import_compute_span_from_window(scene->config->window_w,
                                    scene->config->window_h,
                                    &span_x,
                                    &span_y);
    pos_x_unit = backend_2d_import_pos_to_unit(fallback_imp->position_x, span_x);
    pos_y_unit = backend_2d_import_pos_to_unit(fallback_imp->position_y, span_y);
    cx = (int)lroundf(pos_x_unit * (float)(w - 1));
    cy = (int)lroundf(pos_y_unit * (float)(h - 1));
    if (scene->shape_library) {
        const ShapeAsset *asset =
            shape_lookup_from_path(scene->shape_library, fallback_imp->path);
        ShapeAssetBounds bounds;
        if (asset && shape_asset_bounds(asset, &bounds) && bounds.valid) {
            float max_dim = fmaxf(bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y);
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
        if (!tmp) return;
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

static void backend_2d_emitter_apply_attached_object(SimRuntimeBackend2D *state,
                                                     const FluidEmitter *emitter,
                                                     const PresetObject *obj,
                                                     float vx_dir,
                                                     float vy_dir,
                                                     float total_strength,
                                                     int w,
                                                     int h) {
    int cx = 0;
    int cy = 0;
    float cos_a = 0.0f;
    float sin_a = 0.0f;
    if (!state || !emitter || !obj) return;
    cx = (int)lroundf(obj->position_x * (float)(w - 1));
    cy = (int)lroundf(obj->position_y * (float)(h - 1));
    cos_a = cosf(obj->angle);
    sin_a = sinf(obj->angle);
    if (obj->type == PRESET_OBJECT_CIRCLE) {
        float radius_cells = obj->size_x * (float)w;
        int radius = (int)ceilf(radius_cells);
        if (radius < 1) radius = 1;
        {
            int min_x = (cx - radius - 1 < 1) ? 1 : cx - radius - 1;
            int max_x = (cx + radius + 1 > w - 2) ? w - 2 : cx + radius + 1;
            int min_y = (cy - radius - 1 < 1) ? 1 : cy - radius - 1;
            int max_y = (cy + radius + 1 > h - 2) ? h - 2 : cy + radius + 1;
            uint8_t *tmp = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
            if (!tmp) return;
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
        return;
    }
    {
        float half_width_cells = obj->size_x * (float)w;
        float half_height_cells = obj->size_y * (float)h;
        float bound_cells = fmaxf(half_width_cells, half_height_cells);
        int min_x = (int)fmaxf((float)cx - bound_cells - 1.0f, 1.0f);
        int max_x = (int)fminf((float)cx + bound_cells + 1.0f, (float)(w - 2));
        int min_y = (int)fmaxf((float)cy - bound_cells - 1.0f, 1.0f);
        int max_y = (int)fminf((float)cy + bound_cells + 1.0f, (float)(h - 2));
        uint8_t *tmp = (uint8_t *)calloc((size_t)w * (size_t)h, sizeof(uint8_t));
        if (!tmp) return;
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                float dx = (float)x - (float)cx;
                float dy = (float)y - (float)cy;
                float local_x = dx * cos_a + dy * sin_a;
                float local_y = -dx * sin_a + dy * cos_a;
                if (fabsf(local_x) <= half_width_cells && fabsf(local_y) <= half_height_cells) {
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

void backend_2d_apply_emitters(SimRuntimeBackend *backend,
                               SceneState *scene,
                               double dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    SimRuntimeBackend2D *state = backend_2d_state(backend);
    float dt_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = (float)dt;
    float zero_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0f;
    if (!scene || !scene->preset || !state || !state->fluid) return;
    if (!scene->emitters_enabled) return;
    if (dt_seconds <= zero_seconds) return;
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
                        backend_2d_emitter_total_strength(scene, emitter, dt_seconds, area_scale);
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
                            backend_2d_emitter_apply_import_fallback(state,
                                                                     scene,
                                                                     emitter,
                                                                     fallback_imp,
                                                                     vx_dir,
                                                                     vy_dir,
                                                                     total_strength,
                                                                     w,
                                                                     h);
                        }
                        continue;
                    }

                    if (attached_obj >= 0 && attached_obj < (int)scene->preset->object_count) {
                        const PresetObject *obj = &scene->preset->objects[attached_obj];
                        backend_2d_emitter_apply_attached_object(state,
                                                                 emitter,
                                                                 obj,
                                                                 vx_dir,
                                                                 vy_dir,
                                                                 total_strength,
                                                                 w,
                                                                 h);
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
                                                {
                                                    float emitted_velocity_x = 0.0f;
                                                    float emitted_velocity_y = 0.0f;
                                                    backend_2d_emitter_density_source_velocity(
                                                        vx_dir,
                                                        vy_dir,
                                                        fall_scaled,
                                                        &emitted_velocity_x,
                                                        &emitted_velocity_y);
                                                fluid2d_add_density(state->fluid, x, y, fall_scaled);
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     emitted_velocity_x,
                                                                     emitted_velocity_y);
                                                }
                                                break;
                                            case EMITTER_VELOCITY_JET:
                                                {
                                                    float emitted_velocity_x = 0.0f;
                                                    float emitted_velocity_y = 0.0f;
                                                    backend_2d_emitter_jet_velocity(
                                                        vx_dir,
                                                        vy_dir,
                                                        fall_scaled,
                                                        &emitted_velocity_x,
                                                        &emitted_velocity_y);
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     emitted_velocity_x,
                                                                     emitted_velocity_y);
                                                }
                                                fluid2d_add_density(state->fluid, x, y, fall_scaled * 0.3f);
                                                break;
                                            case EMITTER_SINK:
                                                {
                                                    float emitted_velocity_x = 0.0f;
                                                    float emitted_velocity_y = 0.0f;
                                                    backend_2d_emitter_sink_velocity(
                                                        vx_dir,
                                                        vy_dir,
                                                        fall_scaled,
                                                        &emitted_velocity_x,
                                                        &emitted_velocity_y);
                                                fluid2d_add_density(state->fluid, x, y, -fall_scaled * 0.5f);
                                                fluid2d_add_velocity(state->fluid,
                                                                     x,
                                                                     y,
                                                                     emitted_velocity_x,
                                                                     emitted_velocity_y);
                                                }
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
