#include "app/sim_runtime_backend_2d_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "import/shape_import.h"
#include "physics/fluid2d/fluid2d.h"
#include "physics/objects/physics_object_builder.h"

const ShapeAsset *shape_lookup_from_path(const ShapeAssetLibrary *lib,
                                         const char *path_or_name) {
    (void)lib;
    (void)path_or_name;
    return NULL;
}

bool import_compute_span_from_window(int cfg_w, int cfg_h, float *out_span_x, float *out_span_y) {
    (void)cfg_w;
    (void)cfg_h;
    if (out_span_x) *out_span_x = 1.0f;
    if (out_span_y) *out_span_y = 1.0f;
    return true;
}

bool shape_asset_bounds(const ShapeAsset *asset, ShapeAssetBounds *out_bounds) {
    (void)asset;
    if (!out_bounds) return false;
    memset(out_bounds, 0, sizeof(*out_bounds));
    return false;
}

bool physics_object_from_asset(const ShapeAsset *asset,
                               const SceneObjectBase *base,
                               int grid_w,
                               int grid_h,
                               const ShapeAssetRasterOptions *extra_opts,
                               PhysicsObject *out) {
    (void)asset;
    (void)base;
    (void)grid_w;
    (void)grid_h;
    (void)extra_opts;
    (void)out;
    return false;
}

void physics_object_free(PhysicsObject *obj) {
    (void)obj;
}

bool shape_import_load(const char *path, ShapeDocument *out_doc) {
    (void)path;
    (void)out_doc;
    return false;
}

bool shape_import_bounds(const Shape *shape, ShapeBounds *out_bounds) {
    (void)shape;
    if (!out_bounds) return false;
    memset(out_bounds, 0, sizeof(*out_bounds));
    return false;
}

bool shape_import_rasterize(const Shape *shape,
                            int grid_w,
                            int grid_h,
                            const ShapeRasterOptions *opts,
                            uint8_t *mask_out) {
    (void)shape;
    (void)grid_w;
    (void)grid_h;
    (void)opts;
    (void)mask_out;
    return false;
}

void ShapeDocument_Free(ShapeDocument *doc) {
    (void)doc;
}

void backend_2d_compute_obstacle_distance(const SceneState *scene,
                                          SimRuntimeBackend2D *state) {
    (void)scene;
    (void)state;
}

float backend_2d_import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
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
    (void)scene;
    (void)imp;
    if (out_mask && mask_count > 0) memset(out_mask, 0, mask_count);
    return false;
}

void ts_start_timer(const char *name) {
    (void)name;
}

void ts_stop_timer(const char *name) {
    (void)name;
}

static float sum_positive(const float *values, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        if (values[i] > 0.0f) sum += values[i];
    }
    return sum;
}

static float sum_abs(const float *values, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum += fabsf(values[i]);
    }
    return sum;
}

static size_t count_nonzero_mask(const uint8_t *values, size_t count) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (values[i]) ++hits;
    }
    return hits;
}

static bool test_zero_dt_does_not_inject_fields(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend backend = {0};
    SimRuntimeBackend2D state = {0};
    size_t count = 0;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;
    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_velocity_multiplier = 1.0f;
    cfg.emitter_sink_multiplier = 1.0f;

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .radius = 0.08f,
        .strength = 6.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    state.fluid = fluid2d_create(cfg.grid_w, cfg.grid_h);
    if (!state.fluid) return false;
    backend.impl = &state;
    scene.backend = &backend;
    scene.config = &cfg;
    scene.preset = &preset;
    scene.emitters_enabled = true;

    backend_2d_apply_emitters(&backend, &scene, 0.0);
    count = (size_t)state.fluid->w * (size_t)state.fluid->h;
    if (sum_positive(state.fluid->density, count) != 0.0f) return false;
    if (sum_abs(state.fluid->velX, count) != 0.0f) return false;
    if (sum_abs(state.fluid->velY, count) != 0.0f) return false;

    fluid2d_destroy(state.fluid);
    return true;
}

static bool test_free_velocity_jet_injects_density_and_velocity(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend backend = {0};
    SimRuntimeBackend2D state = {0};
    size_t count = 0;
    float density_sum = 0.0f;
    float vel_x_sum = 0.0f;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;
    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_velocity_multiplier = 1.0f;
    cfg.emitter_sink_multiplier = 1.0f;

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .radius = 0.08f,
        .strength = 6.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    state.fluid = fluid2d_create(cfg.grid_w, cfg.grid_h);
    if (!state.fluid) return false;
    backend.impl = &state;
    scene.backend = &backend;
    scene.config = &cfg;
    scene.preset = &preset;
    scene.emitters_enabled = true;

    backend_2d_apply_emitters(&backend, &scene, 0.1);
    count = (size_t)state.fluid->w * (size_t)state.fluid->h;
    density_sum = sum_positive(state.fluid->density, count);
    vel_x_sum = sum_abs(state.fluid->velX, count);

    if (density_sum <= 0.0f) return false;
    if (vel_x_sum <= 0.0f) return false;
    if (sum_abs(state.fluid->velY, count) > 0.0001f) return false;

    fluid2d_destroy(state.fluid);
    return true;
}

