#ifndef PHYSICS_SIM_SESSION_OBSERVATION_H
#define PHYSICS_SIM_SESSION_OBSERVATION_H
#include "app/scene_state.h"
#include <json-c/json.h>
// Bounded XY readout from sparse authority. Does not materialize export fields.
struct json_object *physics_sim_session_observation(const SceneState *scene);
// Safe-boundary rich slice/probe readback; no dense export.
struct json_object *physics_sim_session_sample(const SceneState *scene,
                                               struct json_object *request);
#endif
