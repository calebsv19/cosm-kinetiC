#include "app/sim_runtime_mesh_obstacle_proxy.h"

#include "app/sim_runtime_3d_space.h"
#include "app/sim_runtime_mesh_accel.h"
#include "core_mesh_asset.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES 64u
#define PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY 4u

typedef struct MeshProxyBounds {
    CoreObjectVec3 min;
    CoreObjectVec3 max;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
    bool valid;
} MeshProxyBounds;

typedef struct PhysicsSimRuntimeMeshObstacleFootprintEntry {
    bool valid;
    SimRuntime3DDomainDesc domain;
    PhysicsSimRuntimeMeshVoxelizeMode mode;
    size_t *solid_indices;
    size_t solid_index_count;
    PhysicsSimRuntimeMeshObstacleReport stencil_report;
} PhysicsSimRuntimeMeshObstacleFootprintEntry;

typedef struct PhysicsSimRuntimeMeshObstaclePreparedEntry {
    bool valid;
    bool document_initialized;
    bool accel_built;
    char runtime_mesh_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    CoreObjectVec3 transform_position;
    CoreObjectVec3 transform_rotation_deg;
    CoreObjectVec3 transform_scale;
    bool runtime_mesh_signature_valid;
    uint64_t runtime_mesh_mtime_ns;
    uint64_t runtime_mesh_size_bytes;
    CoreMeshAssetRuntimeDocument document;
    CoreObjectVec3 *vertices;
    MeshProxyBounds bounds;
    PhysicsSimRuntimeMeshAccel accel;
    PhysicsSimRuntimeMeshObstacleFootprintEntry
        footprints[PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY];
    size_t footprint_count;
    size_t next_footprint_evict_index;
} PhysicsSimRuntimeMeshObstaclePreparedEntry;

struct PhysicsSimRuntimeMeshObstacleCache {
    PhysicsSimRuntimeMeshObstaclePreparedEntry
        entries[PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES];
    size_t prepared_entry_count;
    size_t cache_hit_count;
    size_t cache_miss_count;
    size_t stale_refresh_count;
    size_t eviction_count;
    size_t voxel_footprint_entry_count;
    size_t voxel_footprint_cache_hit_count;
    size_t voxel_footprint_cache_miss_count;
    size_t voxel_footprint_eviction_count;
    size_t next_evict_index;
};

static void mesh_proxy_diag(PhysicsSimRuntimeMeshObstacleReport *report,
                            const char *message) {
    if (!report || !message) return;
    snprintf(report->diagnostics, sizeof(report->diagnostics), "%s", message);
}

static void mesh_proxy_fallback_reason(PhysicsSimRuntimeMeshObstacleReport *report,
                                       const char *message) {
    if (!report || !message) return;
    snprintf(report->fallback_reason, sizeof(report->fallback_reason), "%s", message);
}

void physics_sim_runtime_mesh_obstacle_report_init(
    PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->min_x = report->min_y = report->min_z = 0;
    report->max_x = report->max_y = report->max_z = -1;
    mesh_proxy_diag(report, "ok");
}

static void mesh_proxy_prepared_entry_init(PhysicsSimRuntimeMeshObstaclePreparedEntry *entry) {
    if (!entry) return;
    memset(entry, 0, sizeof(*entry));
    core_mesh_asset_runtime_document_init(&entry->document);
    entry->document_initialized = true;
    physics_sim_runtime_mesh_accel_init(&entry->accel);
}

static void mesh_proxy_footprint_entry_clear(
    PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint) {
    if (!footprint) return;
    free(footprint->solid_indices);
    memset(footprint, 0, sizeof(*footprint));
}

static void mesh_proxy_prepared_entry_clear(PhysicsSimRuntimeMeshObstaclePreparedEntry *entry) {
    if (!entry) return;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY; ++i) {
        mesh_proxy_footprint_entry_clear(&entry->footprints[i]);
    }
    free(entry->vertices);
    entry->vertices = NULL;
    physics_sim_runtime_mesh_accel_free(&entry->accel);
    if (entry->document_initialized) {
        core_mesh_asset_runtime_document_free(&entry->document);
    }
    mesh_proxy_prepared_entry_init(entry);
}

PhysicsSimRuntimeMeshObstacleCache *physics_sim_runtime_mesh_obstacle_cache_create(void) {
    PhysicsSimRuntimeMeshObstacleCache *cache =
        (PhysicsSimRuntimeMeshObstacleCache *)calloc(1, sizeof(*cache));
    if (!cache) return NULL;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES; ++i) {
        mesh_proxy_prepared_entry_init(&cache->entries[i]);
    }
    return cache;
}

void physics_sim_runtime_mesh_obstacle_cache_clear(
    PhysicsSimRuntimeMeshObstacleCache *cache) {
    if (!cache) return;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES; ++i) {
        mesh_proxy_prepared_entry_clear(&cache->entries[i]);
    }
    cache->prepared_entry_count = 0u;
    cache->cache_hit_count = 0u;
    cache->cache_miss_count = 0u;
    cache->stale_refresh_count = 0u;
    cache->eviction_count = 0u;
    cache->voxel_footprint_entry_count = 0u;
    cache->voxel_footprint_cache_hit_count = 0u;
    cache->voxel_footprint_cache_miss_count = 0u;
    cache->voxel_footprint_eviction_count = 0u;
    cache->next_evict_index = 0u;
}

