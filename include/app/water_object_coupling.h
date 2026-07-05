#ifndef WATER_OBJECT_COUPLING_H
#define WATER_OBJECT_COUPLING_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_state.h"
#include "export/volume_frames_vf3d.h"

typedef struct WaterObjectCouplingGridBounds {
    bool active;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
} WaterObjectCouplingGridBounds;

typedef struct WaterObjectCouplingDiagnostics {
    bool enabled;
    bool fixture_active;
    bool displacement_applied;
    size_t object_solid_cells;
    size_t object_footprint_columns;
    size_t object_wet_overlap_cells;
    float displaced_volume_m3;
    float displacement_delta_min_m;
    float displacement_delta_max_m;
    float displacement_delta_sum_m;
    float displacement_delta_abs_sum_m;
    float displacement_delta_rms_m;
    size_t displacement_sample_count;
    size_t displacement_capped_sample_count;
    float displacement_weight_sum;
    float displacement_weight_max;
    float object_zone_height_min_y;
    float object_zone_height_max_y;
    float object_zone_height_avg_y;
    float object_zone_height_stddev_m;
    float object_zone_max_slope;
    int affected_min_x;
    int affected_max_x;
    int affected_min_z;
    int affected_max_z;
} WaterObjectCouplingDiagnostics;

bool water_object_coupling_enabled(const AppConfig *cfg);
bool water_object_coupling_grid_bounds(const AppConfig *cfg,
                                       const SceneFluidVolumeExportView3D *volume,
                                       WaterObjectCouplingGridBounds *out_bounds);
size_t water_object_coupling_apply_fixture(SceneState *scene);
bool water_object_coupling_cell_in_fixture(const WaterObjectCouplingGridBounds *bounds,
                                           int x,
                                           int y,
                                           int z);
float water_object_coupling_surface_delta(const SceneFluidVolumeExportView3D *volume,
                                          const VolumeFrameHeaderVf3dV1 *volume_header,
                                          const WaterObjectCouplingGridBounds *bounds,
                                          int x,
                                          int z,
                                          float displaced_volume_m3,
                                          float *out_extent_weight,
                                          bool *out_was_capped);
void water_object_coupling_accumulate_diagnostics(const AppConfig *cfg,
                                                  const SceneFluidVolumeExportView3D *volume,
                                                  const VolumeFrameHeaderVf3dV1 *volume_header,
                                                  WaterObjectCouplingDiagnostics *out_diag);

#endif
