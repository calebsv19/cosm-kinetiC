#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "import/shape_import.h"
#include "physics/fluid2d/fluid2d.h"
#include "physics/objects/physics_object_builder.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void ts_start_timer(const char *name) {
    (void)name;
}

void ts_stop_timer(const char *name) {
    (void)name;
}

SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(
    const AppConfig *cfg,
    const FluidScenePreset *preset,
    const SimModeRoute *mode_route,
    const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
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

static bool test_boundary_flow_zero_dt_is_noop(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SceneFluidFieldView2D view = {0};
    SimRuntimeBackend *backend = NULL;
    size_t count = 0;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].mode = BOUNDARY_FLOW_EMIT;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].strength = 5.0f;

    backend = sim_runtime_backend_create(&cfg, &preset, NULL, NULL);
    if (!backend || !backend->ops || !backend->ops->apply_boundary_flows ||
        !backend->ops->get_fluid_view_2d) {
        sim_runtime_backend_destroy(backend);
        return false;
    }

    scene.backend = backend;
    scene.config = &cfg;
    scene.preset = &preset;

    backend->ops->apply_boundary_flows(backend, &scene, 0.0);
    if (!backend->ops->get_fluid_view_2d(backend, &view)) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    count = view.cell_count;
    if (sum_positive(view.density, count) != 0.0f) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    if (sum_abs(view.velocity_x, count) != 0.0f) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    if (sum_abs(view.velocity_y, count) != 0.0f) {
        sim_runtime_backend_destroy(backend);
        return false;
    }

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_boundary_flow_positive_dt_injects_field(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SceneFluidFieldView2D view = {0};
    SimRuntimeBackend *backend = NULL;
    size_t count = 0;
    float density_sum = 0.0f;
    float velocity_sum = 0.0f;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].mode = BOUNDARY_FLOW_EMIT;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].strength = 5.0f;

    backend = sim_runtime_backend_create(&cfg, &preset, NULL, NULL);
    if (!backend || !backend->ops || !backend->ops->apply_boundary_flows ||
        !backend->ops->get_fluid_view_2d) {
        sim_runtime_backend_destroy(backend);
        return false;
    }

    scene.backend = backend;
    scene.config = &cfg;
    scene.preset = &preset;

    backend->ops->apply_boundary_flows(backend, &scene, 0.1);
    if (!backend->ops->get_fluid_view_2d(backend, &view)) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    count = view.cell_count;
    density_sum = sum_positive(view.density, count);
    velocity_sum = sum_abs(view.velocity_x, count) + sum_abs(view.velocity_y, count);

    sim_runtime_backend_destroy(backend);
    return density_sum > 0.0f && velocity_sum > 0.0f;
}

static bool test_object_motion_injects_velocity(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SceneFluidFieldView2D view = {0};
    SimRuntimeBackend *backend = NULL;
    SceneObject object = {0};
    size_t count = 0;
    float velocity_sum = 0.0f;

    cfg.grid_w = 32;
    cfg.grid_h = 24;
    cfg.window_w = 320;
    cfg.window_h = 240;

    backend = sim_runtime_backend_create(&cfg, &preset, NULL, NULL);
    if (!backend || !backend->ops || !backend->ops->inject_object_motion ||
        !backend->ops->get_fluid_view_2d) {
        sim_runtime_backend_destroy(backend);
        return false;
    }

    object.type = SCENE_OBJECT_CIRCLE;
    object.body.position.x = 160.0f;
    object.body.position.y = 120.0f;
    object.body.velocity.x = 40.0f;
    object.body.velocity.y = 0.0f;
    object.body.is_static = 0;
    object.body.locked = 0;

    scene.backend = backend;
    scene.config = &cfg;
    scene.preset = &preset;
    scene.objects.objects = &object;
    scene.objects.count = 1;

    backend->ops->inject_object_motion(backend, &scene);
    if (!backend->ops->get_fluid_view_2d(backend, &view)) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    count = view.cell_count;
    velocity_sum = sum_abs(view.velocity_x, count);

    sim_runtime_backend_destroy(backend);
    return velocity_sum > 0.0f;
}

int main(void) {
    if (!test_boundary_flow_zero_dt_is_noop()) {
        fprintf(stderr, "sim_runtime_backend_2d_contract_test: zero-dt boundary flow failed\n");
        return 1;
    }
    if (!test_boundary_flow_positive_dt_injects_field()) {
        fprintf(stderr, "sim_runtime_backend_2d_contract_test: positive-dt boundary flow failed\n");
        return 1;
    }
    if (!test_object_motion_injects_velocity()) {
        fprintf(stderr, "sim_runtime_backend_2d_contract_test: object motion injection failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_2d_contract_test: success\n");
    return 0;
}