void physics_sim_runtime_mesh_obstacle_cache_destroy(
    PhysicsSimRuntimeMeshObstacleCache *cache) {
    if (!cache) return;
    physics_sim_runtime_mesh_obstacle_cache_clear(cache);
    free(cache);
}

void physics_sim_runtime_mesh_obstacle_cache_stats(
    const PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstacleCacheStats *out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!cache) return;
    out_stats->prepared_entry_count = cache->prepared_entry_count;
    out_stats->cache_hit_count = cache->cache_hit_count;
    out_stats->cache_miss_count = cache->cache_miss_count;
    out_stats->stale_refresh_count = cache->stale_refresh_count;
    out_stats->eviction_count = cache->eviction_count;
    out_stats->voxel_footprint_entry_count = cache->voxel_footprint_entry_count;
    out_stats->voxel_footprint_cache_hit_count = cache->voxel_footprint_cache_hit_count;
    out_stats->voxel_footprint_cache_miss_count = cache->voxel_footprint_cache_miss_count;
    out_stats->voxel_footprint_eviction_count = cache->voxel_footprint_eviction_count;
}

bool physics_sim_runtime_mesh_obstacle_instance_enabled(
    const PhysicsSimRuntimeMeshPreviewInstance *instance) {
    if (!instance) return false;
    if (instance->fluid_emitter_enabled) return false;
    if (!instance->fluid_obstacle_enabled) return false;
    if (strcmp(instance->fluid_behavior, "visual_only") == 0 ||
        strcmp(instance->fluid_behavior, "none") == 0) {
        return false;
    }
    return instance->runtime_path_resolved && instance->runtime_mesh_path[0];
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static CoreObjectVec3 rotate_xyz_degrees(CoreObjectVec3 value, CoreObjectVec3 rotation_deg) {
    const double rx = rotation_deg.x * M_PI / 180.0;
    const double ry = rotation_deg.y * M_PI / 180.0;
    const double rz = rotation_deg.z * M_PI / 180.0;
    const double cx = cos(rx);
    const double sx = sin(rx);
    const double cy = cos(ry);
    const double sy = sin(ry);
    const double cz = cos(rz);
    const double sz = sin(rz);
    CoreObjectVec3 r = value;
    CoreObjectVec3 tmp = r;

    tmp.y = r.y * cx - r.z * sx;
    tmp.z = r.y * sx + r.z * cx;
    r = tmp;
    tmp = r;
    tmp.x = r.x * cy + r.z * sy;
    tmp.z = -r.x * sy + r.z * cy;
    r = tmp;
    tmp = r;
    tmp.x = r.x * cz - r.y * sz;
    tmp.y = r.x * sz + r.y * cz;
    return tmp;
}

static CoreObjectVec3 transform_mesh_vertex(CoreObjectVec3 local,
                                            const PhysicsSimRuntimeMeshPreviewInstance *instance) {
    CoreObjectVec3 scaled = {
        local.x * instance->transform_scale.x,
        local.y * instance->transform_scale.y,
        local.z * instance->transform_scale.z
    };
    CoreObjectVec3 world = rotate_xyz_degrees(scaled, instance->transform_rotation_deg);
    world.x += instance->transform_position.x;
    world.y += instance->transform_position.y;
    world.z += instance->transform_position.z;
    return world;
}

static bool vec3_finite(CoreObjectVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static double cell_center_axis(double world_min, double voxel_size, int cell) {
    return world_min + ((double)cell + 0.5) * voxel_size;
}

static void mark_mask_cell(const SimRuntime3DDomainDesc *domain,
                           uint8_t *solid_mask,
                           PhysicsSimRuntimeMeshObstacleReport *report,
                           int x,
                           int y,
                           int z) {
    size_t idx = 0u;
    if (!domain || !solid_mask) return;
    if (x < 0 || y < 0 || z < 0 ||
        x >= domain->grid_w || y >= domain->grid_h || z >= domain->grid_d) {
        return;
    }
    idx = sim_runtime_3d_volume_index(domain, x, y, z);
    if (!solid_mask[idx]) {
        solid_mask[idx] = 1u;
        if (report) report->solid_cell_count += 1u;
    }
}

static void mark_mask_index(uint8_t *solid_mask,
                            size_t solid_mask_count,
                            size_t index,
                            PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!solid_mask || index >= solid_mask_count) return;
    if (!solid_mask[index]) {
        solid_mask[index] = 1u;
        if (report) report->solid_cell_count += 1u;
    }
}

static bool compute_world_vertices(const CoreMeshAssetRuntimeDocument *document,
                                   const PhysicsSimRuntimeMeshPreviewInstance *instance,
                                   CoreObjectVec3 **out_vertices,
                                   MeshProxyBounds *out_bounds) {
    CoreObjectVec3 *vertices = NULL;
    MeshProxyBounds bounds = {0};
    if (!document || !instance || !out_vertices || !out_bounds ||
        document->vertex_count == 0u || !document->vertices) {
        return false;
    }
    vertices = (CoreObjectVec3 *)calloc(document->vertex_count, sizeof(CoreObjectVec3));
    if (!vertices) return false;

    for (size_t i = 0u; i < document->vertex_count; ++i) {
        CoreObjectVec3 world = transform_mesh_vertex(document->vertices[i].position, instance);
        if (!vec3_finite(world)) {
            free(vertices);
            return false;
        }
        vertices[i] = world;
        if (i == 0u) {
            bounds.min = world;
            bounds.max = world;
        } else {
            if (world.x < bounds.min.x) bounds.min.x = world.x;
            if (world.y < bounds.min.y) bounds.min.y = world.y;
            if (world.z < bounds.min.z) bounds.min.z = world.z;
            if (world.x > bounds.max.x) bounds.max.x = world.x;
            if (world.y > bounds.max.y) bounds.max.y = world.y;
            if (world.z > bounds.max.z) bounds.max.z = world.z;
        }
    }
    bounds.valid = true;
    *out_vertices = vertices;
    *out_bounds = bounds;
    return true;
}

static bool mesh_proxy_same_transform(const PhysicsSimRuntimeMeshObstaclePreparedEntry *entry,
                                      const PhysicsSimRuntimeMeshPreviewInstance *instance) {
    return entry && instance &&
           entry->transform_position.x == instance->transform_position.x &&
           entry->transform_position.y == instance->transform_position.y &&
           entry->transform_position.z == instance->transform_position.z &&
           entry->transform_rotation_deg.x == instance->transform_rotation_deg.x &&
           entry->transform_rotation_deg.y == instance->transform_rotation_deg.y &&
           entry->transform_rotation_deg.z == instance->transform_rotation_deg.z &&
           entry->transform_scale.x == instance->transform_scale.x &&
           entry->transform_scale.y == instance->transform_scale.y &&
           entry->transform_scale.z == instance->transform_scale.z;
}

static bool mesh_proxy_file_signature(const char *path,
                                      uint64_t *out_mtime_ns,
                                      uint64_t *out_size_bytes) {
    struct stat st;
    uint64_t sec = 0u;
    if (out_mtime_ns) *out_mtime_ns = 0u;
    if (out_size_bytes) *out_size_bytes = 0u;
    if (!path || !path[0] || stat(path, &st) != 0) return false;
    sec = (uint64_t)st.st_mtime;
    if (out_mtime_ns) *out_mtime_ns = sec * 1000000000ull;
    if (out_size_bytes) {
        *out_size_bytes = (st.st_size > 0) ? (uint64_t)st.st_size : 0u;
    }
    return true;
}

static bool mesh_proxy_same_file_signature(
    const PhysicsSimRuntimeMeshObstaclePreparedEntry *entry,
    bool signature_valid,
    uint64_t mtime_ns,
    uint64_t size_bytes) {
    if (!entry) return false;
    if (!entry->runtime_mesh_signature_valid || !signature_valid) return true;
    return entry->runtime_mesh_mtime_ns == mtime_ns &&
           entry->runtime_mesh_size_bytes == size_bytes;
}

static bool mesh_proxy_same_domain_key(const SimRuntime3DDomainDesc *a,
                                       const SimRuntime3DDomainDesc *b) {
    return a && b &&
           a->grid_w == b->grid_w &&
           a->grid_h == b->grid_h &&
           a->grid_d == b->grid_d &&
           a->cell_count == b->cell_count &&
           a->world_min_x == b->world_min_x &&
           a->world_min_y == b->world_min_y &&
           a->world_min_z == b->world_min_z &&
           a->world_max_x == b->world_max_x &&
           a->world_max_y == b->world_max_y &&
           a->world_max_z == b->world_max_z &&
           a->voxel_size == b->voxel_size;
}

static PhysicsSimRuntimeMeshObstaclePreparedEntry *mesh_proxy_cache_find(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    bool signature_valid,
    uint64_t mtime_ns,
    uint64_t size_bytes,
    PhysicsSimRuntimeMeshObstaclePreparedEntry **out_stale_entry) {
    if (out_stale_entry) *out_stale_entry = NULL;
    if (!cache || !instance || !instance->runtime_mesh_path[0]) return NULL;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES; ++i) {
        PhysicsSimRuntimeMeshObstaclePreparedEntry *entry = &cache->entries[i];
        if (!entry->valid) continue;
        if (strcmp(entry->runtime_mesh_path, instance->runtime_mesh_path) != 0) continue;
        if (!mesh_proxy_same_transform(entry, instance)) continue;
        if (!mesh_proxy_same_file_signature(entry, signature_valid, mtime_ns, size_bytes)) {
            if (out_stale_entry && !*out_stale_entry) *out_stale_entry = entry;
            continue;
        }
        return entry;
    }
    return NULL;
}

