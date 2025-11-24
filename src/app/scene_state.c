#include "app/scene_state.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "physics/fluid2d/fluid2d_boundary.h"

// simple mapping from window coords to grid coords
static void window_to_grid(const SceneState *scene, int win_x, int win_y,
                           int *out_gx, int *out_gy) {
    const AppConfig *cfg = scene->config;
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

static inline int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int world_to_cell(float world_value, float world_max, int grid_res) {
    if (grid_res <= 1 || world_max <= 0.0f) return 0;
    float norm = world_value / world_max;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (int)lroundf(norm * (float)(grid_res - 1));
}

static float world_velocity_to_grid(const SceneState *scene, float v, bool axis_x) {
    if (!scene || !scene->config) return 0.0f;
    int grid = axis_x ? scene->config->grid_w : scene->config->grid_h;
    float world_max = axis_x ? (float)scene->config->window_w
                             : (float)scene->config->window_h;
    if (grid <= 1 || world_max <= 0.0f) return 0.0f;
    return v * (float)(grid - 1) / world_max;
}

static void obstacle_mask_mark_cell(SceneState *scene,
                                    int gx,
                                    int gy,
                                    float vel_x,
                                    float vel_y) {
    if (!scene || !scene->obstacle_mask || !scene->config) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (gx < 1 || gx >= w - 1 || gy < 1 || gy >= h - 1) return;
    size_t id = (size_t)gy * (size_t)w + (size_t)gx;
    scene->obstacle_mask[id] = 1;
    if (scene->obstacle_velX) {
        scene->obstacle_velX[id] = vel_x;
    }
    if (scene->obstacle_velY) {
        scene->obstacle_velY[id] = vel_y;
    }
}

static void obstacle_mask_apply_circle(SceneState *scene,
                                       const SceneObject *obj) {
    if (!scene || !scene->config || !obj) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    float vel_x = obj->body.is_static
                      ? 0.0f
                      : world_velocity_to_grid(scene, obj->body.velocity.x, true);
    float vel_y = obj->body.is_static
                      ? 0.0f
                      : world_velocity_to_grid(scene, obj->body.velocity.y, false);

    int cx = world_to_cell(obj->body.position.x,
                           (float)scene->config->window_w, w);
    int cy = world_to_cell(obj->body.position.y,
                           (float)scene->config->window_h, h);
    float grid_radius_x = obj->body.radius /
                          (float)scene->config->window_w * (float)(w - 1);
    float grid_radius_y = obj->body.radius /
                          (float)scene->config->window_h * (float)(h - 1);
    int radius = (int)ceilf(fmaxf(grid_radius_x, grid_radius_y));
    if (radius < 1) radius = 1;

    for (int y = cy - radius; y <= cy + radius; ++y) {
        if (y <= 0 || y >= h - 1) continue;
        for (int x = cx - radius; x <= cx + radius; ++x) {
            if (x <= 0 || x >= w - 1) continue;
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float nx = dx / fmaxf(grid_radius_x, 1.0f);
            float ny = dy / fmaxf(grid_radius_y, 1.0f);
            if (nx * nx + ny * ny <= 1.0f) {
                obstacle_mask_mark_cell(scene, x, y, vel_x, vel_y);
            }
        }
    }
}

static void obstacle_mask_apply_box(SceneState *scene,
                                    const SceneObject *obj) {
    if (!scene || !scene->config || !obj) return;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 1 || h <= 1) return;

    float vel_x = obj->body.is_static
                      ? 0.0f
                      : world_velocity_to_grid(scene, obj->body.velocity.x, true);
    float vel_y = obj->body.is_static
                      ? 0.0f
                      : world_velocity_to_grid(scene, obj->body.velocity.y, false);

    float half_diag = sqrtf(obj->body.half_extents.x * obj->body.half_extents.x +
                            obj->body.half_extents.y * obj->body.half_extents.y);
    float min_w = obj->body.position.x - half_diag;
    float max_w = obj->body.position.x + half_diag;
    float min_h = obj->body.position.y - half_diag;
    float max_h = obj->body.position.y + half_diag;

    int min_x = world_to_cell(min_w, (float)scene->config->window_w, w);
    int max_x = world_to_cell(max_w, (float)scene->config->window_w, w);
    int min_y = world_to_cell(min_h, (float)scene->config->window_h, h);
    int max_y = world_to_cell(max_h, (float)scene->config->window_h, h);

    min_x = clamp_int(min_x, 1, w - 2);
    max_x = clamp_int(max_x, 1, w - 2);
    min_y = clamp_int(min_y, 1, h - 2);
    max_y = clamp_int(max_y, 1, h - 2);

    float cos_a = cosf(obj->body.angle);
    float sin_a = sinf(obj->body.angle);
    float half_w_world = obj->body.half_extents.x;
    float half_h_world = obj->body.half_extents.y;

    for (int y = min_y; y <= max_y; ++y) {
        float world_y = ((float)y / (float)(h - 1)) * (float)scene->config->window_h;
        for (int x = min_x; x <= max_x; ++x) {
            float world_x = ((float)x / (float)(w - 1)) * (float)scene->config->window_w;
            float rel_x = world_x - obj->body.position.x;
            float rel_y = world_y - obj->body.position.y;
            float local_x =  rel_x * cos_a + rel_y * sin_a;
            float local_y = -rel_x * sin_a + rel_y * cos_a;
            if (fabsf(local_x) <= half_w_world &&
                fabsf(local_y) <= half_h_world) {
                obstacle_mask_mark_cell(scene, x, y, vel_x, vel_y);
            }
        }
    }
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

static void scene_build_obstacle_mask(SceneState *scene) {
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

    for (int i = 0; i < scene->objects.count; ++i) {
        const SceneObject *obj = &scene->objects.objects[i];
        if (!obj) continue;
        if (obj->type == SCENE_OBJECT_CIRCLE) {
            obstacle_mask_apply_circle(scene, obj);
        } else {
            obstacle_mask_apply_box(scene, obj);
        }
    }
    scene_compute_obstacle_distance(scene);
    scene->obstacle_mask_dirty = false;
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
    if (scene->obstacle_mask_dirty) {
        scene_build_obstacle_mask(scene);
    }
    fluid2d_enforce_solid_mask(scene->smoke,
                               scene->obstacle_mask,
                               scene->obstacle_velX,
                               scene->obstacle_velY);
}

SceneState scene_create(const AppConfig *cfg, const FluidScenePreset *preset) {
    SceneState s;
    s.time   = 0.0;
    s.dt     = 0.0;
    s.paused = false;
    s.emitters_enabled = true;
    s.config = cfg;
    s.preset = preset;
    s.wind_ramp_steps = 0;

    s.smoke = fluid2d_create(cfg->grid_w, cfg->grid_h);
    if (!s.smoke) {
        fprintf(stderr, "Failed to create Fluid2D\n");
    }

    size_t mask_count = (size_t)cfg->grid_w * (size_t)cfg->grid_h;
    s.static_mask = (mask_count > 0)
                        ? (uint8_t *)calloc(mask_count, sizeof(uint8_t))
                        : NULL;
    s.obstacle_mask = (mask_count > 0)
                        ? (uint8_t *)calloc(mask_count, sizeof(uint8_t))
                        : NULL;
    s.obstacle_velX = (mask_count > 0)
                        ? (float *)calloc(mask_count, sizeof(float))
                        : NULL;
    s.obstacle_velY = (mask_count > 0)
                        ? (float *)calloc(mask_count, sizeof(float))
                        : NULL;
    s.obstacle_distance = (mask_count > 0)
                        ? (float *)calloc(mask_count, sizeof(float))
                        : NULL;
    s.obstacle_mask_dirty = true;

    object_manager_init(&s.objects, 8);
    if (preset) {
        for (size_t i = 0; i < preset->object_count && i < MAX_PRESET_OBJECTS; ++i) {
            const PresetObject *po = &preset->objects[i];
            Vec2 position = vec2(po->position_x * (float)cfg->window_w,
                                 po->position_y * (float)cfg->window_h);
            if (po->type == PRESET_OBJECT_CIRCLE) {
                float radius = po->size_x * (float)cfg->window_w;
                SceneObject *obj = object_manager_add_circle(&s.objects,
                                                             position,
                                                             radius,
                                                             po->is_static);
                if (obj) {
                    obj->body.angle = po->angle;
                }
            } else {
                Vec2 half_extents = vec2(po->size_x * (float)cfg->window_w,
                                         po->size_y * (float)cfg->window_h);
                SceneObject *obj = object_manager_add_box(&s.objects,
                                                          position,
                                                          half_extents,
                                                          po->is_static);
                if (obj) {
                    obj->body.angle = po->angle;
                }
            }
            static_mask_apply_preset(&s, po);
        }
    }
    return s;
}

void scene_destroy(SceneState *scene) {
    if (!scene) return;
    fluid2d_destroy(scene->smoke);
    scene->smoke = NULL;
    free(scene->static_mask);
    scene->static_mask = NULL;
    free(scene->obstacle_mask);
    scene->obstacle_mask = NULL;
    free(scene->obstacle_velX);
    scene->obstacle_velX = NULL;
    free(scene->obstacle_velY);
    scene->obstacle_velY = NULL;
    object_manager_shutdown(&scene->objects);
}

static const float BRUSH_DENSITY = 20.0f;
static const float BRUSH_VEL_SCALE = 35.0f;
static const float BRUSH_VELOCITY_DENSITY = 4.0f;

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

void scene_apply_input(SceneState *scene, const InputCommands *cmds) {
    if (!scene || !cmds) return;
    (void)cmds;
}

bool scene_handle_command(SceneState *scene, const Command *cmd) {
    if (!scene || !cmd) return false;

    switch (cmd->type) {
    case COMMAND_TOGGLE_PAUSE:
        scene->paused = !scene->paused;
        return true;
    case COMMAND_CLEAR_SMOKE:
        if (scene->smoke) {
            fluid2d_clear(scene->smoke);
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool scene_apply_brush_sample(SceneState *scene, const StrokeSample *sample) {
    if (!scene || !scene->smoke || !sample) return false;

    int gx, gy;
    window_to_grid(scene, sample->x, sample->y, &gx, &gy);

    float inv_w = (float)(scene->config->window_w > 0 ? scene->config->window_w : 1);
    float inv_h = (float)(scene->config->window_h > 0 ? scene->config->window_h : 1);
    float vx = (sample->vx / inv_w) * BRUSH_VEL_SCALE;
    float vy = (sample->vy / inv_h) * BRUSH_VEL_SCALE;

    switch (sample->mode) {
    case BRUSH_MODE_VELOCITY:
        fluid2d_add_velocity(scene->smoke, gx, gy, vx, vy);
        fluid2d_add_density(scene->smoke, gx, gy, BRUSH_VELOCITY_DENSITY);
        break;
    case BRUSH_MODE_DENSITY:
    default:
        fluid2d_add_density(scene->smoke, gx, gy, BRUSH_DENSITY);
        fluid2d_add_velocity(scene->smoke, gx, gy, vx * 0.25f, vy * 0.25f);
        break;
    }

    return true;
}

void scene_apply_emitters(SceneState *scene, double dt) {
    if (!scene || !scene->preset || !scene->smoke) return;
    if (!scene->emitters_enabled) return;
    const FluidScenePreset *preset = scene->preset;
    int w = scene->smoke->w;
    int h = scene->smoke->h;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int cx = (int)(em->position_x * (float)(w - 1));
        int cy = (int)(em->position_y * (float)(h - 1));
        int radius = (int)(em->radius * (float)w);
        if (radius < 1) radius = 1;

        float vx_dir = em->dir_x;
        float vy_dir = em->dir_y;
        float len = sqrtf(vx_dir * vx_dir + vy_dir * vy_dir);
        if (len > 0.0001f) {
            vx_dir /= len;
            vy_dir /= len;
        }

        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (x < 1 || x >= w - 1 || y < 1 || y >= h - 1) continue;
                float dx = (float)x - (float)cx;
                float dy = (float)y - (float)cy;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)radius) continue;
                float falloff = 1.0f - (dist / (float)radius);
                if (falloff <= 0.0f) continue;

                float scale = emitter_strength_scale(scene, em->type);
                float scaled = em->strength * scale * falloff * (float)dt;

                switch (em->type) {
                case EMITTER_DENSITY_SOURCE:
                    fluid2d_add_density(scene->smoke, x, y, scaled);
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         vx_dir * scaled * 0.25f,
                                         vy_dir * scaled * 0.25f);
                    break;
                case EMITTER_VELOCITY_JET:
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         vx_dir * scaled,
                                         vy_dir * scaled);
                    fluid2d_add_density(scene->smoke, x, y, scaled * 0.3f);
                    break;
                case EMITTER_SINK:
                    fluid2d_add_density(scene->smoke, x, y, -scaled * 0.5f);
                    fluid2d_add_velocity(scene->smoke, x, y,
                                         -vx_dir * scaled * 0.4f,
                                         -vy_dir * scaled * 0.4f);
                    break;
                }
            }
        }
    }
}

