#include "app/sim_runtime_mesh_diagnostics.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void mesh_diag_message(PhysicsSimRuntimeMeshDiagnostic *diag, const char *message) {
    if (!diag || !message) return;
    snprintf(diag->diagnostics, sizeof(diag->diagnostics), "%s", message);
}

const char *physics_sim_runtime_mesh_fluid_role_label(PhysicsSimRuntimeMeshFluidRole role) {
    switch (role) {
    case PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_SOLID:
        return "solid";
    case PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_VISUAL_ONLY:
        return "visual-only";
    case PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_EMITTER:
        return "emitter";
    case PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED:
    default:
        return "disabled";
    }
}

void physics_sim_runtime_mesh_diagnostic_init(PhysicsSimRuntimeMeshDiagnostic *diag) {
    if (!diag) return;
    memset(diag, 0, sizeof(*diag));
    diag->instance_index = -1;
    diag->scene_object_index = -1;
    diag->role = PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED;
    snprintf(diag->role_label, sizeof(diag->role_label), "%s", "disabled");
    mesh_diag_message(diag, "ok");
}

static PhysicsSimRuntimeMeshFluidRole resolve_role(const PhysicsSimRuntimeMeshPreviewInstance *instance) {
    if (!instance) return PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED;
    if (instance->fluid_emitter_enabled) return PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_EMITTER;
    if (physics_sim_runtime_mesh_obstacle_instance_enabled(instance)) {
        return PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_SOLID;
    }
    if (instance->runtime_path_resolved && instance->runtime_mesh_path[0] &&
        (strcmp(instance->fluid_behavior, "visual_only") == 0 ||
         strcmp(instance->fluid_behavior, "none") == 0 ||
         !instance->fluid_obstacle_enabled)) {
        return PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_VISUAL_ONLY;
    }
    return PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED;
}

static PhysicsSimRuntimeMeshVoxelizeMode emitter_voxelize_mode(
    const PhysicsSimRuntimeMeshPreviewInstance *instance) {
    if (!instance) return PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL;
    if (instance->emitter_source_mode_3d == EMITTER_3D_SOURCE_MODE_VOLUME_FILL ||
        instance->emitter_source_mode_3d == EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT) {
        return PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SOLID_VOLUME;
    }
    return PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL;
}

static FluidEmitter3DSurface resolve_surface(FluidEmitter3DSurface surface) {
    return surface == EMITTER_3D_SURFACE_AUTO ? EMITTER_3D_SURFACE_ALL_FACES : surface;
}

static bool mesh_diag_cell_matches_surface(const PhysicsSimRuntimeMeshObstacleReport *report,
                                           FluidEmitter3DSurface surface,
                                           int x,
                                           int y,
                                           int z) {
    if (!report) return false;
    switch (resolve_surface(surface)) {
    case EMITTER_3D_SURFACE_TOP:
        return z == report->max_z;
    case EMITTER_3D_SURFACE_BOTTOM:
        return z == report->min_z;
    case EMITTER_3D_SURFACE_LEFT:
        return x == report->min_x;
    case EMITTER_3D_SURFACE_RIGHT:
        return x == report->max_x;
    case EMITTER_3D_SURFACE_FRONT:
        return y == report->max_y;
    case EMITTER_3D_SURFACE_BACK:
        return y == report->min_y;
    case EMITTER_3D_SURFACE_ALL_FACES:
    case EMITTER_3D_SURFACE_AUTO:
    default:
        return true;
    }
}

static size_t count_mask_cells(const uint8_t *mask,
                               const SimRuntime3DDomainDesc *domain,
                               const PhysicsSimRuntimeMeshObstacleReport *report,
                               const PhysicsSimRuntimeMeshPreviewInstance *instance,
                               bool emitter) {
    size_t count = 0u;
    if (!mask || !domain || !report) return 0u;
    for (int z = report->min_z; z <= report->max_z; ++z) {
        for (int y = report->min_y; y <= report->max_y; ++y) {
            for (int x = report->min_x; x <= report->max_x; ++x) {
                size_t idx = sim_runtime_3d_volume_index(domain, x, y, z);
                if (!mask[idx]) continue;
                if (emitter && instance &&
                    instance->emitter_source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_PATCH &&
                    !mesh_diag_cell_matches_surface(report, instance->emitter_surface_3d, x, y, z)) {
                    continue;
                }
                count++;
            }
        }
    }
    return count;
}

