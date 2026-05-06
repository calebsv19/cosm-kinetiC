#ifndef SIM_RUNTIME_3D_SOLVER_CORE_SIM_H
#define SIM_RUNTIME_3D_SOLVER_CORE_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "app/sim_runtime_3d_solver.h"
#include "core_sim.h"

enum {
    PHYSICS_SIM_3D_SOLVER_CORE_SIM_PASS_FIRST_PASS = 1u
};

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
    SimRuntime3DSolverStepMetrics *out_metrics);

#endif
