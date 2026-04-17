#include "app/sim_runtime_3d_space.h"

#include <stdbool.h>
#include <stdio.h>

static bool nearly_equal(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 0.0001;
}

static bool test_resolve_world_axis_maps_normalized_and_clamps_world(void) {
    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(0.25, -2.0, 6.0), 0.0)) return false;
    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(-5.0, -2.0, 6.0), -2.0)) return false;
    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(8.0, -2.0, 6.0), 6.0)) return false;
    if (!nearly_equal(sim_runtime_3d_space_resolve_world_axis(1.5, -2.0, 6.0), 1.5)) return false;
    return true;
}

static bool test_normalize_world_axis_and_half_extent_clamp_consistently(void) {
    if (!nearly_equal(sim_runtime_3d_space_normalize_world_axis(1.5, -2.5, 2.5), 0.8)) return false;
    if (!nearly_equal(sim_runtime_3d_space_normalize_world_axis(-9.0, -2.5, 2.5), 0.0)) return false;
    if (!nearly_equal(sim_runtime_3d_space_normalize_world_axis(9.0, -2.5, 2.5), 1.0)) return false;
    if (!nearly_equal(sim_runtime_3d_space_normalize_half_extent(0.25, 5.0, 0.04), 0.05)) return false;
    if (!nearly_equal(sim_runtime_3d_space_normalize_half_extent(0.0, 5.0, 0.04), 0.04)) return false;
    if (!nearly_equal(sim_runtime_3d_space_normalize_half_extent(9.0, 5.0, 0.04), 1.0)) return false;
    return true;
}

static bool test_world_to_grid_axis_uses_voxel_size_and_clamps(void) {
    if (sim_runtime_3d_space_world_to_grid_axis(-5.0, -1.0, 0.5, 8) != 0) return false;
    if (sim_runtime_3d_space_world_to_grid_axis(0.10, -1.0, 0.5, 8) != 2) return false;
    if (sim_runtime_3d_space_world_to_grid_axis(9.0, -1.0, 0.5, 8) != 7) return false;
    return true;
}

static bool test_slice_world_z_matches_domain_centering_and_clamps(void) {
    SimRuntime3DDomainDesc desc = {0};
    desc.world_min_z = -3.0f;
    desc.voxel_size = 0.25f;
    desc.grid_d = 12;

    if (!nearly_equal(sim_runtime_3d_space_slice_world_z(-3.0, 0.25, 12, 0), -2.875)) return false;
    if (!nearly_equal(sim_runtime_3d_space_slice_world_z(-3.0, 0.25, 12, 99), -0.125)) return false;
    if (!nearly_equal(sim_runtime_3d_space_slice_world_z_for_desc(&desc, 4), -1.875)) return false;
    return true;
}

int main(void) {
    if (!test_resolve_world_axis_maps_normalized_and_clamps_world()) {
        fprintf(stderr, "sim_runtime_3d_space_contract_test: world axis resolve failed\n");
        return 1;
    }
    if (!test_world_to_grid_axis_uses_voxel_size_and_clamps()) {
        fprintf(stderr, "sim_runtime_3d_space_contract_test: world-to-grid failed\n");
        return 1;
    }
    if (!test_normalize_world_axis_and_half_extent_clamp_consistently()) {
        fprintf(stderr, "sim_runtime_3d_space_contract_test: normalization failed\n");
        return 1;
    }
    if (!test_slice_world_z_matches_domain_centering_and_clamps()) {
        fprintf(stderr, "sim_runtime_3d_space_contract_test: slice world-z failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_3d_space_contract_test: success\n");
    return 0;
}
