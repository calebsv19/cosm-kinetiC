#ifndef SIM_RUNTIME_OBSTACLE_H
#define SIM_RUNTIME_OBSTACLE_H

#include <stdbool.h>

#include "app/sim_runtime_3d_domain.h"

typedef enum SimRuntimeObstacleStorageKind {
    SIM_RUNTIME_OBSTACLE_STORAGE_DENSE_OCCUPANCY_VOLUME = 0
} SimRuntimeObstacleStorageKind;

typedef enum SimRuntimeObstacleCompatibilityPolicy {
    SIM_RUNTIME_OBSTACLE_COMPAT_DERIVED_XY_OCCUPANCY_SLICE = 0
} SimRuntimeObstacleCompatibilityPolicy;

typedef enum SimRuntimeBoundaryFace {
    SIM_RUNTIME_BOUNDARY_FACE_MIN_X = 0,
    SIM_RUNTIME_BOUNDARY_FACE_MAX_X = 1,
    SIM_RUNTIME_BOUNDARY_FACE_MIN_Y = 2,
    SIM_RUNTIME_BOUNDARY_FACE_MAX_Y = 3,
    SIM_RUNTIME_BOUNDARY_FACE_MIN_Z = 4,
    SIM_RUNTIME_BOUNDARY_FACE_MAX_Z = 5,
    SIM_RUNTIME_BOUNDARY_FACE_COUNT = 6
} SimRuntimeBoundaryFace;

typedef enum SimRuntimeObstacleSourceKind {
    SIM_RUNTIME_OBSTACLE_SOURCE_DOMAIN_WALL = 0,
    SIM_RUNTIME_OBSTACLE_SOURCE_BACKEND_GENERATED = 1,
    SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_OBJECT = 2,
    SIM_RUNTIME_OBSTACLE_SOURCE_RETAINED_IMPORT = 3
} SimRuntimeObstacleSourceKind;

typedef enum SimRuntimeObstacleFootprintKind {
    SIM_RUNTIME_OBSTACLE_FOOTPRINT_DOMAIN_FACE_SLAB = 0,
    SIM_RUNTIME_OBSTACLE_FOOTPRINT_BACKEND_GENERATED_OCCUPANCY = 1,
    SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_OBJECT_OCCUPANCY = 2,
    SIM_RUNTIME_OBSTACLE_FOOTPRINT_RETAINED_IMPORT_OCCUPANCY = 3
} SimRuntimeObstacleFootprintKind;

typedef struct SimRuntimeObstacleContract {
    SimRuntimeObstacleStorageKind storage_kind;
    SimRuntimeObstacleCompatibilityPolicy compatibility_policy;
    bool domain_walls_enabled[SIM_RUNTIME_BOUNDARY_FACE_COUNT];
    bool backend_generated_occupancy_supported;
    bool retained_object_occupancy_supported;
    bool retained_import_occupancy_supported;
    bool obstacle_velocity_support;
    bool obstacle_normal_support;
} SimRuntimeObstacleContract;

typedef struct SimRuntimeObstacleSourcePolicy {
    SimRuntimeObstacleSourceKind source_kind;
    SimRuntimeObstacleFootprintKind primary_footprint;
    SimRuntimeObstacleFootprintKind fallback_footprint;
    bool contributes_occupancy;
} SimRuntimeObstacleSourcePolicy;

typedef struct SimRuntimeObstacleBounds3D {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
} SimRuntimeObstacleBounds3D;

void sim_runtime_obstacle_contract_default(SimRuntimeObstacleContract *out_contract);
bool sim_runtime_obstacle_source_policy(SimRuntimeObstacleSourceKind kind,
                                        SimRuntimeObstacleSourcePolicy *out_policy);
bool sim_runtime_obstacle_domain_face_bounds(const SimRuntime3DDomainDesc *domain,
                                             SimRuntimeBoundaryFace face,
                                             SimRuntimeObstacleBounds3D *out_bounds);

const char *sim_runtime_obstacle_storage_kind_label(SimRuntimeObstacleStorageKind kind);
const char *sim_runtime_obstacle_compatibility_policy_label(
    SimRuntimeObstacleCompatibilityPolicy policy);
const char *sim_runtime_boundary_face_label(SimRuntimeBoundaryFace face);
const char *sim_runtime_obstacle_source_kind_label(SimRuntimeObstacleSourceKind kind);
const char *sim_runtime_obstacle_footprint_kind_label(SimRuntimeObstacleFootprintKind kind);

#endif // SIM_RUNTIME_OBSTACLE_H
