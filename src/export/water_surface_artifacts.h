#ifndef PHYSICS_SIM_WATER_SURFACE_ARTIFACTS_H
#define PHYSICS_SIM_WATER_SURFACE_ARTIFACTS_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_state.h"
#include "app/water_object_coupling.h"
#include "export/volume_frames_vf3d.h"

typedef struct WaterSurfaceArtifactStatsV1 {
    int grid_w;
    int grid_d;
    size_t sample_count;
    size_t wet_columns;
    size_t dry_columns;
    size_t solid_columns;
    size_t water_cells;
    float surface_min_y;
    float surface_max_y;
    float surface_avg_y;
    float max_slope;
    bool review_ripples_applied;
    float review_ripple_amplitude_m;
    float review_ripple_delta_min_m;
    float review_ripple_delta_max_m;
    WaterObjectCouplingDiagnostics object_coupling;
    bool finite_normals;
} WaterSurfaceArtifactStatsV1;

bool water_surface_artifacts_should_export(const SceneState *scene);
bool water_surface_artifacts_write_frame(const SceneState *scene,
                                         const VolumeFrameHeaderVf3dV1 *volume_header,
                                         const char *run_dir,
                                         WaterSurfaceArtifactStatsV1 *out_stats);

#endif
