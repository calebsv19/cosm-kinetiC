#include "app/scene_apply.h"

#include <math.h>

#include "physics/fluid2d/fluid2d_boundary.h"
#include "app/scene_masks.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "render/import_project.h"

static const float EMITTER_POWER_BOOST = 40.0f;

static inline void emitter_normalize_dir(float *x, float *y) {
    float len = sqrtf((*x) * (*x) + (*y) * (*y));
    if (len > 0.0001f) {
        *x /= len;
        *y /= len;
    } else {
        *x = 0.0f;
        *y = -1.0f;
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

static float emitter_strength_scale(const SceneState *scene,
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

static float emitter_total_strength(const SceneState *scene,
                                    const FluidEmitter *em,
                                    float dt,
                                    float area_scale) {
    float scale = emitter_strength_scale(scene, em->type);
    float total = em->strength * scale * EMITTER_POWER_BOOST * dt * area_scale;
    return total;
}

static void emitter_apply_mask(const SceneState *scene,
                               const FluidEmitter *em,
                               const uint8_t *mask,
                               int min_x, int max_x,
                               int min_y, int max_y,
                               float vx_dir,
                               float vy_dir,
                               float total_strength,
                               int w,
                               int h) {
    if (!scene || !scene->smoke || !mask) return;
    if (w <= 1 || h <= 1) return;
    if (min_x < 1) min_x = 1;
    if (max_x > w - 2) max_x = w - 2;
    if (min_y < 1) min_y = 1;
    if (max_y > h - 2) max_y = h - 2;
    size_t cells = 0;
    for (int y = min_y; y <= max_y; ++y) {
        size_t row = (size_t)y * (size_t)w;
        for (int x = min_x; x <= max_x; ++x) {
            if (mask[row + (size_t)x]) ++cells;
        }
    }
    float per_cell = (cells > 0) ? total_strength / (float)cells : 0.0f;
    if (per_cell <= 0.0f) return;

    for (int y = min_y; y <= max_y; ++y) {
        size_t row = (size_t)y * (size_t)w;
        for (int x = min_x; x <= max_x; ++x) {
            if (!mask[row + (size_t)x]) continue;
            switch (em->type) {
            case EMITTER_DENSITY_SOURCE:
                fluid2d_add_density(scene->smoke, x, y, per_cell);
                fluid2d_add_velocity(scene->smoke, x, y,
                                     vx_dir * per_cell * 0.25f,
                                     vy_dir * per_cell * 0.25f);
                break;
            case EMITTER_VELOCITY_JET:
                fluid2d_add_velocity(scene->smoke, x, y,
                                     vx_dir * per_cell,
                                     vy_dir * per_cell);
                fluid2d_add_density(scene->smoke, x, y, per_cell * 0.3f);
                break;
            case EMITTER_SINK:
                fluid2d_add_density(scene->smoke, x, y, -per_cell * 0.5f);
                fluid2d_add_velocity(scene->smoke, x, y,
                                     -vx_dir * per_cell * 0.4f,
                                     -vy_dir * per_cell * 0.4f);
                break;
            }
        }
    }
}

void scene_apply_emitters(SceneState *scene, double dt) {
    if (!scene || !scene->preset || !scene->smoke) return;
    if (!scene->emitters_enabled) return;
    if (scene->emitter_masks_dirty) {
        scene_masks_build_emitter(scene);
    }
    const FluidScenePreset *preset = scene->preset;
    int w = scene->smoke->w;
    int h = scene->smoke->h;
    const float REF_GRID_AREA = 256.0f * 256.0f;
    float grid_area = (float)w * (float)h;
    if (grid_area < 1.0f) grid_area = 1.0f;
    float area_scale = grid_area / REF_GRID_AREA;
    if (area_scale < 0.25f) area_scale = 0.25f;
    if (area_scale > 4.0f) area_scale = 4.0f;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        float vx_dir = em->dir_x;
        float vy_dir = em->dir_y;
        emitter_normalize_dir(&vx_dir, &vy_dir);
        float total_strength = emitter_total_strength(scene, em, (float)dt, area_scale);

        int attached_obj = em->attached_object;
        int attached_imp = em->attached_import;

        if (attached_imp >= 0 && attached_imp < (int)scene->import_shape_count) {
            const ImportedShape *imp = &scene->import_shapes[attached_imp];
            if (imp) {
                // Rotate direction by import rotation so handle can be stored in local space.
                float rad = imp->rotation_deg * (float)M_PI / 180.0f;
                float c = cosf(rad), s = sinf(rad);
                float rx = vx_dir * c - vy_dir * s;
                float ry = vx_dir * s + vy_dir * c;
                vx_dir = rx;
                vy_dir = ry;
                emitter_normalize_dir(&vx_dir, &vy_dir);
            }
            bool used_mask = false;
            if (scene->emitter_masks[i].mask &&
                scene->emitter_masks[i].max_x >= scene->emitter_masks[i].min_x &&
                scene->emitter_masks[i].max_y >= scene->emitter_masks[i].min_y) {
                // Use prebuilt mask for attached import.
                const uint8_t *mask = scene->emitter_masks[i].mask;
                int min_x = scene->emitter_masks[i].min_x;
                int max_x = scene->emitter_masks[i].max_x;
                int min_y = scene->emitter_masks[i].min_y;
                int max_y = scene->emitter_masks[i].max_y;
                if (min_x < 1) min_x = 1;
                if (max_x > w - 2) max_x = w - 2;
                if (min_y < 1) min_y = 1;
                if (max_y > h - 2) max_y = h - 2;
                emitter_apply_mask(scene, em, mask, min_x, max_x, min_y, max_y, vx_dir, vy_dir, total_strength, w, h);
                used_mask = true;
            }
            if (!used_mask) {
                // Fallback: approximate emitter footprint around the import center.
                const ImportedShape *imp = &scene->import_shapes[attached_imp];
                float span_x = 1.0f, span_y = 1.0f;
                import_compute_span_from_window(scene->config->window_w,
                                                scene->config->window_h,
                                                &span_x, &span_y);
                float grid_min_dim = (float)((w < h) ? w : h);
                if (grid_min_dim <= 0.0f) grid_min_dim = 1.0f;
                float pos_x_unit = import_pos_to_unit(imp->position_x, span_x);
                float pos_y_unit = import_pos_to_unit(imp->position_y, span_y);
                int cx = (int)lroundf(pos_x_unit * (float)(w - 1));
                int cy = (int)lroundf(pos_y_unit * (float)(h - 1));

                float radius_cells = 10.0f;
                if (scene->shape_library) {
                    const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
                    ShapeAssetBounds b;
                    if (asset && shape_asset_bounds(asset, &b) && b.valid) {
                        float max_dim = fmaxf(b.max_x - b.min_x, b.max_y - b.min_y);
                        if (max_dim > 0.0001f) {
                            const float desired_fit = 0.25f;
                            float norm = (imp->scale * desired_fit) / max_dim;
                            radius_cells = norm * grid_min_dim;
                        }
                    }
                }
                if (radius_cells < 1.0f) radius_cells = 1.0f;
                int radius = (int)ceilf(radius_cells);
                int min_x = (cx - radius < 1) ? 1 : cx - radius;
                int max_x = (cx + radius > w - 2) ? w - 2 : cx + radius;
                int min_y = (cy - radius < 1) ? 1 : cy - radius;
                int max_y = (cy + radius > h - 2) ? h - 2 : cy + radius;
                size_t cells = 0;
                for (int y = min_y; y <= max_y; ++y) {
                    for (int x = min_x; x <= max_x; ++x) {
                        float dx = (float)x - (float)cx;
                        float dy = (float)y - (float)cy;
                        if ((dx * dx + dy * dy) <= (float)(radius * radius)) {
                            ++cells;
                        }
                    }
                }
                if (cells > 0) {
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
                        emitter_apply_mask(scene, em, tmp, min_x, max_x, min_y, max_y, vx_dir, vy_dir, total_strength, w, h);
                        free(tmp);
                    }
                }
            }
            continue;
        }

        if (attached_obj >= 0 && attached_obj < (int)preset->object_count) {
            const PresetObject *obj = &preset->objects[attached_obj];
            int cx = (int)lroundf(obj->position_x * (float)(w - 1));
            int cy = (int)lroundf(obj->position_y * (float)(h - 1));
            float cos_a = cosf(obj->angle);
            float sin_a = sinf(obj->angle);
            if (obj->type == PRESET_OBJECT_CIRCLE) {
                int radius = (int)ceilf(obj->size_x * (float)w);
                if (radius < 1) radius = 1;
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
                    emitter_apply_mask(scene, em, tmp, min_x, max_x, min_y, max_y, vx_dir, vy_dir, total_strength, w, h);
                    free(tmp);
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
                    emitter_apply_mask(scene, em, tmp, min_x, max_x, min_y, max_y, vx_dir, vy_dir, total_strength, w, h);
                    free(tmp);
                }
            }
            continue;
        }

        // Fallback: free emitter behaves as a circle (legacy behavior).
        int cx = (int)(em->position_x * (float)(w - 1));
        int cy = (int)(em->position_y * (float)(h - 1));
        int radius = (int)(em->radius * (float)w);
        if (radius < 1) radius = 1;

        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (x < 1 || x >= w - 1 || y < 1 || y >= h - 1) continue;
                float dx = (float)x - (float)cx;
                float dy = (float)y - (float)cy;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)radius) continue;
                float falloff = 1.0f - (dist / (float)radius);
                if (falloff <= 0.0f) continue;
                float fall_scaled = total_strength * falloff;

                switch (em->type) {
                case EMITTER_DENSITY_SOURCE:
                    fluid2d_add_density(scene->smoke, x, y, fall_scaled);
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         vx_dir * fall_scaled * 0.25f,
                                         vy_dir * fall_scaled * 0.25f);
                    break;
                case EMITTER_VELOCITY_JET:
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         vx_dir * fall_scaled,
                                         vy_dir * fall_scaled);
                    fluid2d_add_density(scene->smoke, x, y, fall_scaled * 0.3f);
                    break;
                case EMITTER_SINK:
                    fluid2d_add_density(scene->smoke, x, y, -fall_scaled * 0.5f);
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         -vx_dir * fall_scaled * 0.4f,
                                         -vy_dir * fall_scaled * 0.4f);
                    break;
                }
            }
        }
    }
}

