#include "app/sim_mode.h"

#include <math.h>

#include "app/scene_state.h"
#include "physics/fluid2d/fluid2d_boundary.h"

static float clamp_positive(float value, float fallback) {
    if (!isfinite(value) || value <= 0.0f) {
        return fallback;
    }
    return value;
}

static void wind_apply_default_boundaries(const AppConfig *cfg, FluidScenePreset *preset) {
    if (!cfg || !preset) return;
    BoundaryFlow *flows = preset->boundary_flows;
    flows[BOUNDARY_EDGE_LEFT].mode = BOUNDARY_FLOW_EMIT;
    flows[BOUNDARY_EDGE_LEFT].strength = clamp_positive(cfg->tunnel_inflow_speed, 35.0f);

    flows[BOUNDARY_EDGE_RIGHT].mode = BOUNDARY_FLOW_RECEIVE;
    flows[BOUNDARY_EDGE_RIGHT].strength = clamp_positive(cfg->tunnel_inflow_speed, 35.0f);

    flows[BOUNDARY_EDGE_TOP].mode = BOUNDARY_FLOW_DISABLED;
    flows[BOUNDARY_EDGE_TOP].strength = 0.0f;

    flows[BOUNDARY_EDGE_BOTTOM].mode = BOUNDARY_FLOW_DISABLED;
    flows[BOUNDARY_EDGE_BOTTOM].strength = 0.0f;
}

static void wind_apply_config_tweaks(AppConfig *cfg) {
    if (!cfg) return;
    cfg->fluid_buoyancy_force = 0.0f;
    if (cfg->density_decay > 0.02f) {
        cfg->density_decay = 0.02f;
    } else if (cfg->density_decay < 0.005f) {
        cfg->density_decay = 0.005f;
    }
    if (cfg->density_diffusion < 1e-5f) {
        cfg->density_diffusion = 1e-5f;
    }
    if (cfg->fluid_solver_iterations < 30) {
        cfg->fluid_solver_iterations = 30;
    }
}

static void wind_seed_velocity_field(SceneState *scene) {
    if (!scene || !scene->smoke || !scene->config) return;
    if (scene->time > 0.0) return;
    size_t count = (size_t)scene->smoke->w * (size_t)scene->smoke->h;
    if (count == 0) return;
    float inflow = clamp_positive(scene->config->tunnel_inflow_speed, 25.0f);
    float vx = inflow * 0.5f;
    for (size_t i = 0; i < count; ++i) {
        scene->smoke->velX[i] = vx;
        scene->smoke->velY[i] = 0.0f;
    }
}

static void wind_configure(AppConfig *cfg, FluidScenePreset *preset) {
    if (!cfg || !preset) return;
    const int max_window_w = 1536;
    float width = clamp_positive(preset->domain_width, 1.0f);
    float height = clamp_positive(preset->domain_height, 1.0f);
    float aspect = width / height;
    if (aspect < 0.25f) aspect = 0.25f;
    if (aspect > 8.0f) aspect = 8.0f;
    int base_h = cfg->grid_h > 0 ? cfg->grid_h : 128;
    int new_w = (int)lroundf((float)base_h * aspect);
    if (new_w < 32) new_w = 32;
    cfg->grid_w = new_w;
    cfg->window_w = max_window_w;
    cfg->window_h = (int)lroundf((float)max_window_w / aspect);
    if (cfg->window_h < 256) cfg->window_h = 256;
    wind_apply_config_tweaks(cfg);
    wind_apply_default_boundaries(cfg, preset);
}

static void wind_prepare(SceneState *scene) {
    if (!scene || !scene->config || !scene->preset) return;
    scene_set_emitters_enabled(scene, false);
    scene->wind_ramp_steps = 0;
    wind_seed_velocity_field(scene);
    wind_apply_default_boundaries(scene->config, (FluidScenePreset *)scene->preset);
}

static void wind_pre_substep(SceneState *scene, double dt) {
    (void)dt;
    if (!scene || !scene->config || !scene->preset) return;
    scene_set_emitters_enabled(scene, false);
    wind_apply_default_boundaries(scene->config, (FluidScenePreset *)scene->preset);
}

static void wind_post_substep(SceneState *scene, double dt) {
    (void)scene;
    (void)dt;
}

const SimModeHooks g_sim_mode_wind = {
    .configure_app = wind_configure,
    .prepare_scene = wind_prepare,
    .pre_substep   = wind_pre_substep,
    .post_substep  = wind_post_substep,
};
