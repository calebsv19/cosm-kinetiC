#include "app/scene_core_sim_runtime_step.h"

#include <math.h>

#include "app/scene_apply.h"
#include "app/sim_runtime_backend.h"
#include "physics/objects/object_manager.h"
#include "timer_hud/time_scope.h"

typedef struct PhysicsSimSceneCoreSimStepContext {
    SceneState *scene;
    AppConfig *step_cfg;
    const SimModeHooks *mode_hooks;
} PhysicsSimSceneCoreSimStepContext;

static void set_pass_failure(CoreSimPassOutcome *outcome,
                             uint32_t pass_id,
                             const char *message) {
    if (!outcome) return;
    outcome->status = CORE_SIM_STATUS_PASS_FAILED;
    outcome->pass_id = pass_id;
    outcome->message = message;
}

static bool run_mode_pre(void *user_context,
                         const CoreSimTickContext *tick,
                         CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_PRE);
    if (!ctx || !tick || !ctx->scene || !ctx->step_cfg) {
        set_pass_failure(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_PRE, "missing mode pre context");
        return false;
    }
    if (ctx->mode_hooks && ctx->mode_hooks->pre_substep) {
        ctx->mode_hooks->pre_substep(ctx->scene, tick->dt_seconds);
    }
    return true;
}

static bool run_emitters_boundary(void *user_context,
                                  const CoreSimTickContext *tick,
                                  CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_EMITTERS_BOUNDARY);
    if (!ctx || !tick || !ctx->scene) {
        set_pass_failure(outcome,
                         PHYSICS_SIM_SCENE_CORE_SIM_PASS_EMITTERS_BOUNDARY,
                         "missing emitters/boundary context");
        return false;
    }
    scene_apply_emitters(ctx->scene, tick->dt_seconds);
    scene_apply_boundary_flows(ctx->scene, tick->dt_seconds);
    scene_enforce_obstacles(ctx->scene);
    return true;
}

static bool run_backend_step(void *user_context,
                             const CoreSimTickContext *tick,
                             CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_BACKEND_STEP);
    if (!ctx || !tick || !ctx->scene || !ctx->step_cfg) {
        set_pass_failure(outcome,
                         PHYSICS_SIM_SCENE_CORE_SIM_PASS_BACKEND_STEP,
                         "missing backend-step context");
        return false;
    }
    ts_start_timer("fluid_step");
    sim_runtime_backend_step(ctx->scene->backend, ctx->scene, ctx->step_cfg, tick->dt_seconds);
    ts_stop_timer("fluid_step");
    return true;
}

static bool run_post_enforce(void *user_context,
                             const CoreSimTickContext *tick,
                             CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    (void)tick;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_POST_ENFORCE);
    if (!ctx || !ctx->scene) {
        set_pass_failure(outcome,
                         PHYSICS_SIM_SCENE_CORE_SIM_PASS_POST_ENFORCE,
                         "missing post-enforce context");
        return false;
    }
    scene_enforce_obstacles(ctx->scene);
    scene_enforce_boundary_flows(ctx->scene);
    return true;
}

static bool run_mode_post(void *user_context,
                          const CoreSimTickContext *tick,
                          CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_POST);
    if (!ctx || !tick || !ctx->scene) {
        set_pass_failure(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_POST, "missing mode post context");
        return false;
    }
    if (ctx->mode_hooks && ctx->mode_hooks->post_substep) {
        ctx->mode_hooks->post_substep(ctx->scene, tick->dt_seconds);
    }
    return true;
}

static void sync_import_body_poses(SceneState *scene, const AppConfig *cfg) {
    if (!scene || !cfg || cfg->window_w == 0 || cfg->window_h == 0) return;
    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        int body_idx = scene->import_body_map[i];
        RigidBody2D *body = NULL;
        if (body_idx < 0 || body_idx >= scene->objects.count) continue;
        body = &scene->objects.objects[body_idx].body;
        scene->import_shapes[i].position_x = body->position.x / (float)cfg->window_w;
        scene->import_shapes[i].position_y = body->position.y / (float)cfg->window_h;
        scene->import_shapes[i].rotation_deg = body->angle * 180.0f / (float)M_PI;
    }
}

static bool run_objects(void *user_context,
                        const CoreSimTickContext *tick,
                        CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_OBJECTS);
    if (!ctx || !tick || !ctx->scene || !ctx->step_cfg) {
        set_pass_failure(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_OBJECTS, "missing object-step context");
        return false;
    }
    object_manager_step(&ctx->scene->objects,
                        tick->dt_seconds,
                        ctx->step_cfg,
                        ctx->scene->objects_gravity_enabled);
    sync_import_body_poses(ctx->scene, ctx->step_cfg);
    return true;
}

