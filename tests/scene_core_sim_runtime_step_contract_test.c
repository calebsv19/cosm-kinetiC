#include "app/scene_core_sim_runtime_step.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void ts_start_timer(const char *name) {
    (void)name;
}

void ts_stop_timer(const char *name) {
    (void)name;
}

void sim_runtime_backend_apply_emitters(SimRuntimeBackend *backend,
                                        struct SceneState *scene,
                                        double dt) {
    (void)backend;
    (void)scene;
    (void)dt;
}

void sim_runtime_backend_apply_boundary_flows(SimRuntimeBackend *backend,
                                              struct SceneState *scene,
                                              double dt) {
    (void)backend;
    (void)scene;
    (void)dt;
}

void sim_runtime_backend_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    (void)backend;
    (void)scene;
}

void sim_runtime_backend_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene) {
    (void)backend;
    (void)scene;
}

void sim_runtime_backend_step(SimRuntimeBackend *backend,
                              struct SceneState *scene,
                              const AppConfig *cfg,
                              double dt) {
    (void)backend;
    (void)scene;
    (void)cfg;
    (void)dt;
}

void sim_runtime_backend_inject_object_motion(SimRuntimeBackend *backend,
                                              const struct SceneState *scene) {
    (void)backend;
    (void)scene;
}

void scene_rasterize_dynamic_obstacles(SceneState *scene) {
    (void)scene;
}

SimModeStepPolicy sim_mode_step_policy(const SimModeRoute *route,
                                       FluidSceneDimensionMode dimension_mode) {
    SimModeStepPolicy policy;
    policy.constrained_3d_active = route && route->constrained_3d_solver_scaffold &&
                                   dimension_mode == SCENE_DIMENSION_MODE_3D;
    policy.min_substeps = policy.constrained_3d_active && route->constrained_3d_min_substeps > 1
                              ? route->constrained_3d_min_substeps
                              : 1;
    policy.buoyancy_scale = policy.constrained_3d_active && route->constrained_3d_buoyancy_scale > 0.0f
                                ? route->constrained_3d_buoyancy_scale
                                : 1.0f;
    return policy;
}

static AppConfig test_config(void) {
    AppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.physics_substeps = 3;
    cfg.fluid_solver_iterations = 4;
    cfg.velocity_damping = 0.98f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 1.0f;
    cfg.sim_mode = SIM_MODE_BOX;
    cfg.space_mode = SPACE_MODE_2D;
    return cfg;
}

static bool nearly_equal(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 0.000001;
}

static bool test_scene_runtime_step_uses_core_sim_pass_network(void) {
    AppConfig cfg = test_config();
    SceneState scene;
    PhysicsSimSceneCoreSimStepResult result;

    memset(&scene, 0, sizeof(scene));
    memset(&result, 0, sizeof(result));
    scene.config = &cfg;
    scene.mode_route.simulation_mode = cfg.sim_mode;
    scene.mode_route.requested_space_mode = cfg.space_mode;
    scene.mode_route.projection_space_mode = cfg.space_mode;
    scene.mode_route.backend_lane = SIM_BACKEND_CANONICAL_2D;
    scene.mode_route.backend_uses_canonical_2d_solver = true;
    if (!core_sim_loop_init(&scene.runtime_loop, NULL)) return false;

    if (!physics_sim_scene_core_sim_step(&scene, &cfg, NULL, 0.03, &result)) {
        fprintf(stderr,
                "step failed: status=%d ticks=%u passes=%u failed_pass=%u message=%s\n",
                (int)result.outcome.status,
                result.outcome.ticks_executed,
                result.outcome.passes_executed,
                result.outcome.failed_pass_id,
                result.outcome.message ? result.outcome.message : "(none)");
        return false;
    }

    if (!(result.substeps_requested == 3 &&
          nearly_equal(result.substep_dt, 0.01) &&
          result.outcome.status == CORE_SIM_STATUS_OK &&
          result.outcome.ticks_executed == 3u &&
          result.outcome.passes_executed == 21u &&
          scene.runtime_loop.tick_index == 3u &&
          scene.runtime_loop.frame_index == 1u &&
          nearly_equal(scene.time, 0.03))) {
        fprintf(stderr,
                "substeps=%d sub_dt=%f status=%d ticks=%u passes=%u tick_index=%llu frame_index=%llu time=%f accumulator=%f\n",
                result.substeps_requested,
                result.substep_dt,
                (int)result.outcome.status,
                result.outcome.ticks_executed,
                result.outcome.passes_executed,
                (unsigned long long)scene.runtime_loop.tick_index,
                (unsigned long long)scene.runtime_loop.frame_index,
                scene.time,
                scene.runtime_loop.accumulator_seconds);
        return false;
    }
    return true;
}

static bool test_scene_runtime_step_syncs_pause_state(void) {
    SceneState scene;
    memset(&scene, 0, sizeof(scene));
    if (!core_sim_loop_init(&scene.runtime_loop, NULL)) return false;

    physics_sim_scene_core_sim_set_paused(&scene, false);
    if (scene.runtime_loop.paused) return false;

    scene.runtime_loop.accumulator_seconds = 0.5;
    physics_sim_scene_core_sim_set_paused(&scene, true);
    return scene.runtime_loop.paused && nearly_equal(scene.runtime_loop.accumulator_seconds, 0.0);
}

int main(void) {
    if (!test_scene_runtime_step_uses_core_sim_pass_network()) {
        fprintf(stderr, "scene_core_sim_runtime_step_contract_test: pass network failed\n");
        return 1;
    }
    if (!test_scene_runtime_step_syncs_pause_state()) {
        fprintf(stderr, "scene_core_sim_runtime_step_contract_test: pause sync failed\n");
        return 1;
    }
    fprintf(stdout, "scene_core_sim_runtime_step_contract_test: success\n");
    return 0;
}
