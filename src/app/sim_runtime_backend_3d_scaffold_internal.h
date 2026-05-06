#ifndef SIM_RUNTIME_BACKEND_3D_SCAFFOLD_INTERNAL_H
#define SIM_RUNTIME_BACKEND_3D_SCAFFOLD_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_obstacle.h"
#include "app/sim_runtime_3d_brick_store.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/sim_runtime_backend.h"
#include "core_sim.h"

typedef struct SimRuntimeBackend3DScaffold {
    SimRuntime3DVolume volume;
    SimRuntime3DBrickStore brick_store;
    SimRuntime3DVolume solver_volume;
    uint8_t *solver_solid_mask;
    SimRuntime3DVolume export_volume_cache;
    uint8_t *export_solid_mask_cache;
    SimRuntime3DSolverScratch solver_scratch;
    CoreSimLoopState solver_loop;
    SimRuntimeObstacleContract obstacle_contract;
    const struct SceneState *scene_ref;
    int compatibility_slice_z;
    bool fluid_slice_dirty;
    bool obstacle_volume_dirty;
    bool obstacle_slice_dirty;
    bool scene_up_valid;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    PhysicsSimRuntimeSceneUpSource scene_up_source;
    bool debug_volume_stats_dirty;
    bool export_volume_cache_dirty;
    bool runtime_solver_region_guard_triggered;
    bool runtime_solver_cluster_limit_reached;
    size_t runtime_last_active_region_cell_count;
    size_t runtime_last_solver_region_cell_count;
    size_t runtime_last_solver_cluster_count;
    size_t runtime_last_solver_max_cluster_cell_count;
    size_t runtime_last_solver_solved_cluster_count;
    size_t runtime_last_solver_skipped_cluster_count;
    size_t runtime_last_solver_skipped_solver_cell_count;
    size_t runtime_export_cache_materialization_count;
    size_t runtime_solver_region_cell_budget;
    bool runtime_solver_region_cell_budget_overridden;
    float runtime_solver_max_velocity_displacement_cells_limit;
    bool runtime_solver_max_velocity_displacement_cells_limit_overridden;
    size_t runtime_solver_velocity_clamp_cell_count;
    float runtime_solver_max_velocity_magnitude_pre_clamp;
    float runtime_solver_max_velocity_magnitude_post_clamp;
    float runtime_solver_max_velocity_displacement_cells_pre_clamp;
    float runtime_solver_max_velocity_displacement_cells_post_clamp;
    float runtime_solver_max_abs_divergence_after_project;
    size_t debug_volume_active_density_cells;
    size_t debug_volume_solid_cells;
    float debug_volume_max_density;
    float debug_volume_max_velocity_magnitude;
    bool debug_volume_scene_up_velocity_valid;
    float debug_volume_scene_up_velocity_avg;
    float debug_volume_scene_up_velocity_peak;
    float *slice_density;
    float *slice_velocity_x;
    float *slice_velocity_y;
    float *slice_pressure;
    bool obstacle_dense_cache_dirty;
    void **obstacle_bricks;
    uint8_t *obstacle_occupancy;
    uint8_t *obstacle_brick_flags;
    uint8_t *slice_solid_mask;
    float *slice_obstacle_velocity_x;
    float *slice_obstacle_velocity_y;
    float *slice_obstacle_distance;
    size_t emitter_step_emitters_applied;
    size_t emitter_step_free_emitters_applied;
    size_t emitter_step_attached_emitters_applied;
    size_t emitter_step_affected_cells;
    size_t emitter_step_last_footprint_cells;
    float emitter_step_density_delta;
    float emitter_step_velocity_magnitude_delta;
} SimRuntimeBackend3DScaffold;

static inline bool backend_3d_scaffold_dense_mirror_live(
    const SimRuntimeBackend3DScaffold *state) {
    return state &&
           state->volume.density &&
           state->volume.velocity_x &&
           state->volume.velocity_y &&
           state->volume.velocity_z &&
           state->volume.pressure;
}

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
void backend_3d_scaffold_set_obstacle_cell(SimRuntimeBackend3DScaffold *state,
                                           int x,
                                           int y,
                                           int z,
                                           bool solid);
bool backend_3d_scaffold_ensure_obstacle_dense_cache(SimRuntimeBackend3DScaffold *state);
bool backend_3d_scaffold_obstacle_cell_solid(const SimRuntimeBackend3DScaffold *state,
                                             int x,
                                             int y,
                                             int z);
bool backend_3d_scaffold_obstacle_materialize_region(const SimRuntimeBackend3DScaffold *state,
                                                     const SimRuntime3DBrickRegion *region,
                                                     uint8_t *out_solid_mask,
                                                     size_t out_cell_count);
bool backend_3d_scaffold_obstacle_materialize_full(const SimRuntimeBackend3DScaffold *state,
                                                   uint8_t *out_solid_mask,
                                                   size_t out_cell_count);
bool backend_3d_scaffold_obstacle_fill_slice_xy(const SimRuntimeBackend3DScaffold *state,
                                                int z,
                                                uint8_t *out_solid_mask,
                                                size_t out_cell_count);
size_t backend_3d_scaffold_obstacle_solid_cell_count(const SimRuntimeBackend3DScaffold *state);
void backend_3d_scaffold_clear_obstacle_bricks(SimRuntimeBackend3DScaffold *state);
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