static void copy_report_bounds(PhysicsSimRuntimeMeshDiagnostic *diag,
                               const PhysicsSimRuntimeMeshObstacleReport *report) {
    if (!diag || !report) return;
    diag->has_local_bounds = report->has_local_bounds;
    diag->has_world_bounds = report->has_world_bounds;
    diag->local_bounds_min = report->local_bounds_min;
    diag->local_bounds_max = report->local_bounds_max;
    diag->world_bounds_min = report->world_bounds_min;
    diag->world_bounds_max = report->world_bounds_max;
    diag->runtime_vertex_count = report->runtime_vertex_count;
    diag->runtime_triangle_count = report->runtime_triangle_count;
    diag->file_signature_valid = report->file_signature_valid;
    diag->runtime_mesh_file_mtime_ns = report->runtime_mesh_file_mtime_ns;
    diag->runtime_mesh_file_size_bytes = report->runtime_mesh_file_size_bytes;
    diag->used_triangle_accel = report->used_triangle_accel;
    diag->accel_triangle_count = report->accel_triangle_count;
    diag->accel_node_count = report->accel_node_count;
    diag->accel_leaf_count = report->accel_leaf_count;
    diag->accel_max_depth = report->accel_max_depth;
    diag->budget_limited = report->budget_limited;
    diag->wind_projected_area_yz = report->wind_projected_area_yz;
    diag->bounds_volume = report->bounds_volume;
    snprintf(diag->fallback_reason,
             sizeof(diag->fallback_reason),
             "%s",
             report->fallback_reason);
}

static bool bounds_intersect_domain(CoreObjectVec3 min,
                                    CoreObjectVec3 max,
                                    const SimRuntime3DDomainDesc *domain) {
    if (!domain) return false;
    return max.x >= domain->world_min_x && min.x <= domain->world_max_x &&
           max.y >= domain->world_min_y && min.y <= domain->world_max_y &&
           max.z >= domain->world_min_z && min.z <= domain->world_max_z;
}

static bool bounds_inside_domain(CoreObjectVec3 min,
                                 CoreObjectVec3 max,
                                 const SimRuntime3DDomainDesc *domain) {
    if (!domain) return false;
    return min.x >= domain->world_min_x && max.x <= domain->world_max_x &&
           min.y >= domain->world_min_y && max.y <= domain->world_max_y &&
           min.z >= domain->world_min_z && max.z <= domain->world_max_z;
}

static void update_domain_overlap_diagnostic(PhysicsSimRuntimeMeshDiagnostic *diag,
                                             const SimRuntime3DDomainDesc *domain) {
    if (!diag || !domain || !diag->has_world_bounds) return;
    diag->domain_overlap_tested = true;
    diag->world_bounds_intersect_domain =
        bounds_intersect_domain(diag->world_bounds_min, diag->world_bounds_max, domain);
    diag->world_bounds_inside_domain =
        bounds_inside_domain(diag->world_bounds_min, diag->world_bounds_max, domain);
    if (!diag->world_bounds_intersect_domain) {
        mesh_diag_message(diag, "mesh bounds outside active domain");
    }
}

bool physics_sim_runtime_mesh_diagnostic_collect(
    const PhysicsSimRuntimeMeshPreviewSet *set,
    int instance_index,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshDiagnostic *out_diag) {
    const PhysicsSimRuntimeMeshPreviewInstance *instance = NULL;
    PhysicsSimRuntimeMeshDiagnostic diag;

    if (!out_diag) return false;
    physics_sim_runtime_mesh_diagnostic_init(&diag);
    if (!set || !set->valid_contract || instance_index < 0 || instance_index >= set->instance_count) {
        mesh_diag_message(&diag, "mesh instance unavailable");
        *out_diag = diag;
        return false;
    }

    instance = &set->instances[instance_index];
    return physics_sim_runtime_mesh_diagnostic_collect_instance(instance,
                                                               instance_index,
                                                               domain,
                                                               out_diag);
}

