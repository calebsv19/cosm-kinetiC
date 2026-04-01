#ifndef SIM_MODE_H
#define SIM_MODE_H

#include "app/app_config.h"
#include "app/scene_presets.h"

struct SceneState;

typedef struct SimModeHooks {
    void (*configure_app)(AppConfig *cfg, FluidScenePreset *preset);
    void (*prepare_scene)(struct SceneState *scene);
    void (*pre_substep)(struct SceneState *scene, double dt);
    void (*post_substep)(struct SceneState *scene, double dt);
} SimModeHooks;

typedef enum SimBackendLane {
    SIM_BACKEND_CANONICAL_2D = 0,
    SIM_BACKEND_CONTROLLED_3D = 1
} SimBackendLane;

typedef struct SimModeRoute {
    SimulationMode simulation_mode;
    SpaceMode requested_space_mode;
    SpaceMode projection_space_mode;
    SimBackendLane backend_lane;
    bool backend_uses_canonical_2d_solver;
    bool fallback_to_2d_projection;
    const SimModeHooks *hooks;
} SimModeRoute;

SimModeRoute sim_mode_resolve_route(SimulationMode mode, SpaceMode space_mode);
const SimModeHooks *sim_mode_get_hooks(SimulationMode mode);

#endif // SIM_MODE_H