static PhysicsSimRuntimeMeshObstaclePreparedEntry *mesh_proxy_cache_allocate_entry(
    PhysicsSimRuntimeMeshObstacleCache *cache) {
    if (!cache) return NULL;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES; ++i) {
        if (!cache->entries[i].valid) {
            cache->prepared_entry_count++;
            return &cache->entries[i];
        }
    }
    if (cache->next_evict_index >= PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_CACHE_MAX_ENTRIES) {
        cache->next_evict_index = 0u;
    }
    if (cache->voxel_footprint_entry_count >=
        cache->entries[cache->next_evict_index].footprint_count) {
        cache->voxel_footprint_entry_count -=
            cache->entries[cache->next_evict_index].footprint_count;
    } else {
        cache->voxel_footprint_entry_count = 0u;
    }
    mesh_proxy_prepared_entry_clear(&cache->entries[cache->next_evict_index]);
    cache->eviction_count++;
    return &cache->entries[cache->next_evict_index++];
}

static void mesh_proxy_cache_clear_entry_contents(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstaclePreparedEntry *entry) {
    if (!cache || !entry) return;
    if (cache->voxel_footprint_entry_count >= entry->footprint_count) {
        cache->voxel_footprint_entry_count -= entry->footprint_count;
    } else {
        cache->voxel_footprint_entry_count = 0u;
    }
    mesh_proxy_prepared_entry_clear(entry);
}

