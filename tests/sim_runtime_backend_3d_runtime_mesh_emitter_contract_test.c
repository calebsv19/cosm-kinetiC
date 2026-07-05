#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_mesh_diagnostics.h"
#include "core_mesh_asset.h"
#include "sim_runtime_backend_3d_test_support.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
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
    if (core_mesh_asset_runtime_contract_set_asset_id(&document.contract, "cube_runtime_mesh_emitter").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(&document.contract, "cube_runtime_mesh_emitter").code != CORE_OK ||
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

static bool any_velocity_non_zero(const SimRuntimeBackend3DScaffoldTestView *view) {
    if (!view) return false;
    for (size_t i = 0u; i < view->volume.desc.cell_count; ++i) {
        if (fabsf(view->volume.velocity_x[i]) > 1e-6f ||
            fabsf(view->volume.velocity_y[i]) > 1e-6f ||
            fabsf(view->volume.velocity_z[i]) > 1e-6f) {
            return true;
        }
    }
    return false;
}

static bool test_runtime_mesh_boundary_flow_emitter_uses_runtime_mesh_footprint(void) {
    char temp_dir[256] = {0};
    char mesh_path[512] = {0};
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    PhysicsSimRuntimeMeshPreviewInstance *instance = NULL;
    PhysicsSimRuntimeMeshDiagnostic diag;
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView view = {0};
    SimRuntimeBackendReport report = {0};
    SimRuntime3DDomainDesc domain = {0};
    SceneState scene = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    bool ok = false;

    snprintf(temp_dir,
             sizeof(temp_dir),
             "/private/tmp/physics_sim_runtime_mesh_emitter_%ld",
             (long)getpid());
    if (mkdir(temp_dir, 0700) != 0) return false;
    snprintf(mesh_path, sizeof(mesh_path), "%s/cube.runtime.json", temp_dir);
    if (!write_cube_runtime_mesh(mesh_path)) {
        rmdir(temp_dir);
        return false;
    }

    cfg.quality_index = 1;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_velocity_multiplier = 1.0f;

    visual.valid = true;
    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){-1.0, -1.0, -1.0};
    visual.scene_domain.max = (CoreObjectVec3){1.0, 1.0, 1.0};
    visual.mesh_previews.valid_contract = true;
    visual.mesh_previews.instance_count = 1;
    instance = &visual.mesh_previews.instances[0];
    memset(instance, 0, sizeof(*instance));
    instance->scene_object_index = 0;
    instance->runtime_path_resolved = true;
    instance->fluid_obstacle_enabled = false;
    instance->fluid_emitter_enabled = true;
    instance->emitter_type = EMITTER_VELOCITY_JET;
    instance->emitter_source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
    instance->emitter_surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
    instance->emitter_obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
    instance->emitter_strength = 24.0f;
    instance->emitter_radius = 0.08f;
    instance->emitter_direction = (CoreObjectVec3){1.0, 0.0, 0.0};
    instance->transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(instance->object_id, sizeof(instance->object_id), "%s", "mesh_flow");
    snprintf(instance->asset_id, sizeof(instance->asset_id), "%s", "cube_runtime_mesh_emitter");
    snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", "boundary_flow_emitter");
    snprintf(instance->runtime_mesh_path, sizeof(instance->runtime_mesh_path), "%s", mesh_path);

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.emitter_count = 1u;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.0f,
        .position_y = 0.0f,
        .position_z = 0.0f,
        .radius = 0.08f,
        .strength = 24.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
        .attached_runtime_mesh_enabled = true,
        .attached_runtime_mesh = 0,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL,
        .surface_3d = EMITTER_3D_SURFACE_ALL_FACES,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED,
        .thermal_buoyancy_3d = 0.0f,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) goto cleanup;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    if (!sim_runtime_backend_get_domain_desc_3d(backend, &domain)) goto cleanup;
    if (!physics_sim_runtime_mesh_diagnostic_collect(&visual.mesh_previews, 0, &domain, &diag)) goto cleanup;
    if (strcmp(diag.role_label, "emitter") != 0) goto cleanup;
    if (diag.emitter_footprint_voxel_count == 0u) goto cleanup;
    if (diag.runtime_vertex_count != 8u || diag.runtime_triangle_count != 12u) goto cleanup;
    if (!(diag.wind_projected_area_yz > 0.9)) goto cleanup;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_get_report(backend, &report)) goto cleanup;
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &view)) goto cleanup;

    ok = report.emitter_step_attached_emitters_applied == 1u &&
         report.emitter_step_affected_cells > 0u &&
         report.emitter_step_velocity_magnitude_delta > 0.0f &&
         any_velocity_non_zero(&view);

cleanup:
    if (backend) sim_runtime_backend_destroy(backend);
    unlink(mesh_path);
    rmdir(temp_dir);
    return ok;
}

int main(void) {
    if (!test_runtime_mesh_boundary_flow_emitter_uses_runtime_mesh_footprint()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_runtime_mesh_emitter_contract_test: runtime mesh emitter failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_runtime_mesh_emitter_contract_test: success\n");
    return 0;
}
