#ifndef PHYSICS_SIM_SESSION_OBSERVATION_H
#define PHYSICS_SIM_SESSION_OBSERVATION_H
#include "app/scene_state.h"
#include <json-c/json.h>
// Bounded XY readout from sparse authority. Does not materialize export fields.
struct json_object *physics_sim_session_observation(const SceneState *scene);
#endif