static bool mesh_proxy_prepare_entry(
    PhysicsSimRuntimeMeshObstaclePreparedEntry *entry,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    bool signature_valid,
    uint64_t mtime_ns,
    uint64_t size_bytes,
    PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!entry || !instance || !instance->runtime_mesh_path[0]) {
        mesh_proxy_diag(report, "invalid runtime mesh cache entry");
        return false;
    }
    mesh_proxy_prepared_entry_clear(entry);
    snprintf(entry->runtime_mesh_path,
             sizeof(entry->runtime_mesh_path),
             "%s",
             instance->runtime_mesh_path);
    entry->transform_position = instance->transform_position;
    entry->transform_rotation_deg = instance->transform_rotation_deg;
    entry->transform_scale = instance->transform_scale;
    entry->runtime_mesh_signature_valid = signature_valid;
    entry->runtime_mesh_mtime_ns = mtime_ns;
    entry->runtime_mesh_size_bytes = size_bytes;
    if (core_mesh_asset_runtime_document_load_file(entry->runtime_mesh_path, &entry->document).code != CORE_OK ||
        entry->document.vertex_count == 0u ||
        entry->document.triangle_count == 0u) {
        mesh_proxy_diag(report, "failed to load runtime mesh");
        return false;
    }
    if (!compute_world_vertices(&entry->document, instance, &entry->vertices, &entry->bounds)) {
        mesh_proxy_diag(report, "failed to transform runtime mesh bounds");
        return false;
    }
    if (entry->document.contract.topology_closed_volume) {
        entry->accel_built =
            physics_sim_runtime_mesh_accel_build(&entry->accel,
                                                 &entry->document,
                                                 entry->vertices);
    }
    entry->valid = true;
    return true;
}

static PhysicsSimRuntimeMeshObstaclePreparedEntry *mesh_proxy_prepare_instance(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstaclePreparedEntry *local_entry,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    PhysicsSimRuntimeMeshObstacleReport *report) {
    PhysicsSimRuntimeMeshObstaclePreparedEntry *entry = NULL;
    PhysicsSimRuntimeMeshObstaclePreparedEntry *stale_entry = NULL;
    uint64_t mtime_ns = 0u;
    uint64_t size_bytes = 0u;
    bool signature_valid = false;
    if (instance && instance->runtime_mesh_path[0]) {
        signature_valid =
            mesh_proxy_file_signature(instance->runtime_mesh_path, &mtime_ns, &size_bytes);
    }
    if (cache) {
        report->used_prepared_cache = true;
        entry = mesh_proxy_cache_find(cache,
                                      instance,
                                      signature_valid,
                                      mtime_ns,
                                      size_bytes,
                                      &stale_entry);
        if (entry) {
            cache->cache_hit_count++;
            report->prepared_cache_hit = true;
            return entry;
        }
        cache->cache_miss_count++;
        if (stale_entry) {
            cache->stale_refresh_count++;
            report->prepared_cache_stale_refresh = true;
            mesh_proxy_cache_clear_entry_contents(cache, stale_entry);
            entry = stale_entry;
        } else {
            entry = mesh_proxy_cache_allocate_entry(cache);
        }
        if (!entry) {
            mesh_proxy_diag(report, "failed to allocate runtime mesh cache entry");
            return NULL;
        }
        if (!mesh_proxy_prepare_entry(entry,
                                      instance,
                                      signature_valid,
                                      mtime_ns,
                                      size_bytes,
                                      report)) {
            mesh_proxy_prepared_entry_clear(entry);
            if (cache->prepared_entry_count > 0u) cache->prepared_entry_count--;
            return NULL;
        }
        return entry;
    }
    if (!local_entry) return NULL;
    mesh_proxy_prepared_entry_init(local_entry);
    if (!mesh_proxy_prepare_entry(local_entry,
                                  instance,
                                  signature_valid,
                                  mtime_ns,
                                  size_bytes,
                                  report)) {
        mesh_proxy_prepared_entry_clear(local_entry);
        return NULL;
    }
    return local_entry;
}

static PhysicsSimRuntimeMeshObstacleFootprintEntry *mesh_proxy_footprint_find(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstaclePreparedEntry *prepared,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshVoxelizeMode mode) {
    if (!cache || !prepared || !domain) return NULL;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY; ++i) {
        PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint = &prepared->footprints[i];
        if (!footprint->valid) continue;
        if (footprint->mode != mode) continue;
        if (!mesh_proxy_same_domain_key(&footprint->domain, domain)) continue;
        cache->voxel_footprint_cache_hit_count++;
        return footprint;
    }
    cache->voxel_footprint_cache_miss_count++;
    return NULL;
}

static PhysicsSimRuntimeMeshObstacleFootprintEntry *mesh_proxy_footprint_allocate(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstaclePreparedEntry *prepared) {
    if (!cache || !prepared) return NULL;
    for (size_t i = 0u; i < PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY; ++i) {
        PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint = &prepared->footprints[i];
        if (!footprint->valid) {
            prepared->footprint_count++;
            cache->voxel_footprint_entry_count++;
            return footprint;
        }
    }
    if (prepared->next_footprint_evict_index >=
        PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_FOOTPRINTS_PER_ENTRY) {
        prepared->next_footprint_evict_index = 0u;
    }
    mesh_proxy_footprint_entry_clear(
        &prepared->footprints[prepared->next_footprint_evict_index]);
    cache->voxel_footprint_eviction_count++;
    return &prepared->footprints[prepared->next_footprint_evict_index++];
}

