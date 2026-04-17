#include "app/sim_runtime_3d_footprint.h"

#include <stdbool.h>
#include <stdio.h>

static bool test_emitter_radius_uses_world_span_and_voxel_size(void) {
    SimRuntime3DDomainDesc coarse = {
        .world_min_x = -5.0f,
        .world_max_x = 5.0f,
        .world_min_y = -2.0f,
        .world_max_y = 2.0f,
        .world_min_z = -1.0f,
        .world_max_z = 1.0f,
        .voxel_size = 0.25f,
    };
    SimRuntime3DDomainDesc dense = coarse;
    dense.voxel_size = 0.10f;

    if (sim_runtime_3d_footprint_emitter_radius_cells(&coarse, 0.10f) != 2) return false;
    if (sim_runtime_3d_footprint_emitter_radius_cells(&dense, 0.10f) != 4) return false;
    if (sim_runtime_3d_footprint_emitter_radius_cells(&coarse, 2.0f) != 8) return false;
    if (sim_runtime_3d_footprint_emitter_radius_cells(&dense, 2.0f) != 20) return false;
    return true;
}

static bool test_object_box_half_extents_accept_normalized_and_world_sizes(void) {
    SimRuntime3DDomainDesc desc = {
        .world_min_x = -5.0f,
        .world_max_x = 5.0f,
        .world_min_y = -2.0f,
        .world_max_y = 2.0f,
        .world_min_z = -1.0f,
        .world_max_z = 1.0f,
        .voxel_size = 0.50f,
    };
    PresetObject normalized = {
        .type = PRESET_OBJECT_BOX,
        .size_x = 0.10f,
        .size_y = 0.25f,
        .size_z = 0.50f,
    };
    PresetObject authored_world = {
        .type = PRESET_OBJECT_BOX,
        .size_x = 3.0f,
        .size_y = 2.0f,
        .size_z = 1.5f,
    };
    SimRuntime3DFootprintHalfExtents half_extents = {0};

    if (!sim_runtime_3d_footprint_object_box_half_extents_cells(&desc, &normalized, &half_extents)) {
        return false;
    }
    if (half_extents.half_x_cells != 2) return false;
    if (half_extents.half_y_cells != 2) return false;
    if (half_extents.half_z_cells != 2) return false;

    if (!sim_runtime_3d_footprint_object_box_half_extents_cells(&desc, &authored_world, &half_extents)) {
        return false;
    }
    if (half_extents.half_x_cells != 6) return false;
    if (half_extents.half_y_cells != 4) return false;
    if (half_extents.half_z_cells != 3) return false;
    return true;
}

static bool test_import_box_uses_world_z_extent_not_emitter_radius(void) {
    SimRuntime3DDomainDesc desc = {
        .world_min_x = -5.0f,
        .world_max_x = 5.0f,
        .world_min_y = -2.0f,
        .world_max_y = 2.0f,
        .world_min_z = -1.0f,
        .world_max_z = 1.0f,
        .voxel_size = 0.25f,
    };
    ImportedShape imp = {
        .enabled = true,
        .scale = 1.0f,
    };
    SimRuntime3DFootprintHalfExtents half_extents = {0};

    if (!sim_runtime_3d_footprint_import_box_half_extents_cells(&desc, &imp, &half_extents)) {
        return false;
    }
    if (half_extents.half_x_cells != 5) return false;
    if (half_extents.half_y_cells != 2) return false;
    if (half_extents.half_z_cells != 1) return false;
    return true;
}

int main(void) {
    if (!test_emitter_radius_uses_world_span_and_voxel_size()) {
        fprintf(stderr, "sim_runtime_3d_footprint_contract_test: emitter radius sizing failed\n");
        return 1;
    }
    if (!test_object_box_half_extents_accept_normalized_and_world_sizes()) {
        fprintf(stderr, "sim_runtime_3d_footprint_contract_test: object box sizing failed\n");
        return 1;
    }
    if (!test_import_box_uses_world_z_extent_not_emitter_radius()) {
        fprintf(stderr, "sim_runtime_3d_footprint_contract_test: import box sizing failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_3d_footprint_contract_test: success\n");
    return 0;
}
