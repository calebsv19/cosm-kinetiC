#ifndef SIM_RUNTIME_MESH_OBSTACLE_PROXY_H
#define SIM_RUNTIME_MESH_OBSTACLE_PROXY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_3d_domain.h"
#include "import/runtime_mesh_preview_bridge.h"

#define PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_MAX_GRID_TEST_CELLS 1000000u

typedef enum PhysicsSimRuntimeMeshVoxelizeMode {
    PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SOLID_VOLUME = 0,
    PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL = 1
} PhysicsSimRuntimeMeshVoxelizeMode;

typedef struct PhysicsSimRuntimeMeshObstacleReport {
    bool loaded_runtime_mesh;
    bool used_surface_shell;
    bool used_closed_volume_fill;
    bool used_prepared_cache;
    bool prepared_cache_hit;
    bool prepared_cache_stale_refresh;
    bool used_voxel_footprint_cache;
    bool voxel_footprint_cache_hit;
    bool file_signature_valid;
    bool budget_limited;
    bool has_local_bounds;
    bool has_world_bounds;
    CoreObjectVec3 local_bounds_min;
    CoreObjectVec3 local_bounds_max;
    CoreObjectVec3 world_bounds_min;
    CoreObjectVec3 world_bounds_max;
    size_t runtime_vertex_count;
    size_t runtime_triangle_count;
    uint64_t runtime_mesh_file_mtime_ns;
    uint64_t runtime_mesh_file_size_bytes;
    bool used_triangle_accel;
    size_t accel_triangle_count;
    size_t accel_node_count;
    size_t accel_leaf_count;
    size_t accel_max_depth;
    size_t tested_cell_count;
    size_t solid_cell_count;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
    double wind_projected_area_yz;
    double bounds_volume;
    char fallback_reason[128];
    char diagnostics[128];
} PhysicsSimRuntimeMeshObstacleReport;

typedef struct PhysicsSimRuntimeMeshObstacleCache PhysicsSimRuntimeMeshObstacleCache;

typedef struct PhysicsSimRuntimeMeshObstacleCacheStats {
    size_t prepared_entry_count;
    size_t cache_hit_count;
    size_t cache_miss_count;
    size_t stale_refresh_count;
    size_t eviction_count;
    size_t voxel_footprint_entry_count;
    size_t voxel_footprint_cache_hit_count;
    size_t voxel_footprint_cache_miss_count;
    size_t voxel_footprint_eviction_count;
} PhysicsSimRuntimeMeshObstacleCacheStats;

PhysicsSimRuntimeMeshObstacleCache *physics_sim_runtime_mesh_obstacle_cache_create(void);
void physics_sim_runtime_mesh_obstacle_cache_destroy(
    PhysicsSimRuntimeMeshObstacleCache *cache);
void physics_sim_runtime_mesh_obstacle_cache_clear(
    PhysicsSimRuntimeMeshObstacleCache *cache);
void physics_sim_runtime_mesh_obstacle_cache_stats(
    const PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstacleCacheStats *out_stats);
void physics_sim_runtime_mesh_obstacle_report_init(
    PhysicsSimRuntimeMeshObstacleReport *report);
bool physics_sim_runtime_mesh_obstacle_instance_enabled(
    const PhysicsSimRuntimeMeshPreviewInstance *instance);
bool physics_sim_runtime_mesh_obstacle_voxelize_instance(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshObstacleReport *out_report);
bool physics_sim_runtime_mesh_obstacle_voxelize_instance_mode(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    PhysicsSimRuntimeMeshObstacleReport *out_report);
bool physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshObstacleReport *out_report);
bool physics_sim_runtime_mesh_obstacle_voxelize_instance_mode_cached(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    PhysicsSimRuntimeMeshObstacleReport *out_report);

#endif