static bool run_dynamic_obstacles(void *user_context,
                                  const CoreSimTickContext *tick,
                                  CoreSimPassOutcome *outcome) {
    PhysicsSimSceneCoreSimStepContext *ctx = (PhysicsSimSceneCoreSimStepContext *)user_context;
    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_SCENE_CORE_SIM_PASS_DYNAMIC_OBSTACLES);
    if (!ctx || !tick || !ctx->scene) {
        set_pass_failure(outcome,
                         PHYSICS_SIM_SCENE_CORE_SIM_PASS_DYNAMIC_OBSTACLES,
                         "missing dynamic-obstacle context");
        return false;
    }
    scene_rasterize_dynamic_obstacles(ctx->scene);
    sim_runtime_backend_inject_object_motion(ctx->scene->backend, ctx->scene);
    ctx->scene->time += tick->dt_seconds;
    return true;
}

static int resolve_substeps(SceneState *scene, AppConfig *step_cfg) {
    SimModeStepPolicy step_policy;
    int substeps;

    if (!scene || !step_cfg) return 1;

    step_policy = sim_mode_step_policy(&scene->mode_route,
                                       scene->preset ? scene->preset->dimension_mode
                                                     : SCENE_DIMENSION_MODE_2D);
    if (step_policy.constrained_3d_active) {
        if (step_cfg->physics_substeps < step_policy.min_substeps) {
            step_cfg->physics_substeps = step_policy.min_substeps;
        }
        step_cfg->fluid_buoyancy_force *= step_policy.buoyancy_scale;
    }

    substeps = step_cfg->physics_substeps > 0 ? step_cfg->physics_substeps : 1;
    return substeps;
}

void physics_sim_scene_core_sim_set_paused(SceneState *scene, bool paused) {
    if (!scene) return;
    core_sim_loop_set_paused(&scene->runtime_loop, paused);
}

bool physics_sim_scene_core_sim_step(SceneState *scene,
                                     AppConfig *cfg,
                                     const SimModeHooks *mode_hooks,
                                     double dt,
                                     PhysicsSimSceneCoreSimStepResult *result) {
    static const CoreSimPassDescriptor passes[] = {
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_PRE, "mode_pre", run_mode_pre},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_EMITTERS_BOUNDARY, "emitters_boundary", run_emitters_boundary},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_BACKEND_STEP, "backend_step", run_backend_step},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_POST_ENFORCE, "post_enforce", run_post_enforce},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_POST, "mode_post", run_mode_post},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_OBJECTS, "objects", run_objects},
        {PHYSICS_SIM_SCENE_CORE_SIM_PASS_DYNAMIC_OBSTACLES, "dynamic_obstacles", run_dynamic_obstacles},
    };
    AppConfig step_cfg;
    CoreSimStepPolicy policy;
    CoreSimPassOrder pass_order;
    CoreSimFrameRequest request;
    PhysicsSimSceneCoreSimStepContext ctx;
    CoreSimFrameOutcome outcome;
    int substeps;
    double substep_dt;

    if (!scene || !cfg || dt <= 0.0) {
        if (result) {
            result->outcome = core_sim_frame_outcome_make_invalid(
                CORE_SIM_STATUS_INVALID_ARGUMENT,
                "invalid scene core sim step arguments");
            result->substeps_requested = 0;
            result->substep_dt = 0.0;
        }
        return false;
    }

    step_cfg = *cfg;
    substeps = resolve_substeps(scene, &step_cfg);
    substep_dt = dt / (double)substeps;

    policy.fixed_dt_seconds = substep_dt;
    policy.max_ticks_per_frame = (uint32_t)substeps;
    policy.drop_excess_accumulator_on_clamp = true;

    if (!core_sim_step_policy_valid(&scene->runtime_loop.policy)) {
        if (!core_sim_loop_init(&scene->runtime_loop, &policy)) {
            if (result) {
                result->outcome = core_sim_frame_outcome_make_invalid(
                    CORE_SIM_STATUS_INVALID_POLICY,
                    "invalid scene core sim policy");
                result->substeps_requested = substeps;
                result->substep_dt = substep_dt;
            }
            return false;
        }
    } else {
        scene->runtime_loop.policy = policy;
    }

    core_sim_loop_set_paused(&scene->runtime_loop, false);

    ctx.scene = scene;
    ctx.step_cfg = &step_cfg;
    ctx.mode_hooks = mode_hooks;

    pass_order.passes = passes;
    pass_order.pass_count = sizeof(passes) / sizeof(passes[0]);

    request.frame_dt_seconds = substep_dt * (double)substeps;
    request.frame_dt_seconds += substep_dt * 0.000000001;
    request.user_context = &ctx;
    request.pass_order = &pass_order;

    scene->runtime_loop.accumulator_seconds = 0.0;
    outcome = core_sim_loop_advance(&scene->runtime_loop, &request);
    if (outcome.accumulator_remaining_seconds > 0.0 &&
        outcome.accumulator_remaining_seconds < substep_dt * 0.000001) {
        scene->runtime_loop.accumulator_seconds = 0.0;
        outcome.accumulator_remaining_seconds = 0.0;
    }
    if (result) {
        result->outcome = outcome;
        result->substeps_requested = substeps;
        result->substep_dt = substep_dt;
    }

    return outcome.status == CORE_SIM_STATUS_OK && outcome.ticks_executed == (uint32_t)substeps;
}