static bool test_attached_object_velocity_jet_injects_density_and_velocity(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend backend = {0};
    SimRuntimeBackend2D state = {0};
    size_t count = 0;
    float density_sum = 0.0f;
    float vel_x_sum = 0.0f;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;
    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_velocity_multiplier = 1.0f;
    cfg.emitter_sink_multiplier = 1.0f;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .size_x = 0.08f,
        .size_y = 0.05f,
        .angle = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .strength = 6.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .attached_object = 0,
        .attached_import = -1,
    };

    state.fluid = fluid2d_create(cfg.grid_w, cfg.grid_h);
    if (!state.fluid) return false;
    backend.impl = &state;
    scene.backend = &backend;
    scene.config = &cfg;
    scene.preset = &preset;
    scene.emitters_enabled = true;

    backend_2d_apply_emitters(&backend, &scene, 0.1);
    count = (size_t)state.fluid->w * (size_t)state.fluid->h;
    density_sum = sum_positive(state.fluid->density, count);
    vel_x_sum = sum_abs(state.fluid->velX, count);

    if (density_sum <= 0.0f) return false;
    if (vel_x_sum <= 0.0f) return false;
    if (sum_abs(state.fluid->velY, count) > 0.0001f) return false;

    fluid2d_destroy(state.fluid);
    return true;
}

static bool test_dynamic_circle_obstacle_writes_mask_and_velocity(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend backend = {0};
    SimRuntimeBackend2D state = {0};
    SceneObject object = {0};
    size_t count = 0;
    size_t obstacle_hits = 0;
    float obstacle_vel_x_sum = 0.0f;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;

    count = (size_t)cfg.grid_w * (size_t)cfg.grid_h;
    state.obstacle_mask = (uint8_t *)calloc(count, sizeof(uint8_t));
    state.obstacle_vel_x = (float *)calloc(count, sizeof(float));
    state.obstacle_vel_y = (float *)calloc(count, sizeof(float));
    if (!state.obstacle_mask || !state.obstacle_vel_x || !state.obstacle_vel_y) {
        free(state.obstacle_mask);
        free(state.obstacle_vel_x);
        free(state.obstacle_vel_y);
        return false;
    }

    object.source_import = -1;
    object.body.shape = RIGID2D_SHAPE_CIRCLE;
    object.body.position.x = 160.0f;
    object.body.position.y = 120.0f;
    object.body.velocity.x = 40.0f;
    object.body.velocity.y = 0.0f;
    object.body.radius = 18.0f;
    object.body.is_static = 0;

    backend.impl = &state;
    scene.backend = &backend;
    scene.config = &cfg;
    scene.preset = &preset;
    scene.objects.objects = &object;
    scene.objects.count = 1;

    backend_2d_rasterize_dynamic_obstacles(&backend, &scene);
    obstacle_hits = count_nonzero_mask(state.obstacle_mask, count);
    obstacle_vel_x_sum = sum_abs(state.obstacle_vel_x, count);

    free(state.obstacle_mask);
    free(state.obstacle_vel_x);
    free(state.obstacle_vel_y);

    if (obstacle_hits == 0) return false;
    if (obstacle_vel_x_sum <= 0.0f) return false;
    return true;
}

int main(void) {
    if (!test_zero_dt_does_not_inject_fields()) {
        fprintf(stderr,
                "sim_runtime_backend_2d_runtime_fields_contract_test: zero-dt guard failed\n");
        return 1;
    }
    if (!test_free_velocity_jet_injects_density_and_velocity()) {
        fprintf(stderr,
                "sim_runtime_backend_2d_runtime_fields_contract_test: free emitter injection failed\n");
        return 1;
    }
    if (!test_attached_object_velocity_jet_injects_density_and_velocity()) {
        fprintf(stderr,
                "sim_runtime_backend_2d_runtime_fields_contract_test: attached object emitter injection failed\n");
        return 1;
    }
    if (!test_dynamic_circle_obstacle_writes_mask_and_velocity()) {
        fprintf(stderr,
                "sim_runtime_backend_2d_runtime_fields_contract_test: dynamic obstacle velocity bridge failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_2d_runtime_fields_contract_test: success\n");
    return 0;
}
