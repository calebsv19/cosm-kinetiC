#include "app/sim_runtime_3d_footprint.h"

#include <math.h>

static const double SCAFFOLD_IMPORT_DESIRED_FIT = 0.25;

static double clamp_positive(double value, double fallback) {
    return value > 0.0 ? value : fallback;
}

static double axis_span(double world_min, double world_max) {
    double span = world_max - world_min;
    return span > 0.0 ? span : 1.0;
}

static double reference_xy_span(const SimRuntime3DDomainDesc *desc) {
    double span_x = 0.0;
    double span_y = 0.0;
    if (!desc) return 1.0;
    span_x = axis_span(desc->world_min_x, desc->world_max_x);
    span_y = axis_span(desc->world_min_y, desc->world_max_y);
    return span_x < span_y ? span_x : span_y;
}

static double resolve_world_size(double authored_size, double world_span) {
    if (!(authored_size > 0.0)) return 0.0;
    if (authored_size <= 1.0) return authored_size * world_span;
    return authored_size;
}

static int world_size_to_cells(const SimRuntime3DDomainDesc *desc,
                               double world_size,
                               int fallback_cells) {
    int cells = 0;
    double ratio = 0.0;
    if (!desc || !(desc->voxel_size > 0.0)) return fallback_cells;
    ratio = world_size / (double)desc->voxel_size;
    cells = (int)ceil(ratio - 1e-6);
    return cells > 0 ? cells : fallback_cells;
}

int sim_runtime_3d_footprint_emitter_radius_cells(const SimRuntime3DDomainDesc *desc,
                                                  float emitter_radius) {
    double world_radius = 0.0;
    if (!desc) return 1;
    world_radius = resolve_world_size((double)emitter_radius, reference_xy_span(desc));
    return world_size_to_cells(desc, clamp_positive(world_radius, 0.0), 1);
}

int sim_runtime_3d_footprint_object_sphere_radius_cells(const SimRuntime3DDomainDesc *desc,
                                                        const PresetObject *object) {
    double world_radius_xy = 0.0;
    double world_radius_z = 0.0;
    double size_z = 0.0;
    if (!desc || !object) return 1;
    size_z = clamp_positive((double)object->size_z, (double)object->size_x);
    world_radius_xy = resolve_world_size((double)object->size_x, reference_xy_span(desc));
    world_radius_z = resolve_world_size(size_z, axis_span(desc->world_min_z, desc->world_max_z));
    if (world_radius_z > world_radius_xy) world_radius_xy = world_radius_z;
    return world_size_to_cells(desc, clamp_positive(world_radius_xy, 0.0), 1);
}

bool sim_runtime_3d_footprint_object_box_half_extents_cells(
    const SimRuntime3DDomainDesc *desc,
    const PresetObject *object,
    SimRuntime3DFootprintHalfExtents *out_half_extents) {
    SimRuntime3DFootprintHalfExtents half_extents = {0};
    double size_y = 0.0;
    double size_z = 0.0;
    if (!desc || !object || !out_half_extents) return false;
    size_y = clamp_positive((double)object->size_y, (double)object->size_x);
    size_z = clamp_positive((double)object->size_z, (double)object->size_x);
    half_extents.half_x_cells = world_size_to_cells(
        desc,
        resolve_world_size((double)object->size_x, axis_span(desc->world_min_x, desc->world_max_x)),
        1);
    half_extents.half_y_cells = world_size_to_cells(
        desc,
        resolve_world_size(size_y, axis_span(desc->world_min_y, desc->world_max_y)),
        1);
    half_extents.half_z_cells = world_size_to_cells(
        desc,
        resolve_world_size(size_z, axis_span(desc->world_min_z, desc->world_max_z)),
        1);
    *out_half_extents = half_extents;
    return true;
}

bool sim_runtime_3d_footprint_import_box_half_extents_cells(
    const SimRuntime3DDomainDesc *desc,
    const ImportedShape *imp,
    SimRuntime3DFootprintHalfExtents *out_half_extents) {
    SimRuntime3DFootprintHalfExtents half_extents = {0};
    double scale = 1.0;
    double scale_factor = 0.0;
    if (!desc || !imp || !out_half_extents) return false;
    scale = clamp_positive((double)imp->scale, 1.0);
    scale_factor = scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5;
    half_extents.half_x_cells = world_size_to_cells(
        desc,
        axis_span(desc->world_min_x, desc->world_max_x) * scale_factor,
        1);
    half_extents.half_y_cells = world_size_to_cells(
        desc,
        axis_span(desc->world_min_y, desc->world_max_y) * scale_factor,
        1);
    half_extents.half_z_cells = world_size_to_cells(
        desc,
        axis_span(desc->world_min_z, desc->world_max_z) * scale_factor,
        1);
    *out_half_extents = half_extents;
    return true;
}