static bool mesh_proxy_footprint_store(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    PhysicsSimRuntimeMeshObstaclePreparedEntry *prepared,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    const uint8_t *footprint_mask,
    size_t footprint_mask_count,
    const PhysicsSimRuntimeMeshObstacleReport *stencil_report,
    PhysicsSimRuntimeMeshObstacleFootprintEntry **out_footprint) {
    PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint = NULL;
    size_t solid_count = 0u;
    size_t write_index = 0u;
    size_t previous_prepared_footprint_count = 0u;
    size_t previous_cache_footprint_count = 0u;
    if (out_footprint) *out_footprint = NULL;
    if (!cache || !prepared || !domain || !footprint_mask || !stencil_report ||
        footprint_mask_count != domain->cell_count) {
        return false;
    }
    for (size_t i = 0u; i < footprint_mask_count; ++i) {
        if (footprint_mask[i]) solid_count++;
    }
    previous_prepared_footprint_count = prepared->footprint_count;
    previous_cache_footprint_count = cache->voxel_footprint_entry_count;
    footprint = mesh_proxy_footprint_allocate(cache, prepared);
    if (!footprint) return false;
    mesh_proxy_footprint_entry_clear(footprint);
    footprint->domain = *domain;
    footprint->mode = mode;
    footprint->solid_index_count = solid_count;
    footprint->stencil_report = *stencil_report;
    if (solid_count > 0u) {
        footprint->solid_indices = (size_t *)calloc(solid_count, sizeof(size_t));
        if (!footprint->solid_indices) {
            mesh_proxy_footprint_entry_clear(footprint);
            prepared->footprint_count = previous_prepared_footprint_count;
            cache->voxel_footprint_entry_count = previous_cache_footprint_count;
            return false;
        }
        for (size_t i = 0u; i < footprint_mask_count; ++i) {
            if (footprint_mask[i]) {
                footprint->solid_indices[write_index++] = i;
            }
        }
    }
    footprint->valid = true;
    if (out_footprint) *out_footprint = footprint;
    return true;
}

static void mesh_proxy_apply_footprint(
    const PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!footprint || !solid_mask || !report) return;
    report->solid_cell_count = 0u;
    for (size_t i = 0u; i < footprint->solid_index_count; ++i) {
        mark_mask_index(solid_mask, solid_mask_count, footprint->solid_indices[i], report);
    }
}

static void mesh_proxy_apply_footprint_mask(const uint8_t *footprint_mask,
                                            size_t footprint_mask_count,
                                            uint8_t *solid_mask,
                                            size_t solid_mask_count,
                                            PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!footprint_mask || !solid_mask || !report) return;
    report->solid_cell_count = 0u;
    for (size_t i = 0u; i < footprint_mask_count && i < solid_mask_count; ++i) {
        if (footprint_mask[i]) mark_mask_index(solid_mask, solid_mask_count, i, report);
    }
}

static bool bounds_to_grid(const SimRuntime3DDomainDesc *domain,
                           MeshProxyBounds *bounds,
                           double expansion) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    if (!domain || !bounds || !bounds->valid ||
        domain->grid_w <= 0 || domain->grid_h <= 0 || domain->grid_d <= 0 ||
        domain->voxel_size <= 0.0f) {
        return false;
    }
    min_x = bounds->min.x - expansion;
    min_y = bounds->min.y - expansion;
    min_z = bounds->min.z - expansion;
    max_x = bounds->max.x + expansion;
    max_y = bounds->max.y + expansion;
    max_z = bounds->max.z + expansion;
    if (max_x < domain->world_min_x || min_x > domain->world_max_x ||
        max_y < domain->world_min_y || min_y > domain->world_max_y ||
        max_z < domain->world_min_z || min_z > domain->world_max_z) {
        return false;
    }
    bounds->min_x = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(min_x,
                                                domain->world_min_x,
                                                domain->voxel_size,
                                                domain->grid_w),
        0,
        domain->grid_w - 1);
    bounds->max_x = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(max_x,
                                                domain->world_min_x,
                                                domain->voxel_size,
                                                domain->grid_w),
        0,
        domain->grid_w - 1);
    bounds->min_y = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(min_y,
                                                domain->world_min_y,
                                                domain->voxel_size,
                                                domain->grid_h),
        0,
        domain->grid_h - 1);
    bounds->max_y = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(max_y,
                                                domain->world_min_y,
                                                domain->voxel_size,
                                                domain->grid_h),
        0,
        domain->grid_h - 1);
    bounds->min_z = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(min_z,
                                                domain->world_min_z,
                                                domain->voxel_size,
                                                domain->grid_d),
        0,
        domain->grid_d - 1);
    bounds->max_z = clamp_int(
        sim_runtime_3d_space_world_to_grid_axis(max_z,
                                                domain->world_min_z,
                                                domain->voxel_size,
                                                domain->grid_d),
        0,
        domain->grid_d - 1);
    return bounds->min_x <= bounds->max_x &&
           bounds->min_y <= bounds->max_y &&
           bounds->min_z <= bounds->max_z;
}

