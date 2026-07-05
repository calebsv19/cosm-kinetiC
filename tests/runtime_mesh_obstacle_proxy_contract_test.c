#include "app/sim_runtime_mesh_obstacle_proxy.h"

#include "app/scene_state.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "core_mesh_asset.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        g_failures += 1; \
    } \
} while (0)

void backend_3d_scaffold_set_obstacle_cell(SimRuntimeBackend3DScaffold *state,
                                           int x,
                                           int y,
                                           int z,
                                           bool solid) {
    size_t idx = 0u;
    if (!state || !state->obstacle_occupancy || !solid) return;
    idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
    state->obstacle_occupancy[idx] = 1u;
}

static void configure_domain(SimRuntime3DDomainDesc *domain) {
    memset(domain, 0, sizeof(*domain));
    domain->grid_w = 8;
    domain->grid_h = 8;
    domain->grid_d = 8;
    domain->slice_cell_count = 64u;
    domain->cell_count = 512u;
    domain->world_min_x = -1.0f;
    domain->world_min_y = -1.0f;
    domain->world_min_z = -1.0f;
    domain->world_max_x = 1.0f;
    domain->world_max_y = 1.0f;
    domain->world_max_z = 1.0f;
    domain->voxel_size = 0.25f;
}

static void configure_large_domain(SimRuntime3DDomainDesc *domain) {
    memset(domain, 0, sizeof(*domain));
    domain->grid_w = 110;
    domain->grid_h = 110;
    domain->grid_d = 110;
    domain->slice_cell_count = 12100u;
    domain->cell_count = 1331000u;
    domain->world_min_x = -1.0f;
    domain->world_min_y = -1.0f;
    domain->world_min_z = -1.0f;
    domain->world_max_x = 1.0f;
    domain->world_max_y = 1.0f;
    domain->world_max_z = 1.0f;
    domain->voxel_size = 2.0f / 110.0f;
}

static void configure_imported_mesh_domain(SimRuntime3DDomainDesc *domain) {
    memset(domain, 0, sizeof(*domain));
    domain->grid_w = 24;
    domain->grid_h = 24;
    domain->grid_d = 24;
    domain->slice_cell_count = 576u;
    domain->cell_count = 13824u;
    domain->world_min_x = -1.0f;
    domain->world_min_y = -1.0f;
    domain->world_min_z = -1.0f;
    domain->world_max_x = 1.0f;
    domain->world_max_y = 1.0f;
    domain->world_max_z = 1.0f;
    domain->voxel_size = 2.0f / 24.0f;
}

static size_t test_index(const SimRuntime3DDomainDesc *domain, int x, int y, int z) {
    return ((size_t)z * (size_t)domain->grid_h + (size_t)y) * (size_t)domain->grid_w +
           (size_t)x;
}

