#include "app/sim_runtime_backend_3d_emitter_shapes.h"

#include "app/scene_state.h"
#include "app/sim_runtime_3d_anchor.h"
#include "app/sim_runtime_3d_footprint.h"
#include "app/sim_runtime_3d_space.h"

#include <math.h>

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void backend_3d_scaffold_fill_sphere_bounds(const SimRuntime3DDomainDesc *desc,
                                            SimRuntimeEmitterPlacement3D *placement) {
    if (!desc || !placement) return;
    placement->min_x = clamp_int_value(placement->center_x - placement->radius_cells, 0, desc->grid_w - 1);
    placement->max_x = clamp_int_value(placement->center_x + placement->radius_cells, 0, desc->grid_w - 1);
    placement->min_y = clamp_int_value(placement->center_y - placement->radius_cells, 0, desc->grid_h - 1);
    placement->max_y = clamp_int_value(placement->center_y + placement->radius_cells, 0, desc->grid_h - 1);
    placement->min_z = clamp_int_value(placement->center_z - placement->radius_cells, 0, desc->grid_d - 1);
    placement->max_z = clamp_int_value(placement->center_z + placement->radius_cells, 0, desc->grid_d - 1);
}

bool backend_3d_scaffold_resolve_emitter_placement(const SceneState *scene,
                                                   const SimRuntime3DDomainDesc *desc,
                                                   const SimRuntimeEmitterResolved *emitter,
                                                   SimRuntimeEmitterPlacement3D *out_placement) {
    SimRuntimeEmitterPlacement3D placement = {0};
    CoreObjectVec3 world = {0};
    if (!scene || !desc || !emitter || !out_placement) return false;
    if (!sim_runtime_3d_anchor_resolve_resolved_emitter_world_anchor(scene, desc, emitter, &world)) {
        return false;
    }

    placement.center_x = sim_runtime_3d_space_world_to_grid_axis(
        world.x, desc->world_min_x, desc->voxel_size, desc->grid_w);
    placement.center_y = sim_runtime_3d_space_world_to_grid_axis(
        world.y, desc->world_min_y, desc->voxel_size, desc->grid_h);
    placement.center_z = sim_runtime_3d_space_world_to_grid_axis(
        world.z, desc->world_min_z, desc->voxel_size, desc->grid_d);
    placement.radius_cells = sim_runtime_3d_footprint_emitter_radius_cells(desc, emitter->radius);
    backend_3d_scaffold_fill_sphere_bounds(desc, &placement);
    *out_placement = placement;
    return true;
}

bool backend_3d_scaffold_build_attached_object_sphere(
    const SimRuntime3DDomainDesc *desc,
    const SimRuntimeEmitterPlacement3D *placement,
    const PresetObject *object,
    SimRuntimeEmitterPlacement3D *out_placement) {
    SimRuntimeEmitterPlacement3D sphere = {0};
    if (!desc || !placement || !object || !out_placement) return false;
    sphere = *placement;
    sphere.radius_cells = sim_runtime_3d_footprint_object_sphere_radius_cells(desc, object);
    backend_3d_scaffold_fill_sphere_bounds(desc, &sphere);
    *out_placement = sphere;
    return true;
}

bool backend_3d_scaffold_build_object_box(const SimRuntime3DDomainDesc *desc,
                                          const SimRuntimeEmitterPlacement3D *placement,
                                          const PresetObject *object,
                                          SimRuntimeEmitterOrientedBox3D *out_box) {
    if (!desc || !placement || !object || !out_box) return false;
    return sim_runtime_backend_3d_build_preset_object_oriented_box(desc,
                                                                   object,
                                                                   placement->center_x,
                                                                   placement->center_y,
                                                                   placement->center_z,
                                                                   out_box);
}

bool backend_3d_scaffold_build_import_box(const SimRuntime3DDomainDesc *desc,
                                          const SimRuntimeEmitterPlacement3D *placement,
                                          const ImportedShape *imp,
                                          SimRuntimeEmitterOrientedBox3D *out_box) {
    SimRuntimeEmitterOrientedBox3D box = {0};
    SimRuntime3DFootprintHalfExtents half_extents = {0};
    float radians = 0.0f;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    if (!desc || !placement || !imp || !out_box) return false;
    if (!sim_runtime_3d_footprint_import_box_half_extents_cells(desc, imp, &half_extents)) {
        return false;
    }

    box.center_x = placement->center_x;
    box.center_y = placement->center_y;
    box.center_z = placement->center_z;
    box.half_u_cells = (float)half_extents.half_x_cells;
    box.half_v_cells = (float)half_extents.half_y_cells;
    box.half_w_cells = (float)half_extents.half_z_cells;
    radians = imp->rotation_deg * (float)M_PI / 180.0f;
    box.axis_u_x = cosf(radians);
    box.axis_u_y = sinf(radians);
    box.axis_u_z = 0.0f;
    box.axis_v_x = -sinf(radians);
    box.axis_v_y = cosf(radians);
    box.axis_v_z = 0.0f;
    box.axis_w_x = 0.0f;
    box.axis_w_y = 0.0f;
    box.axis_w_z = 1.0f;
    span_x = fabsf(box.axis_u_x) * box.half_u_cells +
             fabsf(box.axis_v_x) * box.half_v_cells +
             fabsf(box.axis_w_x) * box.half_w_cells;
    span_y = fabsf(box.axis_u_y) * box.half_u_cells +
             fabsf(box.axis_v_y) * box.half_v_cells +
             fabsf(box.axis_w_y) * box.half_w_cells;
    span_z = fabsf(box.axis_u_z) * box.half_u_cells +
             fabsf(box.axis_v_z) * box.half_v_cells +
             fabsf(box.axis_w_z) * box.half_w_cells;
    box.min_x = clamp_int_value((int)floorf((float)box.center_x - span_x - 1.0f), 0, desc->grid_w - 1);
    box.max_x = clamp_int_value((int)ceilf((float)box.center_x + span_x + 1.0f), 0, desc->grid_w - 1);
    box.min_y = clamp_int_value((int)floorf((float)box.center_y - span_y - 1.0f), 0, desc->grid_h - 1);
    box.max_y = clamp_int_value((int)ceilf((float)box.center_y + span_y + 1.0f), 0, desc->grid_h - 1);
    box.min_z = clamp_int_value((int)floorf((float)box.center_z - span_z - 1.0f), 0, desc->grid_d - 1);
    box.max_z = clamp_int_value((int)ceilf((float)box.center_z + span_z + 1.0f), 0, desc->grid_d - 1);
    *out_box = box;
    return true;
}
