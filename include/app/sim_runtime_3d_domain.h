#ifndef SIM_RUNTIME_3D_DOMAIN_H
#define SIM_RUNTIME_3D_DOMAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_config.h"
#include "app/scene_presets.h"

typedef struct PhysicsSimRuntimeVisualBootstrap PhysicsSimRuntimeVisualBootstrap;

typedef enum SimRuntime3DDepthPolicy {
    SIM_RUNTIME_3D_DEPTH_POLICY_NONE = 0,
    SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS,
    SIM_RUNTIME_3D_DEPTH_POLICY_SCENE_DOMAIN_BOUNDS,
    SIM_RUNTIME_3D_DEPTH_POLICY_RETAINED_SCENE_BOUNDS,
    SIM_RUNTIME_3D_DEPTH_POLICY_LEGACY_MIN_XY
} SimRuntime3DDepthPolicy;

typedef struct SimRuntime3DDomainDesc {
    int requested_major_axis_cells;
    int applied_major_axis_cells;
    int requested_depth_cells;
    int applied_depth_cells;
    int grid_w;
    int grid_h;
    int grid_d;
    size_t slice_cell_count;
    size_t cell_count;
    SimRuntime3DDepthPolicy depth_policy;
    float world_min_x;
    float world_min_y;
    float world_min_z;
    float world_max_x;
    float world_max_y;
    float world_max_z;
    float voxel_size;
} SimRuntime3DDomainDesc;

typedef struct SimRuntime3DVolume {
    SimRuntime3DDomainDesc desc;
    float *density;
    float *velocity_x;
    float *velocity_y;
    float *velocity_z;
    float *pressure;
} SimRuntime3DVolume;

int sim_runtime_3d_requested_major_axis_cells_for_config(const AppConfig *cfg);
int sim_runtime_3d_major_axis_cells_for_config(const AppConfig *cfg);
int sim_runtime_3d_applied_major_axis_cells_for_requested(int requested_major_axis_cells);
int sim_runtime_3d_requested_depth_cells_for_config(const AppConfig *cfg);
int sim_runtime_3d_applied_depth_cells_for_requested(int requested_depth_cells);
const char *sim_runtime_3d_depth_policy_label(SimRuntime3DDepthPolicy policy);
bool sim_runtime_3d_domain_desc_resolve(const AppConfig *cfg,
                                        const FluidScenePreset *preset,
                                        const PhysicsSimRuntimeVisualBootstrap *runtime_visual,
                                        SimRuntime3DDomainDesc *out_desc);
bool sim_runtime_3d_domain_desc_from_legacy(const AppConfig *cfg,
                                            const FluidScenePreset *preset,
                                            SimRuntime3DDomainDesc *out_desc);
size_t sim_runtime_3d_domain_estimated_resident_bytes(const SimRuntime3DDomainDesc *desc);
size_t sim_runtime_3d_domain_resident_bytes_budget(void);

bool sim_runtime_3d_volume_init(SimRuntime3DVolume *volume,
                                const SimRuntime3DDomainDesc *desc);
void sim_runtime_3d_volume_destroy(SimRuntime3DVolume *volume);
void sim_runtime_3d_volume_clear(SimRuntime3DVolume *volume);
size_t sim_runtime_3d_volume_index(const SimRuntime3DDomainDesc *desc,
                                   int x,
                                   int y,
                                   int z);

#endif // SIM_RUNTIME_3D_DOMAIN_H
