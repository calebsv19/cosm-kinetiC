#ifndef PHYSICS_SIM_RUNTIME_MESH_DIAGNOSTICS_H
#define PHYSICS_SIM_RUNTIME_MESH_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_mesh_obstacle_proxy.h"

typedef enum PhysicsSimRuntimeMeshFluidRole {
    PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED = 0,
    PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_SOLID,
    PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_VISUAL_ONLY,
    PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_EMITTER
} PhysicsSimRuntimeMeshFluidRole;

typedef struct PhysicsSimRuntimeMeshDiagnostic {
    bool valid;
    int instance_index;
    int scene_object_index;
    PhysicsSimRuntimeMeshFluidRole role;
    char role_label[32];
    char object_id[64];
    char asset_id[64];
    char runtime_mesh_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    char runtime_mesh_path_hint[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    char preview_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    bool runtime_path_resolved;
    bool runtime_path_recovered;
    CoreObjectVec3 transform_position;
    CoreObjectVec3 transform_rotation_deg;
    CoreObjectVec3 transform_scale;
    bool has_local_bounds;
    bool has_world_bounds;
    CoreObjectVec3 local_bounds_min;
    CoreObjectVec3 local_bounds_max;
    CoreObjectVec3 world_bounds_min;
    CoreObjectVec3 world_bounds_max;
    size_t runtime_vertex_count;
    size_t runtime_triangle_count;
    bool file_signature_valid;
    uint64_t runtime_mesh_file_mtime_ns;
    uint64_t runtime_mesh_file_size_bytes;
    bool used_triangle_accel;
    size_t accel_triangle_count;
    size_t accel_node_count;
    size_t accel_leaf_count;
    size_t accel_max_depth;
    size_t obstacle_voxel_count;
    size_t emitter_footprint_voxel_count;
    bool domain_overlap_tested;
    bool world_bounds_intersect_domain;
    bool world_bounds_inside_domain;
    bool budget_limited;
    double wind_projected_area_yz;
    double bounds_volume;
    char fallback_reason[128];
    char diagnostics[256];
} PhysicsSimRuntimeMeshDiagnostic;

const char *physics_sim_runtime_mesh_fluid_role_label(PhysicsSimRuntimeMeshFluidRole role);

void physics_sim_runtime_mesh_diagnostic_init(PhysicsSimRuntimeMeshDiagnostic *diag);

bool physics_sim_runtime_mesh_diagnostic_collect(
    const PhysicsSimRuntimeMeshPreviewSet *set,
    int instance_index,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshDiagnostic *out_diag);

bool physics_sim_runtime_mesh_diagnostic_collect_instance(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    int instance_index,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshDiagnostic *out_diag);

#endif
