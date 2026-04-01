#include "app/sim_mode.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * Test-local hook symbols satisfy sim_mode_dispatch.c externs without pulling
 * full runtime dependencies.
 */
const SimModeHooks g_sim_mode_box = {0};
const SimModeHooks g_sim_mode_wind = {0};

static bool test_route_2d_canonical_backend(void) {
    SimModeRoute route = sim_mode_resolve_route(SIM_MODE_BOX, SPACE_MODE_2D);
    if (route.requested_space_mode != SPACE_MODE_2D) return false;
    if (route.projection_space_mode != SPACE_MODE_2D) return false;
    if (route.backend_lane != SIM_BACKEND_CANONICAL_2D) return false;
    if (!route.backend_uses_canonical_2d_solver) return false;
    if (route.fallback_to_2d_projection) return false;
    if (!route.hooks) return false;
    return true;
}

static bool test_route_3d_controlled_lane(void) {
    SimModeRoute route = sim_mode_resolve_route(SIM_MODE_BOX, SPACE_MODE_3D);
    if (route.requested_space_mode != SPACE_MODE_3D) return false;
    if (route.projection_space_mode != SPACE_MODE_2D) return false;
    if (route.backend_lane != SIM_BACKEND_CONTROLLED_3D) return false;
    if (!route.backend_uses_canonical_2d_solver) return false;
    if (!route.fallback_to_2d_projection) return false;
    if (!route.hooks) return false;
    return true;
}

static bool test_invalid_space_mode_clamps_to_2d(void) {
    SpaceMode invalid_mode = (SpaceMode)99;
    SimModeRoute route = sim_mode_resolve_route(SIM_MODE_BOX, invalid_mode);
    if (route.requested_space_mode != SPACE_MODE_2D) return false;
    if (route.projection_space_mode != SPACE_MODE_2D) return false;
    if (route.backend_lane != SIM_BACKEND_CANONICAL_2D) return false;
    if (!route.backend_uses_canonical_2d_solver) return false;
    if (route.fallback_to_2d_projection) return false;
    return true;
}

int main(void) {
    if (!test_route_2d_canonical_backend()) {
        fprintf(stderr, "sim_mode_route_contract_test: 2D route contract failed\n");
        return 1;
    }
    if (!test_route_3d_controlled_lane()) {
        fprintf(stderr, "sim_mode_route_contract_test: 3D controlled lane contract failed\n");
        return 1;
    }
    if (!test_invalid_space_mode_clamps_to_2d()) {
        fprintf(stderr, "sim_mode_route_contract_test: invalid-space clamp contract failed\n");
        return 1;
    }
    fprintf(stdout, "sim_mode_route_contract_test: success\n");
    return 0;
}
