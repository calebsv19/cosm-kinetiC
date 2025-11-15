#include "app/scene_state.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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

SceneState scene_create(const AppConfig *cfg, const FluidScenePreset *preset) {
    SceneState s;
    s.time   = 0.0;
    s.dt     = 0.0;
    s.paused = false;
    s.config = cfg;
    s.preset = preset;

    s.smoke = fluid2d_create(cfg->grid_w, cfg->grid_h);
    if (!s.smoke) {
        fprintf(stderr, "Failed to create Fluid2D\n");
    }

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
        }
    }
    return s;
}

void scene_destroy(SceneState *scene) {
    if (!scene) return;
    fluid2d_destroy(scene->smoke);
    scene->smoke = NULL;
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
