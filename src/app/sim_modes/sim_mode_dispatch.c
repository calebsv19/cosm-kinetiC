#include "app/sim_mode.h"

extern const SimModeHooks g_sim_mode_box;
extern const SimModeHooks g_sim_mode_wind;

static SpaceMode normalize_space_mode(SpaceMode mode) {
    if (mode < SPACE_MODE_2D || mode >= SPACE_MODE_COUNT) {
        return SPACE_MODE_2D;
    }
    return mode;
}

static const SimModeHooks *sim_mode_hooks_for_simulation_mode(SimulationMode mode) {
    switch (mode) {
    case SIM_MODE_WIND_TUNNEL:
        return &g_sim_mode_wind;
    case SIM_MODE_STRUCTURAL:
        return &g_sim_mode_box;
    case SIM_MODE_BOX:
    default:
        return &g_sim_mode_box;
    }
}

SimModeRoute sim_mode_resolve_route(SimulationMode mode, SpaceMode space_mode) {
    SimModeRoute route;
    route.simulation_mode = mode;
    route.requested_space_mode = normalize_space_mode(space_mode);
    route.projection_space_mode = route.requested_space_mode;
    route.backend_lane = SIM_BACKEND_CANONICAL_2D;
    route.backend_uses_canonical_2d_solver = true;
    route.fallback_to_2d_projection = false;
    route.hooks = sim_mode_hooks_for_simulation_mode(mode);

    if (route.requested_space_mode == SPACE_MODE_3D) {
        route.backend_lane = SIM_BACKEND_CONTROLLED_3D;
        route.projection_space_mode = SPACE_MODE_2D;
        route.fallback_to_2d_projection = true;
        route.backend_uses_canonical_2d_solver = true;
    }

    return route;
}

const SimModeHooks *sim_mode_get_hooks(SimulationMode mode) {
    return sim_mode_resolve_route(mode, SPACE_MODE_2D).hooks;
}