static void expand_report_bounds(PhysicsSimRuntimeMeshObstacleReport *report,
                                 const MeshProxyBounds *bounds) {
    double span_x = 0.0;
    double span_y = 0.0;
    double span_z = 0.0;
    if (!report || !bounds) return;
    report->min_x = bounds->min_x;
    report->max_x = bounds->max_x;
    report->min_y = bounds->min_y;
    report->max_y = bounds->max_y;
    report->min_z = bounds->min_z;
    report->max_z = bounds->max_z;
    report->world_bounds_min = bounds->min;
    report->world_bounds_max = bounds->max;
    report->has_world_bounds = bounds->valid;
    span_x = bounds->max.x - bounds->min.x;
    span_y = bounds->max.y - bounds->min.y;
    span_z = bounds->max.z - bounds->min.z;
    report->wind_projected_area_yz = (span_y > 0.0 && span_z > 0.0) ? span_y * span_z : 0.0;
    report->bounds_volume = (span_x > 0.0 && span_y > 0.0 && span_z > 0.0)
                                ? span_x * span_y * span_z
                                : 0.0;
}

static void expand_report_world_bounds_only(PhysicsSimRuntimeMeshObstacleReport *report,
                                            const MeshProxyBounds *bounds) {
    double span_x = 0.0;
    double span_y = 0.0;
    double span_z = 0.0;
    if (!report || !bounds) return;
    report->world_bounds_min = bounds->min;
    report->world_bounds_max = bounds->max;
    report->has_world_bounds = bounds->valid;
    span_x = bounds->max.x - bounds->min.x;
    span_y = bounds->max.y - bounds->min.y;
    span_z = bounds->max.z - bounds->min.z;
    report->wind_projected_area_yz = (span_y > 0.0 && span_z > 0.0) ? span_y * span_z : 0.0;
    report->bounds_volume = (span_x > 0.0 && span_y > 0.0 && span_z > 0.0)
                                ? span_x * span_y * span_z
                                : 0.0;
}

static void rasterize_triangle_shell(const SimRuntime3DDomainDesc *domain,
                                     const CoreObjectVec3 *vertices,
                                     const CoreMeshAssetRuntimeTriangle *tri,
                                     uint8_t *solid_mask,
                                     PhysicsSimRuntimeMeshObstacleReport *report) {
    MeshProxyBounds bounds = {0};
    CoreObjectVec3 a = vertices[tri->a];
    CoreObjectVec3 b = vertices[tri->b];
    CoreObjectVec3 c = vertices[tri->c];
    CoreObjectVec3 values[2] = {b, c};
    double expansion = domain->voxel_size * 0.5;
    bounds.min = a;
    bounds.max = a;
    for (size_t i = 0u; i < 2u; ++i) {
        CoreObjectVec3 v = values[i];
        if (v.x < bounds.min.x) bounds.min.x = v.x;
        if (v.y < bounds.min.y) bounds.min.y = v.y;
        if (v.z < bounds.min.z) bounds.min.z = v.z;
        if (v.x > bounds.max.x) bounds.max.x = v.x;
        if (v.y > bounds.max.y) bounds.max.y = v.y;
        if (v.z > bounds.max.z) bounds.max.z = v.z;
    }
    bounds.valid = true;
    if (!bounds_to_grid(domain, &bounds, expansion)) return;
    for (int z = bounds.min_z; z <= bounds.max_z; ++z) {
        for (int y = bounds.min_y; y <= bounds.max_y; ++y) {
            for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
                mark_mask_cell(domain, solid_mask, report, x, y, z);
            }
        }
    }
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {a.x - b.x, a.y - b.y, a.z - b.z};
    return r;
}

static bool ray_intersects_triangle(CoreObjectVec3 origin,
                                    CoreObjectVec3 direction,
                                    CoreObjectVec3 a,
                                    CoreObjectVec3 b,
                                    CoreObjectVec3 c) {
    const double eps = 1.0e-9;
    CoreObjectVec3 edge1 = vec_sub(b, a);
    CoreObjectVec3 edge2 = vec_sub(c, a);
    CoreObjectVec3 h = vec_cross(direction, edge2);
    double det = vec_dot(edge1, h);
    double inv_det = 0.0;
    CoreObjectVec3 s = {0};
    double u = 0.0;
    CoreObjectVec3 q = {0};
    double v = 0.0;
    double t = 0.0;
    if (det > -eps && det < eps) return false;
    inv_det = 1.0 / det;
    s = vec_sub(origin, a);
    u = inv_det * vec_dot(s, h);
    if (u < -eps || u > 1.0 + eps) return false;
    q = vec_cross(s, edge1);
    v = inv_det * vec_dot(direction, q);
    if (v < -eps || u + v > 1.0 + eps) return false;
    t = inv_det * vec_dot(edge2, q);
    return t > eps;
}

static bool point_inside_closed_mesh(const CoreMeshAssetRuntimeDocument *document,
                                     const CoreObjectVec3 *vertices,
                                     CoreObjectVec3 point) {
    CoreObjectVec3 direction = {1.0, 0.3713906763541037, 0.23717082451262844};
    int hits = 0;
    if (!document || !vertices) return false;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        if (tri->a >= document->vertex_count ||
            tri->b >= document->vertex_count ||
            tri->c >= document->vertex_count) {
            continue;
        }
        if (ray_intersects_triangle(point,
                                    direction,
                                    vertices[tri->a],
                                    vertices[tri->b],
                                    vertices[tri->c])) {
            hits += 1;
        }
    }
    return (hits % 2) == 1;
}

