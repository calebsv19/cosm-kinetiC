#include "app/sim_runtime_emitter.h"
#include "app/sim_runtime_3d_space.h"

#include <math.h>
#include <string.h>

static float clamp_unit(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int max3_int(int a, int b, int c) {
    int value = (a > b) ? a : b;
    return (c > value) ? c : value;
}

static FluidEmitter3DSourceMode sanitize_source_mode_3d(FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return mode;
    default:
        break;
    }
    return EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
}

static FluidEmitter3DSurface sanitize_surface_3d(FluidEmitter3DSurface surface) {
    switch (surface) {
    case EMITTER_3D_SURFACE_AUTO:
    case EMITTER_3D_SURFACE_TOP:
    case EMITTER_3D_SURFACE_BOTTOM:
    case EMITTER_3D_SURFACE_LEFT:
    case EMITTER_3D_SURFACE_RIGHT:
    case EMITTER_3D_SURFACE_FRONT:
    case EMITTER_3D_SURFACE_BACK:
    case EMITTER_3D_SURFACE_ALL_FACES:
        return surface;
    default:
        break;
    }
    return EMITTER_3D_SURFACE_AUTO;
}

static FluidEmitter3DObstacleMode sanitize_obstacle_mode_3d(FluidEmitter3DObstacleMode mode) {
    switch (mode) {
    case EMITTER_3D_OBSTACLE_MODE_AUTO:
    case EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED:
    case EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED:
        return mode;
    default:
        break;
    }
    return EMITTER_3D_OBSTACLE_MODE_AUTO;
}

static SimRuntimeEmitterFootprintKind resolve_attached_object_footprint(FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_PATCH;
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_SHELL;
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_HEATED_OBSTACLE;
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
    default:
        break;
    }
    return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_OCCUPANCY;
}

static SimRuntimeEmitterFootprintKind resolve_attached_import_footprint(FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_PATCH;
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_SHELL;
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_HEATED_OBSTACLE;
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
    default:
        break;
    }
    return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_OCCUPANCY;
}

static SimRuntimeEmitterFootprintKind resolve_attached_runtime_mesh_footprint(
    FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_SURFACE_PATCH;
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_SURFACE_SHELL;
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_HEATED_OBSTACLE;
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
    default:
        break;
    }
    return SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_OCCUPANCY;
}

static void normalize_dir3(float *x, float *y, float *z, bool *out_has_magnitude) {
    float len = 0.0f;
    bool has_magnitude = false;
    if (!x || !y || !z) return;
    len = sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    if (len > 0.0001f) {
        *x /= len;
        *y /= len;
        *z /= len;
        has_magnitude = true;
    } else {
        *x = 0.0f;
        *y = 0.0f;
        *z = 0.0f;
    }
    if (out_has_magnitude) *out_has_magnitude = has_magnitude;
}