void scene_apply_boundary_flows(SceneState *scene, double dt) {
    if (!scene || !scene->preset || !scene->smoke) return;
    if (scene->config && scene->config->sim_mode == SIM_MODE_WIND_TUNNEL) {
        static const int WIND_RAMP_STEPS = 200;
        float ramp = 1.0f;
        if (scene->wind_ramp_steps < WIND_RAMP_STEPS) {
            ramp = (float)scene->wind_ramp_steps / (float)WIND_RAMP_STEPS;
            if (ramp > 1.0f) ramp = 1.0f;
        }
        fluid2d_boundary_apply_wind(scene->config, scene->preset, scene->smoke, dt, ramp);
        if (scene->wind_ramp_steps < WIND_RAMP_STEPS) {
            scene->wind_ramp_steps++;
        }
    } else {
        fluid2d_boundary_apply(scene->preset->boundary_flows, scene->smoke, dt);
    }
}

void scene_enforce_boundary_flows(SceneState *scene) {
    if (!scene || !scene->preset || !scene->smoke) return;
    if (scene->config && scene->config->sim_mode == SIM_MODE_WIND_TUNNEL) {
        fluid2d_boundary_enforce_wind(scene->config, scene->preset, scene->smoke);
    } else {
        fluid2d_boundary_enforce(scene->preset->boundary_flows, scene->smoke);
    }
}

void scene_enforce_obstacles(SceneState *scene) {
    if (!scene || !scene->smoke) return;
    scene_masks_rasterize_dynamic(scene);
    fluid2d_enforce_solid_mask(scene->smoke,
                               scene->obstacle_mask,
                               scene->obstacle_velX,
                               scene->obstacle_velY);
}