static bool point_inside_closed_mesh_accel(const PhysicsSimRuntimeMeshAccel *accel,
                                           CoreObjectVec3 point) {
    CoreObjectVec3 direction = {1.0, 0.3713906763541037, 0.23717082451262844};
    int hits = physics_sim_runtime_mesh_accel_count_ray_intersections(accel,
                                                                      point,
                                                                      direction);
    return (hits % 2) == 1;
}

static void fill_closed_volume_cells(const CoreMeshAssetRuntimeDocument *document,
                                     const SimRuntime3DDomainDesc *domain,
                                     const CoreObjectVec3 *vertices,
                                     const MeshProxyBounds *bounds,
                                     const PhysicsSimRuntimeMeshAccel *accel,
                                     uint8_t *solid_mask,
                                     PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!document || !domain || !vertices || !bounds || !solid_mask) return;
    for (int z = bounds->min_z; z <= bounds->max_z; ++z) {
        for (int y = bounds->min_y; y <= bounds->max_y; ++y) {
            for (int x = bounds->min_x; x <= bounds->max_x; ++x) {
                CoreObjectVec3 point = {
                    cell_center_axis(domain->world_min_x, domain->voxel_size, x),
                    cell_center_axis(domain->world_min_y, domain->voxel_size, y),
                    cell_center_axis(domain->world_min_z, domain->voxel_size, z)
                };
                if (report) report->tested_cell_count += 1u;
                if (accel) {
                    if (!point_inside_closed_mesh_accel(accel, point)) continue;
                } else if (!point_inside_closed_mesh(document, vertices, point)) {
                    continue;
                }
                mark_mask_cell(domain, solid_mask, report, x, y, z);
            }
        }
    }
}

static bool mesh_proxy_voxelize_prepared_to_mask(
    PhysicsSimRuntimeMeshObstaclePreparedEntry *prepared,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    PhysicsSimRuntimeMeshObstacleReport *report) {
    CoreMeshAssetRuntimeDocument *document = NULL;
    CoreObjectVec3 *vertices = NULL;
    MeshProxyBounds bounds = {0};
    PhysicsSimRuntimeMeshAccelStats accel_stats;
    size_t bbox_cell_count = 0u;
    if (!prepared || !prepared->valid || !domain || !solid_mask || !report ||
        solid_mask_count != domain->cell_count || domain->cell_count == 0u) {
        mesh_proxy_diag(report, "invalid prepared runtime mesh voxelization request");
        return false;
    }

    document = &prepared->document;
    vertices = prepared->vertices;
    bounds = prepared->bounds;
    report->loaded_runtime_mesh = true;
    report->runtime_vertex_count = document->vertex_count;
    report->runtime_triangle_count = document->triangle_count;
    report->file_signature_valid = prepared->runtime_mesh_signature_valid;
    report->runtime_mesh_file_mtime_ns = prepared->runtime_mesh_mtime_ns;
    report->runtime_mesh_file_size_bytes = prepared->runtime_mesh_size_bytes;
    report->local_bounds_min = document->contract.local_bounds.min;
    report->local_bounds_max = document->contract.local_bounds.max;
    report->has_local_bounds = true;
    if (!bounds_to_grid(domain, &bounds, (double)domain->voxel_size * 0.5)) {
        expand_report_world_bounds_only(report, &bounds);
        mesh_proxy_diag(report, "mesh bounds outside active domain");
        return true;
    }
    expand_report_bounds(report, &bounds);

    bbox_cell_count = (size_t)(bounds.max_x - bounds.min_x + 1) *
                      (size_t)(bounds.max_y - bounds.min_y + 1) *
                      (size_t)(bounds.max_z - bounds.min_z + 1);
    if (bbox_cell_count > PHYSICS_SIM_RUNTIME_MESH_OBSTACLE_MAX_GRID_TEST_CELLS) {
        report->budget_limited = true;
        for (int z = bounds.min_z; z <= bounds.max_z; ++z) {
            for (int y = bounds.min_y; y <= bounds.max_y; ++y) {
                for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
                    mark_mask_cell(domain, solid_mask, report, x, y, z);
                }
            }
        }
        mesh_proxy_fallback_reason(report, "grid-test budget exceeded; filled transformed runtime-mesh bounds");
        mesh_proxy_diag(report, "budget fallback used mesh bounds");
        return true;
    }

    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        if (tri->a >= document->vertex_count ||
            tri->b >= document->vertex_count ||
            tri->c >= document->vertex_count) {
            continue;
        }
        rasterize_triangle_shell(domain, vertices, tri, solid_mask, report);
    }
    report->used_surface_shell = true;

    if (mode == PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SOLID_VOLUME &&
        document->contract.topology_closed_volume) {
        if (prepared->accel_built) {
            physics_sim_runtime_mesh_accel_stats(&prepared->accel, &accel_stats);
            report->used_triangle_accel = true;
            report->accel_triangle_count = accel_stats.triangle_count;
            report->accel_node_count = accel_stats.node_count;
            report->accel_leaf_count = accel_stats.leaf_count;
            report->accel_max_depth = accel_stats.max_depth;
        }
        fill_closed_volume_cells(document,
                                 domain,
                                 vertices,
                                 &bounds,
                                 prepared->accel_built ? &prepared->accel : NULL,
                                 solid_mask,
                                 report);
        report->used_closed_volume_fill = true;
    }
    return true;
}

