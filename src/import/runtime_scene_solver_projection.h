#ifndef PHYSICS_SIM_RUNTIME_SCENE_SOLVER_PROJECTION_H
#define PHYSICS_SIM_RUNTIME_SCENE_SOLVER_PROJECTION_H

#include <stdbool.h>

#include <json-c/json.h>

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "import/runtime_scene_bridge.h"

bool runtime_scene_solver_projection_apply_runtime(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                   json_object *runtime_root,
                                                   AppConfig *in_out_cfg,
                                                   FluidScenePreset *in_out_preset,
                                                   RuntimeSceneBridgePreflight *out_summary);

#endif
