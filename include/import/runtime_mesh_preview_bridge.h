#ifndef PHYSICS_SIM_RUNTIME_MESH_PREVIEW_BRIDGE_H
#define PHYSICS_SIM_RUNTIME_MESH_PREVIEW_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_presets.h"
#include "core_mesh_preview.h"

#define PHYSICS_SIM_RUNTIME_MESH_PREVIEW_MAX_INSTANCES 64
#define PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX 4096

typedef struct PhysicsSimRuntimeMeshPreviewInstance {
    char object_id[64];
    char asset_id[64];
    int scene_object_index;
    bool runtime_path_resolved;
    bool preview_path_resolved;
    bool preview_file_exists;
    bool preview_file_readable;
    bool preview_schema_supported;
    bool preview_metadata_valid;
    bool has_world_bounds;
    bool fluid_obstacle_enabled;
    bool fluid_emitter_enabled;
    FluidEmitterType emitter_type;
    FluidEmitter3DSourceMode emitter_source_mode_3d;
    FluidEmitter3DSurface emitter_surface_3d;
    FluidEmitter3DObstacleMode emitter_obstacle_mode_3d;
    float emitter_strength;
    float emitter_radius;
    CoreObjectVec3 emitter_direction;
    float emitter_thermal_buoyancy_3d;
    CoreObjectVec3 transform_position;
    CoreObjectVec3 transform_rotation_deg;
    CoreObjectVec3 transform_scale;
    CoreObjectVec3 world_bounds_min;
    CoreObjectVec3 world_bounds_max;
    char fluid_behavior[32];
    char mesh_proxy_mode[32];
    bool runtime_path_recovered;
    char runtime_mesh_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    char runtime_mesh_path_hint[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    char preview_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    CoreMeshPreviewRuntimeMetadata metadata;
} PhysicsSimRuntimeMeshPreviewInstance;

typedef struct PhysicsSimRuntimeMeshPreviewSet {
    bool valid_contract;
    int instance_count;
    int invalid_reference_count;
    bool instance_capacity_clamped;
    char diagnostics[256];
    PhysicsSimRuntimeMeshPreviewInstance
        instances[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_MAX_INSTANCES];
} PhysicsSimRuntimeMeshPreviewSet;

void physics_sim_runtime_mesh_preview_set_init(PhysicsSimRuntimeMeshPreviewSet *set);

bool physics_sim_runtime_mesh_preview_scan_scene_json(
    const char *runtime_scene_json,
    const char *runtime_scene_path_hint,
    PhysicsSimRuntimeMeshPreviewSet *out_set,
    char *out_diagnostics,
    size_t out_diagnostics_size);

bool physics_sim_runtime_mesh_preview_scan_scene_file(
    const char *runtime_scene_path,
    PhysicsSimRuntimeMeshPreviewSet *out_set,
    char *out_diagnostics,
    size_t out_diagnostics_size);

#endif