bool physics_sim_runtime_mesh_diagnostic_collect_instance(
    const PhysicsSimRuntimeMeshPreviewInstance *instance,
    int instance_index,
    const SimRuntime3DDomainDesc *domain,
    PhysicsSimRuntimeMeshDiagnostic *out_diag) {
    PhysicsSimRuntimeMeshObstacleReport report;
    PhysicsSimRuntimeMeshDiagnostic diag;
    uint8_t *mask = NULL;
    PhysicsSimRuntimeMeshVoxelizeMode mode = PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL;
    bool voxelized = false;

    if (!out_diag) return false;
    physics_sim_runtime_mesh_diagnostic_init(&diag);
    if (!instance || instance_index < 0) {
        mesh_diag_message(&diag, "mesh instance unavailable");
        *out_diag = diag;
        return false;
    }

    diag.valid = true;
    diag.instance_index = instance_index;
    diag.scene_object_index = instance->scene_object_index;
    diag.role = resolve_role(instance);
    snprintf(diag.role_label,
             sizeof(diag.role_label),
             "%s",
             physics_sim_runtime_mesh_fluid_role_label(diag.role));
    snprintf(diag.object_id, sizeof(diag.object_id), "%s", instance->object_id);
    snprintf(diag.asset_id, sizeof(diag.asset_id), "%s", instance->asset_id);
    snprintf(diag.runtime_mesh_path, sizeof(diag.runtime_mesh_path), "%s", instance->runtime_mesh_path);
    snprintf(diag.runtime_mesh_path_hint,
             sizeof(diag.runtime_mesh_path_hint),
             "%s",
             instance->runtime_mesh_path_hint);
    snprintf(diag.preview_path, sizeof(diag.preview_path), "%s", instance->preview_path);
    diag.runtime_path_resolved = instance->runtime_path_resolved;
    diag.runtime_path_recovered = instance->runtime_path_recovered;
    diag.transform_position = instance->transform_position;
    diag.transform_rotation_deg = instance->transform_rotation_deg;
    diag.transform_scale = instance->transform_scale;
    if (instance->has_world_bounds) {
        diag.has_world_bounds = true;
        diag.world_bounds_min = instance->world_bounds_min;
        diag.world_bounds_max = instance->world_bounds_max;
    }

    if (domain && domain->cell_count > 0u) {
        mask = (uint8_t *)calloc(domain->cell_count, sizeof(uint8_t));
        if (!mask) {
            mesh_diag_message(&diag, "failed to allocate diagnostic mask");
            *out_diag = diag;
            return false;
        }
        if (diag.role == PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_SOLID) {
            voxelized = physics_sim_runtime_mesh_obstacle_voxelize_instance(instance,
                                                                            domain,
                                                                            mask,
                                                                            domain->cell_count,
                                                                            &report);
            if (voxelized) {
                diag.obstacle_voxel_count = report.solid_cell_count;
                copy_report_bounds(&diag, &report);
            } else {
                mesh_diag_message(&diag, report.diagnostics);
            }
        } else if (diag.role == PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_EMITTER) {
            mode = emitter_voxelize_mode(instance);
            voxelized = physics_sim_runtime_mesh_obstacle_voxelize_instance_mode(instance,
                                                                                 domain,
                                                                                 mask,
                                                                                 domain->cell_count,
                                                                                 mode,
                                                                                 &report);
            if (voxelized) {
                diag.emitter_footprint_voxel_count =
                    count_mask_cells(mask, domain, &report, instance, true);
                copy_report_bounds(&diag, &report);
            } else {
                mesh_diag_message(&diag, report.diagnostics);
            }
        } else if (diag.role == PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_VISUAL_ONLY) {
            voxelized = physics_sim_runtime_mesh_obstacle_voxelize_instance_mode(
                instance,
                domain,
                mask,
                domain->cell_count,
                PHYSICS_SIM_RUNTIME_MESH_VOXELIZE_SURFACE_SHELL,
                &report);
            if (voxelized) {
                copy_report_bounds(&diag, &report);
            } else {
                mesh_diag_message(&diag, report.diagnostics);
            }
        }
        free(mask);
    }
    update_domain_overlap_diagnostic(&diag, domain);
    if (!instance->runtime_path_resolved || !instance->runtime_mesh_path[0]) {
        mesh_diag_message(&diag, "runtime mesh path unresolved");
    } else if (diag.runtime_path_recovered && strcmp(diag.diagnostics, "ok") == 0) {
        mesh_diag_message(&diag, "runtime mesh path recovered");
    } else if (diag.role != PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_DISABLED &&
               domain &&
               diag.has_world_bounds &&
               diag.world_bounds_intersect_domain &&
               !voxelized &&
               strcmp(diag.diagnostics, "ok") == 0) {
        mesh_diag_message(&diag, "mesh produced no voxel footprint");
    }

    *out_diag = diag;
    return true;
}
