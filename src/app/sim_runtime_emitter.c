#include "app/sim_runtime_emitter.h"

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
    if (!preset || !out_resolved) return false;
    if (emitter_index >= preset->emitter_count || emitter_index >= MAX_FLUID_EMITTERS) return false;

    emitter = &preset->emitters[emitter_index];
    resolved.emitter_index = emitter_index;
    resolved.type = emitter->type;
    resolved.position_x = clamp_unit(emitter->position_x);
    resolved.position_y = clamp_unit(emitter->position_y);
    resolved.position_z = clamp_unit(emitter->position_z);
    resolved.radius = emitter->radius > 0.0f ? emitter->radius : 0.0f;
    resolved.strength = emitter->strength;
    resolved.dir_x = emitter->dir_x;
    resolved.dir_y = emitter->dir_y;
    resolved.dir_z = emitter->dir_z;
    normalize_dir3(&resolved.dir_x,
                   &resolved.dir_y,
                   &resolved.dir_z,
                   &resolved.direction_has_magnitude);

    resolved.attached_object = emitter->attached_object;
    resolved.attached_import = emitter->attached_import;
    resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_FREE;
    resolved.primary_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE;
    resolved.fallback_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE;

    if (resolved.attached_import >= 0) {
        resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT;
        resolved.primary_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_OCCUPANCY;
    } else if (resolved.attached_object >= 0) {
        resolved.source_kind = SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT;
        resolved.primary_footprint = SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_OCCUPANCY;
    }

    *out_resolved = resolved;
    return true;
}

bool sim_runtime_emitter_resolve_3d_placement(const SimRuntime3DDomainDesc *domain,
                                              const SimRuntimeEmitterResolved *resolved,
                                              SimRuntimeEmitterPlacement3D *out_placement) {
    SimRuntimeEmitterPlacement3D placement = {0};
    int max_axis_cells = 0;
    if (!domain || !resolved || !out_placement) return false;
    if (domain->grid_w <= 0 || domain->grid_h <= 0 || domain->grid_d <= 0) return false;

    placement.center_x = clamp_int_value((int)lroundf(resolved->position_x * (float)(domain->grid_w - 1)),
                                         0,
                                         domain->grid_w - 1);
    placement.center_y = clamp_int_value((int)lroundf(resolved->position_y * (float)(domain->grid_h - 1)),
                                         0,
                                         domain->grid_h - 1);
    placement.center_z = clamp_int_value((int)lroundf(resolved->position_z * (float)(domain->grid_d - 1)),
                                         0,
                                         domain->grid_d - 1);

    max_axis_cells = max3_int(domain->grid_w, domain->grid_h, domain->grid_d);
    placement.radius_cells = (int)ceilf(resolved->radius * (float)max_axis_cells);
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
    default:
        break;
    }
    return "unknown";
}