void scene_set_emitters_enabled(SceneState *scene, bool enabled) {
    if (!scene) return;
    scene->emitters_enabled = enabled;
}

// --- snapshot format ---
//
// uint32 magic  = 'PS2D'
// uint32 version = 1
// uint32 gridW
// uint32 gridH
// double time
// float density[gridW * gridH]
// float velX[gridW * gridH]
// float velY[gridW * gridH]

bool scene_export_snapshot(const SceneState *scene, const char *path) {
    if (!scene || !scene->smoke || !path) return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen snapshot");
        return false;
    }

    uint32_t magic   = ('P' << 24) | ('S' << 16) | ('2' << 8) | ('D');
    uint32_t version = 1;
    uint32_t gridW   = (uint32_t)scene->smoke->w;
    uint32_t gridH   = (uint32_t)scene->smoke->h;

    if (fwrite(&magic,   sizeof(magic),   1, f) != 1 ||
        fwrite(&version, sizeof(version), 1, f) != 1 ||
        fwrite(&gridW,   sizeof(gridW),   1, f) != 1 ||
        fwrite(&gridH,   sizeof(gridH),   1, f) != 1 ||
        fwrite(&scene->time, sizeof(scene->time), 1, f) != 1) {
        fclose(f);
        return false;
    }

    size_t count = (size_t)gridW * (size_t)gridH;
    if (fwrite(scene->smoke->density, sizeof(float), count, f) != count ||
        fwrite(scene->smoke->velX,    sizeof(float), count, f) != count ||
        fwrite(scene->smoke->velY,    sizeof(float), count, f) != count) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}
#include <stdint.h>
