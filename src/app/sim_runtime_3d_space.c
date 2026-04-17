#include "app/sim_runtime_3d_space.h"

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double clamp_double_value(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

double sim_runtime_3d_space_resolve_world_axis(double position,
                                               double world_min,
                                               double world_max) {
    double span = world_max - world_min;
    if (!(span > 0.0)) return world_min;
    if (position >= 0.0 && position <= 1.0) {
        return world_min + position * span;
    }
    if (position < world_min) return world_min;
    if (position > world_max) return world_max;
    return position;
}

double sim_runtime_3d_space_normalize_world_axis(double world_position,
                                                 double world_min,
                                                 double world_max) {
    double span = world_max - world_min;
    if (!(span > 0.0)) return 0.5;
    return clamp_double_value((world_position - world_min) / span, 0.0, 1.0);
}

double sim_runtime_3d_space_normalize_half_extent(double half_extent,
                                                  double world_span,
                                                  double fallback) {
    if (!(world_span > 0.0)) return fallback;
    if (!(half_extent > 0.0)) return fallback;
    return clamp_double_value(half_extent / world_span, 0.0, 1.0);
}

int sim_runtime_3d_space_world_to_grid_axis(double world_position,
                                            double world_min,
                                            double voxel_size,
                                            int grid_extent) {
    int cell = 0;
    if (grid_extent <= 1 || !(voxel_size > 0.0)) return 0;
    cell = (int)((world_position - world_min) / voxel_size);
    return clamp_int_value(cell, 0, grid_extent - 1);
}

double sim_runtime_3d_space_slice_world_z(double world_min_z,
                                          double voxel_size,
                                          int grid_d,
                                          int slice_z) {
    if (grid_d <= 0 || !(voxel_size > 0.0)) return world_min_z;
    slice_z = clamp_int_value(slice_z, 0, grid_d - 1);
    return world_min_z + ((double)slice_z + 0.5) * voxel_size;
}

double sim_runtime_3d_space_slice_world_z_for_desc(const SimRuntime3DDomainDesc *desc,
                                                   int slice_z) {
    if (!desc) return 0.0;
    return sim_runtime_3d_space_slice_world_z(desc->world_min_z,
                                              desc->voxel_size,
                                              desc->grid_d,
                                              slice_z);
}
