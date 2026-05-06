#include "app/sim_runtime_3d_solver_core_sim.h"

typedef struct PhysicsSim3DSolverCoreSimContext {
    SimRuntime3DVolume *volume;
    SimRuntime3DSolverScratch *scratch;
    const uint8_t *solid_mask;
    const SimRuntime3DForceAxis *scene_up_axis;
    const AppConfig *cfg;
    float max_velocity_displacement_cells_limit;
    SimRuntime3DSolverStepMetrics *out_metrics;
    bool solver_step_ran;
} PhysicsSim3DSolverCoreSimContext;

static bool run_solver_first_pass(void *user_context,
                                  const CoreSimTickContext *tick,
                                  CoreSimPassOutcome *outcome) {
    PhysicsSim3DSolverCoreSimContext *ctx =
        (PhysicsSim3DSolverCoreSimContext *)user_context;

    core_sim_pass_outcome_init(outcome, PHYSICS_SIM_3D_SOLVER_CORE_SIM_PASS_FIRST_PASS);

    if (!ctx || !tick || !ctx->volume || !ctx->scratch || !ctx->cfg) {
        if (outcome) {
            outcome->status = CORE_SIM_STATUS_INVALID_ARGUMENT;
            outcome->message = "missing 3d solver context";
        }
        return false;
    }

    if (!sim_runtime_3d_solver_step_first_pass(ctx->volume,
                                               ctx->scratch,
                                               ctx->solid_mask,
                                               ctx->scene_up_axis,
                                               ctx->cfg,
                                               tick->dt_seconds,
                                               ctx->max_velocity_displacement_cells_limit,
                                               ctx->out_metrics)) {
        if (outcome) {
            outcome->status = CORE_SIM_STATUS_PASS_FAILED;
            outcome->message = "3d solver first pass failed";
        }
        return false;
    }

    ctx->solver_step_ran = true;
    return true;
}

bool sim_runtime_3d_solver_core_sim_step_first_pass(
    CoreSimLoopState *loop_state,
    SimRuntime3DVolume *volume,
    SimRuntime3DSolverScratch *scratch,
    const uint8_t *solid_mask,
    const SimRuntime3DForceAxis *scene_up_axis,
    const AppConfig *cfg,
    double dt,
    float max_velocity_displacement_cells_limit,
    CoreSimFrameOutcome *outcome,
    SimRuntime3DSolverStepMetrics *out_metrics) {
    CoreSimStepPolicy policy;
    PhysicsSim3DSolverCoreSimContext ctx;
    CoreSimPassDescriptor passes[1];
    CoreSimPassOrder pass_order;
    CoreSimFrameRequest request;
    CoreSimFrameOutcome frame_outcome;

    if (!loop_state || !volume || !scratch || !cfg || dt <= 0.0) {
        if (outcome) {
            *outcome = core_sim_frame_outcome_make_invalid(
                CORE_SIM_STATUS_INVALID_ARGUMENT,
                "invalid 3d solver core sim step arguments");
        }
        return false;
    }

    policy.fixed_dt_seconds = dt;
    policy.max_ticks_per_frame = 1u;
    policy.drop_excess_accumulator_on_clamp = true;

    if (!core_sim_step_policy_valid(&loop_state->policy)) {
        if (!core_sim_loop_init(loop_state, &policy)) {
            if (outcome) {
                *outcome = core_sim_frame_outcome_make_invalid(
                    CORE_SIM_STATUS_INVALID_POLICY,
                    "invalid 3d solver core sim policy");
            }
            return false;
        }
    } else {
        loop_state->policy = policy;
    }
    core_sim_loop_set_paused(loop_state, false);

    ctx.volume = volume;
    ctx.scratch = scratch;
    ctx.solid_mask = solid_mask;
    ctx.scene_up_axis = scene_up_axis;
    ctx.cfg = cfg;
    ctx.max_velocity_displacement_cells_limit = max_velocity_displacement_cells_limit;
    ctx.out_metrics = out_metrics;
    ctx.solver_step_ran = false;

    passes[0].pass_id = PHYSICS_SIM_3D_SOLVER_CORE_SIM_PASS_FIRST_PASS;
    passes[0].name = "3d_solver_first_pass";
    passes[0].run = run_solver_first_pass;

    pass_order.passes = passes;
    pass_order.pass_count = 1u;

    request.frame_dt_seconds = dt;
    request.user_context = &ctx;
    request.pass_order = &pass_order;

    frame_outcome = core_sim_loop_advance(loop_state, &request);
    if (outcome) {
        *outcome = frame_outcome;
    }

    return frame_outcome.status == CORE_SIM_STATUS_OK &&
           frame_outcome.ticks_executed == 1u &&
           frame_outcome.passes_executed == 1u &&
           ctx.solver_step_ran;
}
