#ifndef SIM_RUNTIME_BACKEND_3D_SCAFFOLD_INTERNAL_H
#define SIM_RUNTIME_BACKEND_3D_SCAFFOLD_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_obstacle.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/sim_runtime_backend.h"

typedef struct SimRuntimeBackend3DScaffold {
    SimRuntime3DVolume volume;
    SimRuntime3DSolverScratch solver_scratch;
    SimRuntimeObstacleContract obstacle_contract;
    const struct SceneState *scene_ref;
    int compatibility_slice_z;
    bool fluid_slice_dirty;
    bool obstacle_volume_dirty;
    bool obstacle_slice_dirty;
    float *slice_density;
    float *slice_velocity_x;
    float *slice_velocity_y;
    float *slice_pressure;
    uint8_t *obstacle_occupancy;
    uint8_t *slice_solid_mask;
    float *slice_obstacle_velocity_x;
    float *slice_obstacle_velocity_y;
    float *slice_obstacle_distance;
} SimRuntimeBackend3DScaffold;

void backend_3d_scaffold_apply_emitters(SimRuntimeBackend *backend,
                                        struct SceneState *scene,
                                        double dt);
void backend_3d_scaffold_rasterize_retained_object_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene);
void backend_3d_scaffold_rasterize_retained_import_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene);
void backend_3d_scaffold_reset_obstacles(SimRuntimeBackend3DScaffold *state);
void backend_3d_scaffold_build_static_obstacles(SimRuntimeBackend *backend,
                                                struct SceneState *scene);
void backend_3d_scaffold_build_obstacles(SimRuntimeBackend *backend,
                                         struct SceneState *scene);
void backend_3d_scaffold_mark_obstacles_dirty(SimRuntimeBackend *backend);
void backend_3d_scaffold_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                     struct SceneState *scene);
void backend_3d_scaffold_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene);
void backend_3d_scaffold_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene);
bool backend_3d_scaffold_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                              SceneObstacleFieldView2D *out_view);

#endif