bool physics_sim_runtime_mesh_obstacle_voxelize_instance(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshObstacleReport *out_report) {
    return physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(
        NULL,
        instance,
        domain,
        solid_mask,
        solid_mask_count,
        out_report);
}

bool physics_sim_runtime_mesh_obstacle_voxelize_instance_mode(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    PhysicsSimRuntimeMeshObstacleReport *out_report) {
    return physics_sim_runtime_mesh_obstacle_voxelize_instance_mode_cached(
        NULL,
        instance,
        domain,
        solid_mask,
        solid_mask_count,
        mode,
        out_report);
}

bool physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshObstacleReport *out_report) {
    return physics_sim_runtime_mesh_obstacle_voxelize_instance_mode_cached(
        cache,
        instance,
        domain,
        solid_mask,
        solid_mask_count,
        PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SOLID_VOLUME,
        out_report);
}

bool physics_sim_runtime_mesh_obstacle_voxelize_instance_mode_cached(
    PhysicsSimRuntimeMeshObstacleCache *cache,
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    const SimRuntime3DDomainDesc *domain,
    uint8_t *solid_mask,
    size_t solid_mask_count,
    PhysicsSimRuntimeMeshVoxelizeMode mode,
    PhysicsSimRuntimeMeshObstacleReport *out_report) {
    PhysicsSimRuntimeMeshObstacleReport report;
    PhysicsSimRuntimeMeshObstaclePreparedEntry local_entry;
    PhysicsSimRuntimeMeshObstaclePreparedEntry *prepared = NULL;
    PhysicsSimRuntimeMeshObstacleFootprintEntry *footprint = NULL;
    uint8_t *footprint_mask = NULL;
    bool ok = false;

    physics_sim_runtime_mesh_obstacle_report_init(&report);
    memset(&local_entry, 0, sizeof(local_entry));
    if (!instance || !instance->runtime_path_resolved || !instance->runtime_mesh_path[0] ||
        (!physics_sim_runtime_mesh_obstacle_instance_enabled(instance) &&
         mode != PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL)) {
        mesh_proxy_diag(&report, "instance disabled");
        goto done;
    }
    if (!domain || !solid_mask || solid_mask_count != domain->cell_count ||
        domain->cell_count == 0u) {
        mesh_proxy_diag(&report, "invalid domain or mask");
        goto done;
    }
    prepared = mesh_proxy_prepare_instance(cache, &local_entry, instance, &report);
    if (!prepared || !prepared->valid) {
        goto done;
    }

    if (cache) {
        report.used_voxel_footprint_cache = true;
        footprint = mesh_proxy_footprint_find(cache, prepared, domain, mode);
        if (footprint) {
            const bool used_prepared_cache = report.used_prepared_cache;
            const bool prepared_cache_hit = report.prepared_cache_hit;
            const bool prepared_cache_stale_refresh = report.prepared_cache_stale_refresh;
            report = footprint->stencil_report;
            report.used_prepared_cache = used_prepared_cache;
            report.prepared_cache_hit = prepared_cache_hit;
            report.prepared_cache_stale_refresh = prepared_cache_stale_refresh;
            report.used_voxel_footprint_cache = true;
            report.voxel_footprint_cache_hit = true;
            mesh_proxy_apply_footprint(footprint, solid_mask, solid_mask_count, &report);
            ok = true;
            goto done;
        }

        footprint_mask = (uint8_t *)calloc(domain->cell_count, sizeof(uint8_t));
        if (footprint_mask) {
            const bool used_prepared_cache = report.used_prepared_cache;
            const bool prepared_cache_hit = report.prepared_cache_hit;
            const bool prepared_cache_stale_refresh = report.prepared_cache_stale_refresh;
            ok = mesh_proxy_voxelize_prepared_to_mask(prepared,
                                                       domain,
                                                       footprint_mask,
                                                       solid_mask_count,
                                                       mode,
                                                       &report);
            report.used_prepared_cache = used_prepared_cache;
            report.prepared_cache_hit = prepared_cache_hit;
            report.prepared_cache_stale_refresh = prepared_cache_stale_refresh;
            report.used_voxel_footprint_cache = true;
            if (ok) {
                if (mesh_proxy_footprint_store(cache,
                                               prepared,
                                               domain,
                                               mode,
                                               footprint_mask,
                                               solid_mask_count,
                                               &report,
                                               &footprint) &&
                    footprint) {
                    mesh_proxy_apply_footprint(footprint, solid_mask, solid_mask_count, &report);
                } else {
                    mesh_proxy_apply_footprint_mask(footprint_mask,
                                                    solid_mask_count,
                                                    solid_mask,
                                                    solid_mask_count,
                                                    &report);
                    mesh_proxy_fallback_reason(&report, "runtime mesh footprint cache store failed");
                }
                goto done;
            }
        } else {
            mesh_proxy_fallback_reason(&report, "runtime mesh footprint cache allocation failed");
        }
    }

    ok = mesh_proxy_voxelize_prepared_to_mask(prepared,
                                              domain,
                                              solid_mask,
                                              solid_mask_count,
                                              mode,
                                              &report);

done:
    free(footprint_mask);
    if (prepared == &local_entry) {
        mesh_proxy_prepared_entry_clear(&local_entry);
    }
    if (out_report) *out_report = report;
    return ok;
}
