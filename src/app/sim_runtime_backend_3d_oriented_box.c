#include "app/sim_runtime_backend_3d_oriented_box.h"

#include "app/sim_runtime_3d_footprint.h"

#include <math.h>

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool normalize_vec3(float *x, float *y, float *z) {
    float length = 0.0f;
    if (!x || !y || !z) return false;
    length = sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    if (!(length > 1e-6f)) return false;
    *x /= length;
    *y /= length;
    *z /= length;
    return true;
}

static void fallback_axes_from_angle(float angle,
                                     float *ux,
                                     float *uy,
                                     float *uz,
                                     float *vx,
                                     float *vy,
                                     float *vz,
                                     float *wx,
                                     float *wy,
                                     float *wz) {
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    if (!ux || !uy || !uz || !vx || !vy || !vz || !wx || !wy || !wz) return;
    *ux = cos_a;
    *uy = sin_a;
    *uz = 0.0f;
    *vx = -sin_a;
    *vy = cos_a;
    *vz = 0.0f;
    *wx = 0.0f;
    *wy = 0.0f;
    *wz = 1.0f;
}

static void resolve_axes(const PresetObject *object, SimRuntimeOrientedBox3DCells *box) {
    if (!object || !box) return;
    if (object->orientation_basis_valid) {
        box->axis_u_x = object->orientation_u_x;
        box->axis_u_y = object->orientation_u_y;
        box->axis_u_z = object->orientation_u_z;
        box->axis_v_x = object->orientation_v_x;
        box->axis_v_y = object->orientation_v_y;
        box->axis_v_z = object->orientation_v_z;
        box->axis_w_x = object->orientation_w_x;
        box->axis_w_y = object->orientation_w_y;
        box->axis_w_z = object->orientation_w_z;
        if (normalize_vec3(&box->axis_u_x, &box->axis_u_y, &box->axis_u_z) &&
            normalize_vec3(&box->axis_v_x, &box->axis_v_y, &box->axis_v_z) &&
            normalize_vec3(&box->axis_w_x, &box->axis_w_y, &box->axis_w_z)) {
            return;
        }
    }
    fallback_axes_from_angle(object->angle,
                             &box->axis_u_x,
                             &box->axis_u_y,
                             &box->axis_u_z,
                             &box->axis_v_x,
                             &box->axis_v_y,
                             &box->axis_v_z,
                             &box->axis_w_x,
                             &box->axis_w_y,
                             &box->axis_w_z);
}

bool sim_runtime_backend_3d_build_preset_object_oriented_box(
    const SimRuntime3DDomainDesc *desc,
    const PresetObject *object,
    int center_x,
    int center_y,
    int center_z,
    SimRuntimeOrientedBox3DCells *out_box) {
    SimRuntimeOrientedBox3DCells box = {0};
    SimRuntime3DFootprintHalfExtents half_extents = {0};
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    if (!desc || !object || !out_box) return false;
    if (!sim_runtime_3d_footprint_object_box_half_extents_cells(desc, object, &half_extents)) {
        return false;
    }

    box.center_x = center_x;
    box.center_y = center_y;
    box.center_z = center_z;
    box.half_u_cells = (float)half_extents.half_x_cells;
    box.half_v_cells = (float)half_extents.half_y_cells;
    box.half_w_cells = (float)half_extents.half_z_cells;
    resolve_axes(object, &box);

    span_x = fabsf(box.axis_u_x) * box.half_u_cells +
             fabsf(box.axis_v_x) * box.half_v_cells +
             fabsf(box.axis_w_x) * box.half_w_cells;
    span_y = fabsf(box.axis_u_y) * box.half_u_cells +
             fabsf(box.axis_v_y) * box.half_v_cells +
             fabsf(box.axis_w_y) * box.half_w_cells;
    span_z = fabsf(box.axis_u_z) * box.half_u_cells +
             fabsf(box.axis_v_z) * box.half_v_cells +
             fabsf(box.axis_w_z) * box.half_w_cells;
    box.min_x = clamp_int_value((int)floorf((float)center_x - span_x - 1.0f), 0, desc->grid_w - 1);
    box.max_x = clamp_int_value((int)ceilf((float)center_x + span_x + 1.0f), 0, desc->grid_w - 1);
    box.min_y = clamp_int_value((int)floorf((float)center_y - span_y - 1.0f), 0, desc->grid_h - 1);
    box.max_y = clamp_int_value((int)ceilf((float)center_y + span_y + 1.0f), 0, desc->grid_h - 1);
    box.min_z = clamp_int_value((int)floorf((float)center_z - span_z - 1.0f), 0, desc->grid_d - 1);
    box.max_z = clamp_int_value((int)ceilf((float)center_z + span_z + 1.0f), 0, desc->grid_d - 1);
    *out_box = box;
    return true;
}

bool sim_runtime_backend_3d_cell_in_oriented_box(const SimRuntimeOrientedBox3DCells *box,
                                                 int x,
                                                 int y,
                                                 int z) {
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float local_u = 0.0f;
    float local_v = 0.0f;
    float local_w = 0.0f;
    if (!box) return false;
    dx = (float)(x - box->center_x);
    dy = (float)(y - box->center_y);
    dz = (float)(z - box->center_z);
    local_u = dx * box->axis_u_x + dy * box->axis_u_y + dz * box->axis_u_z;
    local_v = dx * box->axis_v_x + dy * box->axis_v_y + dz * box->axis_v_z;
    local_w = dx * box->axis_w_x + dy * box->axis_w_y + dz * box->axis_w_z;
    return fabsf(local_u) <= box->half_u_cells &&
           fabsf(local_v) <= box->half_v_cells &&
           fabsf(local_w) <= box->half_w_cells;
}