static bool write_cube_runtime_mesh(const char *path) {
    CoreMeshAssetRuntimeDocument document;
    static const double verts[8][3] = {
        {-0.5, -0.5, -0.5},
        { 0.5, -0.5, -0.5},
        { 0.5,  0.5, -0.5},
        {-0.5,  0.5, -0.5},
        {-0.5, -0.5,  0.5},
        { 0.5, -0.5,  0.5},
        { 0.5,  0.5,  0.5},
        {-0.5,  0.5,  0.5}
    };
    static const size_t tris[12][3] = {
        {0, 2, 1}, {0, 3, 2},
        {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4},
        {1, 2, 6}, {1, 6, 5},
        {2, 3, 7}, {2, 7, 6},
        {3, 0, 4}, {3, 4, 7}
    };

    core_mesh_asset_runtime_document_init(&document);
    if (core_mesh_asset_runtime_contract_set_asset_id(&document.contract, "cube_obstacle").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(&document.contract, "cube_obstacle").code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(&document, 8u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(&document, 12u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(&document, 1u).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    document.contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document.contract.local_bounds.min = (CoreObjectVec3){-0.5, -0.5, -0.5};
    document.contract.local_bounds.max = (CoreObjectVec3){0.5, 0.5, 0.5};
    document.contract.topology_closed_volume = true;
    document.contract.topology_manifold_expected = true;
    for (size_t i = 0u; i < 8u; ++i) {
        document.vertices[i].position = (CoreObjectVec3){verts[i][0], verts[i][1], verts[i][2]};
    }
    for (size_t i = 0u; i < 12u; ++i) {
        document.triangles[i].a = tris[i][0];
        document.triangles[i].b = tris[i][1];
        document.triangles[i].c = tris[i][2];
        snprintf(document.triangles[i].surface_group_id,
                 sizeof(document.triangles[i].surface_group_id),
                 "%s",
                 "shell");
    }
    snprintf(document.surface_groups[0].group_id,
             sizeof(document.surface_groups[0].group_id),
             "%s",
             "shell");
    document.surface_groups[0].triangle_start = 0u;
    document.surface_groups[0].triangle_count = 12u;

    if (core_mesh_asset_runtime_document_save_file(&document, path).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    core_mesh_asset_runtime_document_free(&document);
    return true;
}

static bool append_runtime_mesh_whitespace(const char *path) {
    FILE *f = fopen(path, "a");
    bool ok = false;
    if (!f) return false;
    ok = fputs("\n", f) >= 0;
    ok = fclose(f) == 0 && ok;
    return ok;
}

static size_t sphere_ring_vertex_index(size_t ring, size_t segment, size_t segments) {
    return 2u + (ring - 1u) * segments + (segment % segments);
}

static void write_triangle(CoreMeshAssetRuntimeDocument *document,
                           size_t *triangle_index,
                           size_t a,
                           size_t b,
                           size_t c) {
    document->triangles[*triangle_index].a = a;
    document->triangles[*triangle_index].b = b;
    document->triangles[*triangle_index].c = c;
    snprintf(document->triangles[*triangle_index].surface_group_id,
             sizeof(document->triangles[*triangle_index].surface_group_id),
             "%s",
             "shell");
    *triangle_index += 1u;
}

static bool write_sphere_runtime_mesh(const char *path, size_t lat_segments, size_t segments) {
    CoreMeshAssetRuntimeDocument document;
    size_t vertex_count = 0u;
    size_t triangle_count = 0u;
    size_t vertex_index = 0u;
    size_t triangle_index = 0u;

    if (lat_segments < 4u || segments < 8u) return false;
    vertex_count = 2u + (lat_segments - 1u) * segments;
    triangle_count = segments * 2u + (lat_segments - 2u) * segments * 2u;

    core_mesh_asset_runtime_document_init(&document);
    if (core_mesh_asset_runtime_contract_set_asset_id(&document.contract, "high_quality_sphere_obstacle").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(&document.contract, "high_quality_sphere_obstacle").code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(&document, vertex_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(&document, triangle_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(&document, 1u).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    document.contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document.contract.local_bounds.min = (CoreObjectVec3){-0.5, -0.5, -0.5};
    document.contract.local_bounds.max = (CoreObjectVec3){0.5, 0.5, 0.5};
    document.contract.topology_closed_volume = true;
    document.contract.topology_manifold_expected = true;

    document.vertices[vertex_index++].position = (CoreObjectVec3){0.0, 0.0, 0.5};
    document.vertices[vertex_index++].position = (CoreObjectVec3){0.0, 0.0, -0.5};
    for (size_t ring = 1u; ring < lat_segments; ++ring) {
        double theta = M_PI * (double)ring / (double)lat_segments;
        double ring_radius = 0.5 * sin(theta);
        double z = 0.5 * cos(theta);
        for (size_t segment = 0u; segment < segments; ++segment) {
            double phi = 2.0 * M_PI * (double)segment / (double)segments;
            document.vertices[vertex_index++].position =
                (CoreObjectVec3){ring_radius * cos(phi), ring_radius * sin(phi), z};
        }
    }
    for (size_t segment = 0u; segment < segments; ++segment) {
        size_t next = (segment + 1u) % segments;
        write_triangle(&document,
                       &triangle_index,
                       0u,
                       sphere_ring_vertex_index(1u, segment, segments),
                       sphere_ring_vertex_index(1u, next, segments));
    }
    for (size_t ring = 1u; ring + 1u < lat_segments; ++ring) {
        for (size_t segment = 0u; segment < segments; ++segment) {
            size_t next = (segment + 1u) % segments;
            size_t a = sphere_ring_vertex_index(ring, segment, segments);
            size_t b = sphere_ring_vertex_index(ring, next, segments);
            size_t c = sphere_ring_vertex_index(ring + 1u, next, segments);
            size_t d = sphere_ring_vertex_index(ring + 1u, segment, segments);
            write_triangle(&document, &triangle_index, a, b, c);
            write_triangle(&document, &triangle_index, a, c, d);
        }
    }
    for (size_t segment = 0u; segment < segments; ++segment) {
        size_t next = (segment + 1u) % segments;
        write_triangle(&document,
                       &triangle_index,
                       1u,
                       sphere_ring_vertex_index(lat_segments - 1u, next, segments),
                       sphere_ring_vertex_index(lat_segments - 1u, segment, segments));
    }
    snprintf(document.surface_groups[0].group_id,
             sizeof(document.surface_groups[0].group_id),
             "%s",
             "shell");
    document.surface_groups[0].triangle_start = 0u;
    document.surface_groups[0].triangle_count = triangle_count;

    if (triangle_index != triangle_count ||
        vertex_index != vertex_count ||
        core_mesh_asset_runtime_document_save_file(&document, path).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    core_mesh_asset_runtime_document_free(&document);
    return true;
}

static bool test_default_runtime_mesh_instances_are_enabled(void) {
    PhysicsSimRuntimeMeshPreviewInstance instance;
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", "/tmp/missing.json");
    return physics_sim_runtime_mesh_obstacle_instance_enabled(&instance);
}

static bool test_visual_only_instances_do_not_voxelize(void) {
    PhysicsSimRuntimeMeshPreviewInstance instance;
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "visual_only");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", "/tmp/missing.json");
    return !physics_sim_runtime_mesh_obstacle_instance_enabled(&instance);
}

static bool test_mesh_emitter_instances_do_not_voxelize_as_obstacles(void) {
    PhysicsSimRuntimeMeshPreviewInstance instance;
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    instance.fluid_emitter_enabled = true;
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "surface_heat_emitter");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", "/tmp/missing.json");
    return !physics_sim_runtime_mesh_obstacle_instance_enabled(&instance);
}

static bool test_closed_runtime_mesh_voxelizes_from_actual_asset(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport report;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_proxy_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    instance.transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    CHECK_TRUE(physics_sim_runtime_mesh_obstacle_voxelize_instance(&instance,
                                                                   &domain,
                                                                   mask,
                                                                   sizeof(mask),
                                                                   &report));
    CHECK_TRUE(report.loaded_runtime_mesh);
    CHECK_TRUE(report.used_surface_shell);
    CHECK_TRUE(report.used_closed_volume_fill);
    CHECK_TRUE(!report.budget_limited);
    CHECK_TRUE(report.has_local_bounds);
    CHECK_TRUE(report.has_world_bounds);
    CHECK_TRUE(report.runtime_vertex_count == 8u);
    CHECK_TRUE(report.runtime_triangle_count == 12u);
    CHECK_TRUE(report.wind_projected_area_yz > 0.9);
    CHECK_TRUE(report.bounds_volume > 0.9);
    CHECK_TRUE(report.solid_cell_count > 0u);
    CHECK_TRUE(mask[test_index(&domain, 4, 4, 4)] != 0u);

    unlink(mesh_path);
    rmdir(temp_dir);
    return report.solid_cell_count > 0u && mask[test_index(&domain, 4, 4, 4)] != 0u;
}

static bool test_preview_bounds_do_not_drive_solver_truth(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport report;
    bool ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_preview_truth_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.preview_metadata_valid = true;
    instance.has_world_bounds = true;
    instance.world_bounds_min = (CoreObjectVec3){98.0, 98.0, 98.0};
    instance.world_bounds_max = (CoreObjectVec3){99.0, 99.0, 99.0};
    instance.fluid_obstacle_enabled = true;
    instance.transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    ok = physics_sim_runtime_mesh_obstacle_voxelize_instance(&instance,
                                                             &domain,
                                                             mask,
                                                             sizeof(mask),
                                                             &report);
    CHECK_TRUE(ok);
    CHECK_TRUE(report.has_local_bounds);
    CHECK_TRUE(report.has_world_bounds);
    CHECK_TRUE(report.world_bounds_min.x < -0.49 && report.world_bounds_min.x > -0.51);
    CHECK_TRUE(report.world_bounds_max.x > 0.49 && report.world_bounds_max.x < 0.51);
    CHECK_TRUE(mask[test_index(&domain, 4, 4, 4)] != 0u);

    unlink(mesh_path);
    rmdir(temp_dir);
    return ok &&
           report.has_world_bounds &&
           report.world_bounds_min.x < -0.49 &&
           report.world_bounds_max.x > 0.49 &&
           mask[test_index(&domain, 4, 4, 4)] != 0u;
}

static bool test_out_of_domain_runtime_mesh_does_not_clamp_to_boundary(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport report;
    bool any_marked = false;
    bool ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_outside_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    instance.transform_position = (CoreObjectVec3){0.0, 4.0, 0.0};
    instance.transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    ok = physics_sim_runtime_mesh_obstacle_voxelize_instance(&instance,
                                                             &domain,
                                                             mask,
                                                             sizeof(mask),
                                                             &report);
    CHECK_TRUE(ok);
    CHECK_TRUE(report.loaded_runtime_mesh);
    CHECK_TRUE(report.has_world_bounds);
    CHECK_TRUE(report.world_bounds_min.y > domain.world_max_y);
    CHECK_TRUE(report.solid_cell_count == 0u);
    CHECK_TRUE(strstr(report.diagnostics, "outside active domain") != NULL);
    for (size_t i = 0u; i < sizeof(mask); ++i) {
        if (mask[i] != 0u) {
            any_marked = true;
            break;
        }
    }
    CHECK_TRUE(!any_marked);

    unlink(mesh_path);
    rmdir(temp_dir);
    return ok &&
           report.has_world_bounds &&
           report.world_bounds_min.y > domain.world_max_y &&
           report.solid_cell_count == 0u &&
           !any_marked;
}

static bool test_high_triangle_runtime_mesh_uses_acceleration_and_solver_truth(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t *mask = NULL;
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport report;
    bool ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_high_quality_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/sphere.runtime.json", temp_dir);
    CHECK_TRUE(write_sphere_runtime_mesh(mesh_path, 18u, 32u));

    configure_imported_mesh_domain(&domain);
    mask = (uint8_t *)calloc(domain.cell_count, sizeof(uint8_t));
    CHECK_TRUE(mask != NULL);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.preview_metadata_valid = true;
    instance.has_world_bounds = true;
    instance.world_bounds_min = (CoreObjectVec3){8.0, 8.0, 8.0};
    instance.world_bounds_max = (CoreObjectVec3){9.0, 9.0, 9.0};
    instance.fluid_obstacle_enabled = true;
    instance.transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    if (mask) {
        ok = physics_sim_runtime_mesh_obstacle_voxelize_instance(&instance,
                                                                 &domain,
                                                                 mask,
                                                                 domain.cell_count,
                                                                 &report);
    }
    CHECK_TRUE(ok);
    CHECK_TRUE(report.loaded_runtime_mesh);
    CHECK_TRUE(report.used_surface_shell);
    CHECK_TRUE(report.used_closed_volume_fill);
    CHECK_TRUE(report.used_triangle_accel);
    CHECK_TRUE(report.runtime_triangle_count >= 1000u);
    CHECK_TRUE(report.accel_triangle_count == report.runtime_triangle_count);
    CHECK_TRUE(report.accel_node_count > 1u);
    CHECK_TRUE(report.accel_leaf_count > 1u);
    CHECK_TRUE(report.accel_max_depth > 1u);
    CHECK_TRUE(!report.budget_limited);
    CHECK_TRUE(report.has_world_bounds);
    CHECK_TRUE(report.world_bounds_min.x < -0.49 && report.world_bounds_max.x > 0.49);
    CHECK_TRUE(report.wind_projected_area_yz > 0.9);
    CHECK_TRUE(report.bounds_volume > 0.9);
    CHECK_TRUE(report.solid_cell_count > 0u);
    CHECK_TRUE(mask && mask[test_index(&domain, 12, 12, 12)] != 0u);

    free(mask);
    unlink(mesh_path);
    rmdir(temp_dir);
    return ok &&
           report.used_triangle_accel &&
           report.accel_node_count > 1u &&
           report.solid_cell_count > 0u &&
           report.has_world_bounds &&
           report.world_bounds_min.x < -0.49 &&
           report.world_bounds_max.x > 0.49;
}

static bool test_runtime_mesh_prepared_cache_reuses_loaded_mesh(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t first_mask[512] = {0};
    uint8_t second_mask[512] = {0};
    uint8_t third_mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport first_report;
    PhysicsSimRuntimeMeshObstacleReport second_report;
    PhysicsSimRuntimeMeshObstacleReport third_report;
    PhysicsSimRuntimeMeshObstacleCache *cache = NULL;
    PhysicsSimRuntimeMeshObstacleCacheStats stats;
    PhysicsSimRuntimeMeshObstacleCacheStats refresh_stats;
    bool first_ok = false;
    bool second_ok = false;
    bool third_ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_cache_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    instance.transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    cache = physics_sim_runtime_mesh_obstacle_cache_create();
    CHECK_TRUE(cache != NULL);
    if (cache) {
        first_ok = physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(cache,
                                                                              &instance,
                                                                              &domain,
                                                                              first_mask,
                                                                              sizeof(first_mask),
                                                                              &first_report);
        second_ok = physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(cache,
                                                                               &instance,
                                                                               &domain,
                                                                               second_mask,
                                                                               sizeof(second_mask),
                                                                               &second_report);
        physics_sim_runtime_mesh_obstacle_cache_stats(cache, &stats);
        CHECK_TRUE(append_runtime_mesh_whitespace(mesh_path));
        third_ok = physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(cache,
                                                                              &instance,
                                                                              &domain,
                                                                              third_mask,
                                                                              sizeof(third_mask),
                                                                              &third_report);
        physics_sim_runtime_mesh_obstacle_cache_stats(cache, &refresh_stats);
    } else {
        memset(&stats, 0, sizeof(stats));
        memset(&refresh_stats, 0, sizeof(refresh_stats));
    }

    CHECK_TRUE(first_ok);
    CHECK_TRUE(second_ok);
    CHECK_TRUE(third_ok);
    CHECK_TRUE(first_report.used_prepared_cache);
    CHECK_TRUE(!first_report.prepared_cache_hit);
    CHECK_TRUE(second_report.used_prepared_cache);
    CHECK_TRUE(second_report.prepared_cache_hit);
    CHECK_TRUE(third_report.used_prepared_cache);
    CHECK_TRUE(!third_report.prepared_cache_hit);
    CHECK_TRUE(third_report.prepared_cache_stale_refresh);
    CHECK_TRUE(first_report.used_voxel_footprint_cache);
    CHECK_TRUE(!first_report.voxel_footprint_cache_hit);
    CHECK_TRUE(second_report.used_voxel_footprint_cache);
    CHECK_TRUE(second_report.voxel_footprint_cache_hit);
    CHECK_TRUE(third_report.used_voxel_footprint_cache);
    CHECK_TRUE(!third_report.voxel_footprint_cache_hit);
    CHECK_TRUE(first_report.used_triangle_accel);
    CHECK_TRUE(second_report.used_triangle_accel);
    CHECK_TRUE(third_report.used_triangle_accel);
    CHECK_TRUE(first_report.file_signature_valid);
    CHECK_TRUE(second_report.file_signature_valid);
    CHECK_TRUE(third_report.file_signature_valid);
    CHECK_TRUE(third_report.runtime_mesh_file_size_bytes >
               second_report.runtime_mesh_file_size_bytes);
    CHECK_TRUE(first_report.tested_cell_count == second_report.tested_cell_count);
    CHECK_TRUE(first_report.tested_cell_count == third_report.tested_cell_count);
    CHECK_TRUE(first_report.solid_cell_count == second_report.solid_cell_count);
    CHECK_TRUE(first_report.solid_cell_count == third_report.solid_cell_count);
    CHECK_TRUE(first_mask[test_index(&domain, 4, 4, 4)] != 0u);
    CHECK_TRUE(second_mask[test_index(&domain, 4, 4, 4)] != 0u);
    CHECK_TRUE(third_mask[test_index(&domain, 4, 4, 4)] != 0u);
    CHECK_TRUE(stats.prepared_entry_count == 1u);
    CHECK_TRUE(stats.cache_miss_count == 1u);
    CHECK_TRUE(stats.cache_hit_count == 1u);
    CHECK_TRUE(stats.stale_refresh_count == 0u);
    CHECK_TRUE(stats.eviction_count == 0u);
    CHECK_TRUE(stats.voxel_footprint_entry_count == 1u);
    CHECK_TRUE(stats.voxel_footprint_cache_miss_count == 1u);
    CHECK_TRUE(stats.voxel_footprint_cache_hit_count == 1u);
    CHECK_TRUE(stats.voxel_footprint_eviction_count == 0u);
    CHECK_TRUE(refresh_stats.prepared_entry_count == 1u);
    CHECK_TRUE(refresh_stats.cache_miss_count == 2u);
    CHECK_TRUE(refresh_stats.cache_hit_count == 1u);
    CHECK_TRUE(refresh_stats.stale_refresh_count == 1u);
    CHECK_TRUE(refresh_stats.eviction_count == 0u);
    CHECK_TRUE(refresh_stats.voxel_footprint_entry_count == 1u);
    CHECK_TRUE(refresh_stats.voxel_footprint_cache_miss_count == 2u);
    CHECK_TRUE(refresh_stats.voxel_footprint_cache_hit_count == 1u);
    CHECK_TRUE(refresh_stats.voxel_footprint_eviction_count == 0u);

    physics_sim_runtime_mesh_obstacle_cache_destroy(cache);
    unlink(mesh_path);
    rmdir(temp_dir);
    return first_ok &&
           second_ok &&
           third_ok &&
           first_report.used_prepared_cache &&
           !first_report.prepared_cache_hit &&
           second_report.prepared_cache_hit &&
           third_report.prepared_cache_stale_refresh &&
           first_report.used_voxel_footprint_cache &&
           !first_report.voxel_footprint_cache_hit &&
           second_report.voxel_footprint_cache_hit &&
           !third_report.voxel_footprint_cache_hit &&
           stats.prepared_entry_count == 1u &&
           stats.cache_hit_count == 1u &&
           stats.cache_miss_count == 1u &&
           stats.voxel_footprint_entry_count == 1u &&
           stats.voxel_footprint_cache_hit_count == 1u &&
           stats.voxel_footprint_cache_miss_count == 1u &&
           refresh_stats.prepared_entry_count == 1u &&
           refresh_stats.stale_refresh_count == 1u &&
           refresh_stats.voxel_footprint_entry_count == 1u;
}

static bool test_runtime_mesh_budget_fallback_reports_reason(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    uint8_t *mask = NULL;
    PhysicsSimRuntimeMeshPreviewInstance instance;
    PhysicsSimRuntimeMeshObstacleReport report;
    bool ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_budget_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_large_domain(&domain);
    mask = (uint8_t *)calloc(domain.cell_count, sizeof(uint8_t));
    CHECK_TRUE(mask != NULL);
    memset(&instance, 0, sizeof(instance));
    instance.runtime_path_resolved = true;
    instance.fluid_obstacle_enabled = true;
    instance.transform_scale = (CoreObjectVec3){4.0, 4.0, 4.0};
    snprintf(instance.fluid_behavior, sizeof(instance.fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance.mesh_proxy_mode,
             sizeof(instance.mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance.runtime_mesh_path, sizeof(instance.runtime_mesh_path), "%s", mesh_path);

    if (mask) {
        ok = physics_sim_runtime_mesh_obstacle_voxelize_instance(&instance,
                                                                 &domain,
                                                                 mask,
                                                                 domain.cell_count,
                                                                 &report);
    }
    CHECK_TRUE(ok);
    CHECK_TRUE(report.budget_limited);
    CHECK_TRUE(report.fallback_reason[0] != '\0');
    CHECK_TRUE(report.solid_cell_count > 0u);

    free(mask);
    unlink(mesh_path);
    rmdir(temp_dir);
    return ok && report.budget_limited && report.fallback_reason[0] != '\0' &&
           report.solid_cell_count > 0u;
}

static bool test_backend_adapter_marks_scene_mesh_instance_obstacles(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    SimRuntimeBackend3DScaffold state;
    SceneState scene;
    uint8_t obstacle_mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance *instance = NULL;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_obstacle_adapter_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&state, 0, sizeof(state));
    memset(&scene, 0, sizeof(scene));
    state.volume.desc = domain;
    state.obstacle_occupancy = obstacle_mask;
    scene.runtime_visual.mesh_previews.valid_contract = true;
    scene.runtime_visual.mesh_previews.instance_count = 1;
    instance = &scene.runtime_visual.mesh_previews.instances[0];
    instance->runtime_path_resolved = true;
    instance->fluid_obstacle_enabled = true;
    instance->transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance->mesh_proxy_mode,
             sizeof(instance->mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    snprintf(instance->runtime_mesh_path, sizeof(instance->runtime_mesh_path), "%s", mesh_path);

    backend_3d_scaffold_rasterize_runtime_mesh_asset_obstacles(&state, &scene);
    CHECK_TRUE(obstacle_mask[test_index(&domain, 4, 4, 4)] != 0u);

    unlink(mesh_path);
    rmdir(temp_dir);
    return obstacle_mask[test_index(&domain, 4, 4, 4)] != 0u;
}

static bool test_backend_adapter_skips_mesh_instances_attached_as_emitters(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    SimRuntime3DDomainDesc domain;
    SimRuntimeBackend3DScaffold state;
    SceneState scene;
    FluidScenePreset preset;
    uint8_t obstacle_mask[512] = {0};
    PhysicsSimRuntimeMeshPreviewInstance *instance = NULL;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_mesh_emitter_skip_%ld",
             (long)getpid());
    CHECK_TRUE(mkdir(temp_dir, 0700) == 0);
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    CHECK_TRUE(write_cube_runtime_mesh(mesh_path));

    configure_domain(&domain);
    memset(&state, 0, sizeof(state));
    memset(&scene, 0, sizeof(scene));
    memset(&preset, 0, sizeof(preset));
    state.volume.desc = domain;
    state.obstacle_occupancy = obstacle_mask;
    scene.preset = &preset;
    scene.runtime_visual.mesh_previews.valid_contract = true;
    scene.runtime_visual.mesh_previews.instance_count = 1;
    instance = &scene.runtime_visual.mesh_previews.instances[0];
    instance->runtime_path_resolved = true;
    instance->fluid_obstacle_enabled = true;
    instance->fluid_emitter_enabled = true;
    instance->transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", "surface_heat_emitter");
    snprintf(instance->runtime_mesh_path, sizeof(instance->runtime_mesh_path), "%s", mesh_path);
    preset.emitter_count = 1u;
    preset.emitters[0].attached_object = -1;
    preset.emitters[0].attached_import = -1;
    preset.emitters[0].attached_runtime_mesh_enabled = true;
    preset.emitters[0].attached_runtime_mesh = 0;

    backend_3d_scaffold_rasterize_runtime_mesh_asset_obstacles(&state, &scene);
    CHECK_TRUE(obstacle_mask[test_index(&domain, 4, 4, 4)] == 0u);

    unlink(mesh_path);
    rmdir(temp_dir);
    return obstacle_mask[test_index(&domain, 4, 4, 4)] == 0u;
}

int main(void) {
    CHECK_TRUE(test_default_runtime_mesh_instances_are_enabled());
    CHECK_TRUE(test_visual_only_instances_do_not_voxelize());
    CHECK_TRUE(test_mesh_emitter_instances_do_not_voxelize_as_obstacles());
    CHECK_TRUE(test_closed_runtime_mesh_voxelizes_from_actual_asset());
    CHECK_TRUE(test_preview_bounds_do_not_drive_solver_truth());
    CHECK_TRUE(test_out_of_domain_runtime_mesh_does_not_clamp_to_boundary());
    CHECK_TRUE(test_high_triangle_runtime_mesh_uses_acceleration_and_solver_truth());
    CHECK_TRUE(test_runtime_mesh_prepared_cache_reuses_loaded_mesh());
    CHECK_TRUE(test_runtime_mesh_budget_fallback_reports_reason());
    CHECK_TRUE(test_backend_adapter_marks_scene_mesh_instance_obstacles());
    CHECK_TRUE(test_backend_adapter_skips_mesh_instances_attached_as_emitters());
    if (g_failures != 0) {
        fprintf(stderr, "runtime_mesh_obstacle_proxy_contract_test: %d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stdout, "runtime_mesh_obstacle_proxy_contract_test: success\n");
    return 0;
}
