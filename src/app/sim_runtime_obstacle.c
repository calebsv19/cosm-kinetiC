#include "app/sim_runtime_obstacle.h"

#include <string.h>

void sim_runtime_obstacle_contract_default(SimRuntimeObstacleContract *out_contract) {
    SimRuntimeObstacleContract contract = {0};
    int i = 0;
    if (!out_contract) return;
    contract.storage_kind = SIM_RUNTIME_OBSTACLE_STORAGE_DENSE_OCCUPANCY_VOLUME;
    contract.compatibility_policy = SIM_RUNTIME_OBSTACLE_COMPAT_DERIVED_XY_OCCUPANCY_SLICE;
    for (i = 0; i < SIM_RUNTIME_BOUNDARY_FACE_COUNT; ++i) {
        contract.domain_walls_enabled[i] = true;
    }
    contract.backend_generated_occupancy_supported = true;
    contract.retained_object_occupancy_supported = true;
    contract.retained_import_occupancy_supported = true;
    contract.obstacle_velocity_support = false;
    contract.obstacle_normal_support = false;
    *out_contract = contract;
}

bool sim_runtime_obstacle_source_policy(SimRuntimeObstacleSourceKind kind,
                                        SimRuntimeObstacleSourcePolicy *out_policy) {
    SimRuntimeObstacleSourcePolicy policy = {0};
    if (!out_policy) return false;

    policy.source_kind = kind;
    policy.contributes_occupancy = true;

    switch (kind) {
    case SIM_RUNTIME_OBSTACLE_SOURCE_DOMAIN_WALL:
        policy.primary_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_DOMAIN_FACE_SLAB;
        policy.fallback_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_DOMAIN_FACE_SLAB;
        break;
    case SIM_RUNTIME_OBSTACLE_SOURCE_BACKEND_GENERATED:
        policy.primary_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY;
        policy.fallback_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY;
        break;
    case SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_OBJECT:
        policy.primary_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_OBJECT_OCCUPANCY;
        policy.fallback_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY;
        break;
    case SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_IMPORT:
        policy.primary_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_IMPORT_OCCUPANCY;
        policy.fallback_footprint = SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY;
        break;
    default:
        return false;
    }

    *out_policy = policy;
    return true;
}

bool sim_runtime_obstacle_domain_face_bounds(const SimRuntime3DDomainDesc *domain,
                                             SimRuntimeBoundaryFace face,
                                             SimRuntimeObstacleBounds3D *out_bounds) {
    SimRuntimeObstacleBounds3D bounds = {0};
    if (!domain || !out_bounds) return false;
    if (domain->grid_w <= 0 || domain->grid_h <= 0 || domain->grid_d <= 0) return false;

    bounds.min_x = 0;
    bounds.max_x = domain->grid_w - 1;
    bounds.min_y = 0;
    bounds.max_y = domain->grid_h - 1;
    bounds.min_z = 0;
    bounds.max_z = domain->grid_d - 1;

    switch (face) {
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_X:
        bounds.max_x = 0;
        break;
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_X:
        bounds.min_x = domain->grid_w - 1;
        break;
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_Y:
        bounds.max_y = 0;
        break;
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_Y:
        bounds.min_y = domain->grid_h - 1;
        break;
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_Z:
        bounds.max_z = 0;
        break;
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_Z:
        bounds.min_z = domain->grid_d - 1;
        break;
    default:
        return false;
    }

    *out_bounds = bounds;
    return true;
}

const char *sim_runtime_obstacle_storage_kind_label(SimRuntimeObstacleStorageKind kind) {
    switch (kind) {
    case SIM_RUNTIME_OBSTACLE_STORAGE_DENSE_OCCUPANCY_VOLUME:
        return "dense-occupancy-volume";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_obstacle_compatibility_policy_label(
    SimRuntimeObstacleCompatibilityPolicy policy) {
    switch (policy) {
    case SIM_RUNTIME_OBSTACLE_COMPAT_DERIVED_XY_OCCUPANCY_SLICE:
        return "derived-xy-occupancy-slice";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_boundary_face_label(SimRuntimeBoundaryFace face) {
    switch (face) {
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_X:
        return "min-x";
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_X:
        return "max-x";
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_Y:
        return "min-y";
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_Y:
        return "max-y";
    case SIM_RUNTIME_BOUNDARY_FACE_MIN_Z:
        return "min-z";
    case SIM_RUNTIME_BOUNDARY_FACE_MAX_Z:
        return "max-z";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_obstacle_source_kind_label(SimRuntimeObstacleSourceKind kind) {
    switch (kind) {
    case SIM_RUNTIME_OBSTACLE_SOURCE_DOMAIN_WALL:
        return "domain-wall";
    case SIM_RUNTIME_OBSTACLE_SOURCE_BACKEND_GENERATED:
        return "backend-generated";
    case SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_OBJECT:
        return "retained-object";
    case SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_IMPORT:
        return "retained-import";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_obstacle_footprint_kind_label(SimRuntimeObstacleFootprintKind kind) {
    switch (kind) {
    case SIM_RUNTIME_OBSTACLE_FOOTPRINT_DOMAIN_FACE_SLAB:
        return "domain-face-slab";
    case SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY:
        return "backend-generated-occupancy";
    case SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_OBJECT_OCCUPANCY:
        return "retained-object-occupancy";
    case SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_IMPORT_OCCUPANCY:
        return "retained-import-occupancy";
    default:
        break;
    }
    return "unknown";
}