bool sim_runtime_emitter_resolve(const FluidScenePreset *preset,
                                 size_t emitter_index,
                                 SimRuntimeEmitterResolved *out_resolved) {
    const FluidEmitter *emitter = NULL;
    SimRuntimeEmitterResolved resolved = {0};
    float resolved_position_z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0f;
    float resolved_radius [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0f;
    if (!preset || !out_resolved) return false;
    if (emitter_index >= preset->emitter_count || emitter_index >= MAX_FLUID_EMITTERS) return false;

    emitter = &preset->emitters[emitter_index];
    resolved_position_z = emitter->position_z;
    resolved_radius = emitter->radius > 0.0f ? emitter->radius : 0.0f;
    resolved.emitter_index = emitter_index;
    resolved.type = emitter->type;
    resolved.position_x = clamp_unit(emitter->position_x);
    resolved.position_y = clamp_unit(emitter->position_y);
    resolved.position_z = resolved_position_z;
    resolved.radius = resolved_radius;
    resolved.strength = emitter->strength;
    resolved.dir_x = emitter->dir_x;
    resolved.dir_y = emitter->dir_y;
    resolved.dir_z = emitter->dir_z;
    resolved.source_mode_3d = sanitize_source_mode_3d(emitter->source_mode_3d);
    resolved.surface_3d = sanitize_surface_3d(emitter->surface_3d);
    resolved.obstacle_mode_3d = sanitize_obstacle_mode_3d(emitter->obstacle_mode_3d);
    resolved.thermal_buoyancy_3d = emitter->thermal_buoyancy_3d;
    normalize_dir3(&resolved.dir_x,
                   &resolved.dir_y,
                   &resolved.dir_z,
                   &resolved.direction_has_magnitude);

    resolved.attached_object = emitter->attached_object;
    resolved.attached_import = emitter->attached_import;
    resolved.attached_runtime_mesh_enabled = emitter->attached_runtime_mesh_enabled;
    resolved.attached_runtime_mesh = emitter->attached_runtime_mesh;
    resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_FREE;
    resolved.primary_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE;
    resolved.fallback_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE;

    if (resolved.attached_runtime_mesh_enabled && resolved.attached_runtime_mesh >= 0) {
        resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_RUNTIME_MESH;
        resolved.primary_footprint = resolve_attached_runtime_mesh_footprint(resolved.source_mode_3d);
    } else if (resolved.attached_import >= 0) {
        resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT;
        resolved.primary_footprint = resolve_attached_import_footprint(resolved.source_mode_3d);
    } else if (resolved.attached_object >= 0) {
        resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT;
        resolved.primary_footprint = resolve_attached_object_footprint(resolved.source_mode_3d);
    }

    *out_resolved = resolved;
    return true;
}

bool sim_runtime_emitter_resolve_3d_placement(const SimRuntime3DDomainDesc *domain,
                                              const SimRuntimeEmitterResolved *resolved,
                                              SimRuntimeEmitterPlacement3D *out_placement) {
    SimRuntimeEmitterPlacement3D placement = {0};
    int max_axis_cells = 0;
    float position_z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0f;
    float radius [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0f;
    if (!domain || !resolved || !out_placement) return false;
    if (domain->grid_w <= 0 || domain->grid_h <= 0 || domain->grid_d <= 0) return false;
    position_z = resolved->position_z;
    radius = resolved->radius;

    placement.center_x = clamp_int_value((int)lroundf(resolved->position_x * (float)(domain->grid_w - 1)),
                                         0,
                                         domain->grid_w - 1);
    placement.center_y = clamp_int_value((int)lroundf(resolved->position_y * (float)(domain->grid_h - 1)),
                                         0,
                                         domain->grid_h - 1);
    placement.center_z = sim_runtime_3d_space_world_to_grid_axis(position_z,
                                                                 domain->world_min_z,
                                                                 domain->voxel_size,
                                                                 domain->grid_d);

    max_axis_cells = max3_int(domain->grid_w, domain->grid_h, domain->grid_d);
    placement.radius_cells = (int)ceilf(radius * (float)max_axis_cells);
    if (placement.radius_cells < 1) placement.radius_cells = 1;

    placement.min_x = clamp_int_value(placement.center_x - placement.radius_cells, 0, domain->grid_w - 1);
    placement.max_x = clamp_int_value(placement.center_x + placement.radius_cells, 0, domain->grid_w - 1);
    placement.min_y = clamp_int_value(placement.center_y - placement.radius_cells, 0, domain->grid_h - 1);
    placement.max_y = clamp_int_value(placement.center_y + placement.radius_cells, 0, domain->grid_h - 1);
    placement.min_z = clamp_int_value(placement.center_z - placement.radius_cells, 0, domain->grid_d - 1);
    placement.max_z = clamp_int_value(placement.center_z + placement.radius_cells, 0, domain->grid_d - 1);

    *out_placement = placement;
    return true;
}

const char *sim_runtime_emitter_source_kind_label(SimRuntimeEmitterSourceKind kind) {
    switch (kind) {
    case SIM_RUNTIME_EMITTER_SOURCE_FREE:
        return "free";
    case SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT:
        return "attached-object";
    case SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT:
        return "attached-import";
    case SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_RUNTIME_MESH:
        return "attached-runtime-mesh";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_emitter_footprint_kind_label(SimRuntimeEmitterFootprintKind kind) {
    switch (kind) {
    case SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE:
        return "radial-sphere";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_OCCUPANCY:
        return "attached-object-occupancy";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_OCCUPANCY:
        return "attached-import-occupancy";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_PATCH:
        return "attached-object-surface-patch";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_PATCH:
        return "attached-import-surface-patch";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_SHELL:
        return "attached-object-surface-shell";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_SHELL:
        return "attached-import-surface-shell";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_HEATED_OBSTACLE:
        return "attached-object-heated-obstacle";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_HEATED_OBSTACLE:
        return "attached-import-heated-obstacle";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_OCCUPANCY:
        return "attached-runtime-mesh-occupancy";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_SURFACE_PATCH:
        return "attached-runtime-mesh-surface-patch";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_SURFACE_SHELL:
        return "attached-runtime-mesh-surface-shell";
    case SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_RUNTIME_MESH_HEATED_OBSTACLE:
        return "attached-runtime-mesh-heated-obstacle";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_emitter_3d_source_mode_label(FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
        return "legacy-compat";
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
        return "volume-fill";
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        return "surface-patch";
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        return "surface-shell";
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return "heated-obstacle";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_emitter_3d_surface_label(FluidEmitter3DSurface surface) {
    switch (surface) {
    case EMITTER_3D_SURFACE_AUTO:
        return "auto";
    case EMITTER_3D_SURFACE_TOP:
        return "top";
    case EMITTER_3D_SURFACE_BOTTOM:
        return "bottom";
    case EMITTER_3D_SURFACE_LEFT:
        return "left";
    case EMITTER_3D_SURFACE_RIGHT:
        return "right";
    case EMITTER_3D_SURFACE_FRONT:
        return "front";
    case EMITTER_3D_SURFACE_BACK:
        return "back";
    case EMITTER_3D_SURFACE_ALL_FACES:
        return "all-faces";
    default:
        break;
    }
    return "unknown";
}

const char *sim_runtime_emitter_3d_obstacle_mode_label(FluidEmitter3DObstacleMode mode) {
    switch (mode) {
    case EMITTER_3D_OBSTACLE_MODE_AUTO:
        return "auto";
    case EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED:
        return "clear-attached";
    case EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED:
        return "retain-attached";
    default:
        break;
    }
    return "unknown";
}
