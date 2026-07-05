#ifndef SIM_RUNTIME_MESH_ACCEL_H
#define SIM_RUNTIME_MESH_ACCEL_H

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"

typedef struct PhysicsSimRuntimeMeshAccel {
    const CoreMeshAssetRuntimeDocument *document;
    const CoreObjectVec3 *world_vertices;
    void *triangle_entries;
    void *nodes;
    size_t triangle_count;
    size_t node_count;
    size_t leaf_count;
    size_t max_depth;
    char diagnostics[128];
} PhysicsSimRuntimeMeshAccel;

typedef struct PhysicsSimRuntimeMeshAccelStats {
    bool built;
    size_t triangle_count;
    size_t node_count;
    size_t leaf_count;
    size_t max_depth;
    char diagnostics[128];
} PhysicsSimRuntimeMeshAccelStats;

void physics_sim_runtime_mesh_accel_init(PhysicsSimRuntimeMeshAccel *accel);
void physics_sim_runtime_mesh_accel_free(PhysicsSimRuntimeMeshAccel *accel);
bool physics_sim_runtime_mesh_accel_build(
    PhysicsSimRuntimeMeshAccel *accel,
    const CoreMeshAssetRuntimeDocument *document,
    const CoreObjectVec3 *world_vertices);
int physics_sim_runtime_mesh_accel_count_ray_intersections(
    const PhysicsSimRuntimeMeshAccel *accel,
    CoreObjectVec3 origin,
    CoreObjectVec3 direction);
void physics_sim_runtime_mesh_accel_stats(
    const PhysicsSimRuntimeMeshAccel *accel,
    PhysicsSimRuntimeMeshAccelStats *out_stats);

#endif
