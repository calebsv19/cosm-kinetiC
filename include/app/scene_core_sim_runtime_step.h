#ifndef SCENE_CORE_SIM_RUNTIME_STEP_H
#define SCENE_CORE_SIM_RUNTIME_STEP_H

#include <stdbool.h>

#include "app/scene_state.h"
#include "app/sim_mode.h"
#include "core_sim.h"

typedef struct PhysicsSimSceneCoreSimStepResult {
    CoreSimFrameOutcome outcome;
    int substeps_requested;
    double substep_dt;
} PhysicsSimSceneCoreSimStepResult;

enum {
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_PRE = 1u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_EMITTERS_BOUNDARY = 2u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_BACKEND_STEP = 3u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_POST_ENFORCE = 4u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_MODE_POST = 5u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_OBJECTS = 6u,
    PHYSICS_SIM_SCENE_CORE_SIM_PASS_DYNAMIC_OBSTACLES = 7u
};

void physics_sim_scene_core_sim_set_paused(SceneState *scene, bool paused);

bool physics_sim_scene_core_sim_step(SceneState *scene,
                                     AppConfig *cfg,
                                     const SimModeHooks *mode_hooks,
                                     double dt,
                                     PhysicsSimSceneCoreSimStepResult *result);

#endif
