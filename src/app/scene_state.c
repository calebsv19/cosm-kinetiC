#include "app/scene_state.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "physics/fluid2d/fluid2d_boundary.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"
#include "physics/objects/physics_object_builder.h"
#include "app/shape_lookup.h"
#include "render/import_project.h"
#include "app/scene_imports.h"
#include "app/scene_masks.h"
#include "app/scene_objects.h"
#include "app/scene_apply.h"

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

SceneState scene_create(const AppConfig *cfg,
                        const FluidScenePreset *preset,
                        const ShapeAssetLibrary *shape_library) {
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

    s.import_shape_count = 0;
    memset(s.import_shapes, 0, sizeof(s.import_shapes));
    memset(s.import_start_pos_x, 0, sizeof(s.import_start_pos_x));
    memset(s.import_start_pos_y, 0, sizeof(s.import_start_pos_y));
    memset(s.import_start_rot_deg, 0, sizeof(s.import_start_rot_deg));
    s.emitter_masks_dirty = true;
    for (size_t i = 0; i < MAX_FLUID_EMITTERS; ++i) {
        s.emitter_masks[i].mask = NULL;
        s.emitter_masks[i].min_x = s.emitter_masks[i].min_y = 0;
        s.emitter_masks[i].max_x = s.emitter_masks[i].max_y = -1;
    }

    scene_objects_init(&s);
    s.shape_library = shape_library;
    if (preset) {
        // Copy imports
        size_t copy_count = (preset->import_shape_count < MAX_IMPORTED_SHAPES)
                                ? preset->import_shape_count
                                : MAX_IMPORTED_SHAPES;
        for (size_t i = 0; i < copy_count; ++i) {
            s.import_shapes[s.import_shape_count] = preset->import_shapes[i];
            s.import_start_pos_x[s.import_shape_count] = preset->import_shapes[i].position_x;
            s.import_start_pos_y[s.import_shape_count] = preset->import_shapes[i].position_y;
            s.import_start_rot_deg[s.import_shape_count] = preset->import_shapes[i].rotation_deg;
            s.import_shape_count++;
        }
        scene_imports_resolve(&s);
        scene_imports_rebuild_bodies(&s);
        scene_objects_add_presets(&s);
        scene_masks_build_static(&s);
        scene_masks_build_emitter(&s);
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
    scene_masks_free_emitter_masks(scene);
    scene_objects_shutdown(scene);
}

static const float BRUSH_DENSITY = 20.0f;
static const float BRUSH_VEL_SCALE = 35.0f;
static const float BRUSH_VELOCITY_DENSITY = 4.0f;

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
    case COMMAND_TOGGLE_OBJECT_GRAVITY:
        scene_objects_reset_gravity(scene);
        return true;
    case COMMAND_TOGGLE_ELASTIC_COLLISIONS:
        scene_objects_set_elastic(scene, !scene->objects_elastic);
        return true;
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


void scene_set_emitters_enabled(SceneState *scene, bool enabled) {
    if (!scene) return;
    scene->emitters_enabled = enabled;
}

void scene_rasterize_dynamic_obstacles(SceneState *scene) {
    scene_masks_rasterize_dynamic(scene);
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
