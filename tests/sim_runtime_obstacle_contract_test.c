#include "app/sim_runtime_obstacle.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool test_default_contract_freezes_first_pass_policy(void) {
    SimRuntimeObstacleContract contract = {0};
    int i = 0;

    sim_runtime_obstacle_contract_default(&contract);
    if (contract.storage_kind != SIM_RUNTIME_OBSTACLE_STORAGE_DENSE_OCCUPANCY_VOLUME) return false;
    if (contract.compatibility_policy != SIM_RUNTIME_OBSTACLE_COMPAT_DERIVED_XY_OCCUPANCY_SLICE) {
        return false;
    }
    for (i = 0; i < SIM_RUNTIME_BOUNDARY_FACE_COUNT; ++i) {
        if (!contract.domain_walls_enabled[i]) return false;
    }
    if (!contract.backend_generated_occupancy_supported) return false;
    if (!contract.retained_object_occupancy_supported) return false;
    if (!contract.retained_import_occupancy_supported) return false;
    if (contract.obstacle_velocity_support) return false;
    if (contract.obstacle_normal_support) return false;
    if (strcmp(sim_runtime_obstacle_storage_kind_label(contract.storage_kind),
               "dense-occupancy-volume") != 0) {
        return false;
    }
    if (strcmp(sim_runtime_obstacle_compatibility_policy_label(contract.compatibility_policy),
               "derived-xy-occupancy-slice") != 0) {
        return false;
    }
    return true;
}

static bool test_source_policy_freezes_supported_obstacle_sources(void) {
    SimRuntimeObstacleSourcePolicy policy = {0};

    if (!sim_runtime_obstacle_source_policy(SIM_RUNTIME_OBSTACLE_SOURCE_DOMAIN_WALL, &policy)) {
        return false;
    }
    if (policy.primary_footprint != SIM_RUNTIME_OBSTACLE_FOOTPRINT_DOMAIN_FACE_SLAB) return false;
    if (!policy.contributes_occupancy) return false;

    if (!sim_runtime_obstacle_source_policy(SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_OBJECT, &policy)) {
        return false;
    }
    if (policy.primary_footprint != SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_OBJECT_OCCUPANCY) {
        return false;
    }
    if (policy.fallback_footprint != SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY) {
        return false;
    }

    if (!sim_runtime_obstacle_source_policy(SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_IMPORT, &policy)) {
        return false;
    }
    if (policy.primary_footprint != SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_IMPORT_OCCUPANCY) {
        return false;
    }
    if (strcmp(sim_runtime_obstacle_source_kind_label(policy.source_kind), "retained-import") != 0) {
        return false;
    }
    if (strcmp(sim_runtime_obstacle_footprint_kind_label(policy.primary_footprint),
               "retained-import-occupancy") != 0) {
        return false;
    }
    return true;
}

static bool test_domain_face_bounds_cover_all_six_walls(void) {
    SimRuntime3DDomainDesc domain = {
        .grid_w = 8,
        .grid_h = 6,
        .grid_d = 4,
    };
    SimRuntimeObstacleBounds3D bounds = {0};

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MIN_X, &bounds)) {
        return false;
    }
    if (bounds.min_x != 0 || bounds.max_x != 0 || bounds.max_y != 5 || bounds.max_z != 3) {
        return false;
    }

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MAX_X, &bounds)) {
        return false;
    }
    if (bounds.min_x != 7 || bounds.max_x != 7) return false;

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MIN_Y, &bounds)) {
        return false;
    }
    if (bounds.min_y != 0 || bounds.max_y != 0 || bounds.max_x != 7) return false;

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MAX_Y, &bounds)) {
        return false;
    }
    if (bounds.min_y != 5 || bounds.max_y != 5) return false;

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MIN_Z, &bounds)) {
        return false;
    }
    if (bounds.min_z != 0 || bounds.max_z != 0 || bounds.max_x != 7) return false;

    if (!sim_runtime_obstacle_domain_face_bounds(&domain, SIM_RUNTIME_BOUNDARY_FACE_MAX_Z, &bounds)) {
        return false;
    }
    if (bounds.min_z != 3 || bounds.max_z != 3) return false;
    if (strcmp(sim_runtime_boundary_face_label(SIM_RUNTIME_BOUNDARY_FACE_MAX_Z), "max-z") != 0) {
        return false;
    }
    return true;
}

int main(void) {
    if (!test_default_contract_freezes_first_pass_policy()) {
        fprintf(stderr,
                "sim_runtime_obstacle_contract_test: default contract policy failed\n");
        return 1;
    }
    if (!test_source_policy_freezes_supported_obstacle_sources()) {
        fprintf(stderr,
                "sim_runtime_obstacle_contract_test: source policy failed\n");
        return 1;
    }
    if (!test_domain_face_bounds_cover_all_six_walls()) {
        fprintf(stderr,
                "sim_runtime_obstacle_contract_test: domain face bounds failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_obstacle_contract_test: success\n");
    return 0;
}
