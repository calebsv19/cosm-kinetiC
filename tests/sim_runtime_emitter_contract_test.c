#include "app/sim_runtime_emitter.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nearly_equal(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0001f;
}

static bool test_free_emitter_resolution_normalizes_direction(void) {
    FluidScenePreset preset = {0};
    SimRuntimeEmitterResolved resolved = {0};

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.25f,
        .position_y = 0.5f,
        .position_z = 0.75f,
        .radius = 0.08f,
        .strength = 12.0f,
        .dir_x = 0.0f,
        .dir_y = 3.0f,
        .dir_z = 4.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    if (!sim_runtime_emitter_resolve(&preset, 0, &resolved)) return false;
    if (resolved.source_kind != SIM_RUNTIME_EMITTER_SOURCE_FREE) return false;
    if (resolved.primary_footprint != SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE) return false;
    if (!resolved.direction_has_magnitude) return false;
    if (!nearly_equal(resolved.dir_y, 0.6f)) return false;
    if (!nearly_equal(resolved.dir_z, 0.8f)) return false;
    if (!nearly_equal(resolved.position_z, 0.75f)) return false;
    return true;
}

static bool test_import_attachment_takes_source_precedence(void) {
    FluidScenePreset preset = {0};
    SimRuntimeEmitterResolved resolved = {0};

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = -0.5f,
        .position_y = 1.5f,
        .position_z = 2.0f,
        .radius = 0.0f,
        .strength = 4.0f,
        .attached_object = 3,
        .attached_import = 6,
    };

    if (!sim_runtime_emitter_resolve(&preset, 0, &resolved)) return false;
    if (resolved.source_kind != SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT) return false;
    if (resolved.primary_footprint != SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_OCCUPANCY) return false;
    if (resolved.fallback_footprint != SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE) return false;
    if (!nearly_equal(resolved.position_x, 0.0f)) return false;
    if (!nearly_equal(resolved.position_y, 1.0f)) return false;
    if (!nearly_equal(resolved.position_z, 2.0f)) return false;
    return true;
}

static bool test_3d_placement_maps_additive_world_z(void) {
    SimRuntime3DDomainDesc domain = {
        .grid_w = 96,
        .grid_h = 80,
        .grid_d = 52,
        .world_min_z = -2.5f,
        .voxel_size = 0.125f,
    };
    SimRuntimeEmitterResolved emitter = {
        .position_x = 0.98f,
        .position_y = 0.02f,
        .position_z = 0.25f,
        .radius = 0.10f,
    };
    SimRuntimeEmitterPlacement3D placement = {0};

    if (!sim_runtime_emitter_resolve_3d_placement(&domain, &emitter, &placement)) return false;
    if (placement.center_x != 93) return false;
    if (placement.center_y != 2) return false;
    if (placement.center_z != 22) return false;
    if (placement.radius_cells != 10) return false;
    if (placement.max_x != 95) return false;
    if (placement.min_y != 0) return false;
    if (placement.max_z != 32) return false;
    return true;
}

int main(void) {
    if (!test_free_emitter_resolution_normalizes_direction()) {
        fprintf(stderr, "sim_runtime_emitter_contract_test: free emitter resolution failed\n");
        return 1;
    }
    if (!test_import_attachment_takes_source_precedence()) {
        fprintf(stderr, "sim_runtime_emitter_contract_test: attachment precedence failed\n");
        return 1;
    }
    if (!test_3d_placement_maps_additive_world_z()) {
        fprintf(stderr, "sim_runtime_emitter_contract_test: 3d placement failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_emitter_contract_test: success\n");
    return 0;
}
