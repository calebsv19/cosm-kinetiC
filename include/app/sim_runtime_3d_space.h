#ifndef SIM_RUNTIME_3D_SPACE_H
#define SIM_RUNTIME_3D_SPACE_H

#include "app/sim_runtime_3d_domain.h"

double sim_runtime_3d_space_resolve_world_axis(double position,
                                               double world_min,
                                               double world_max);
double sim_runtime_3d_space_normalize_world_axis(double world_position,
                                                 double world_min,
                                                 double world_max);
double sim_runtime_3d_space_normalize_half_extent(double half_extent,
                                                  double world_span,
                                                  double fallback);
int sim_runtime_3d_space_world_to_grid_axis(double world_position,
                                            double world_min,
                                            double voxel_size,
                                            int grid_extent);
double sim_runtime_3d_space_slice_world_z(double world_min_z,
                                          double voxel_size,
                                          int grid_d,
                                          int slice_z);
double sim_runtime_3d_space_slice_world_z_for_desc(const SimRuntime3DDomainDesc *desc,
                                                   int slice_z);

#endif // SIM_RUNTIME_3D_SPACE_H
