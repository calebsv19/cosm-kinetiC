#ifndef SIM_RUNTIME_BACKEND_H
#define SIM_RUNTIME_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/app_config.h"
#include "app/sim_mode.h"
#include "app/sim_runtime_3d_domain.h"
#include "import/runtime_scene_bridge.h"
#include "input/stroke_buffer.h"

struct SceneState;

typedef enum SimRuntimeBackendKind {
    SIM_RUNTIME_BACKEND_KIND_NONE = 0,
    SIM_RUNTIME_BACKEND_KIND_FLUID_2D = 1,
    SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD = 2
} SimRuntimeBackendKind;

typedef enum SimRuntimeInitialStateSource {
    SIM_RUNTIME_INITIAL_STATE_SOURCE_BLANK = 0,
    SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE = 1,
    SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_OPTIONAL_LAYER = 2,
    SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_WARM_START = 3
} SimRuntimeInitialStateSource;

typedef struct SceneFluidFieldView2D {
    int width;
    int height;
    size_t cell_count;
    const float *density;
    const float *velocity_x;
    const float *velocity_y;
    const float *pressure;
} SceneFluidFieldView2D;

typedef struct SceneObstacleFieldView2D {
    int width;
    int height;
    size_t cell_count;
    const uint8_t *solid_mask;
    const float *velocity_x;
    const float *velocity_y;
    const float *distance;
} SceneObstacleFieldView2D;

typedef struct SceneDebugVolumeView3D {
    int width;
    int height;
    int depth;
    size_t cell_count;
    float world_min_x;
    float world_min_y;
    float world_min_z;
    float world_max_x;
    float world_max_y;
    float world_max_z;
    float voxel_size;
    const float *density;
    const uint8_t *solid_mask;
} SceneDebugVolumeView3D;

// Dedicated authoritative 3D export carrier.
// This is intentionally separate from SceneDebugVolumeView3D so export code can
// consume backend-owned XYZ field truth without depending on debug/readout
// surfaces or the derived compatibility slice.
typedef struct SceneFluidVolumeExportView3D {
    int width;
    int height;
    int depth;
    size_t cell_count;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    bool scene_up_valid;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    const float *density;
    const float *velocity_x;
    const float *velocity_y;
    const float *velocity_z;
    const float *pressure;
    const uint8_t *solid_mask;
} SceneFluidVolumeExportView3D;

typedef struct SimRuntimeBackendReport {
    SimRuntimeBackendKind kind;
    int requested_major_axis_cells;
    int applied_major_axis_cells;
    int requested_depth_cells;
    int applied_depth_cells;
    SimRuntime3DDepthPolicy depth_policy;
    int requested_solver_iterations;
    int applied_solver_iterations;
    int domain_w;
    int domain_h;
    int domain_d;
    size_t cell_count;
    bool volumetric_emitters_free_live;
    bool volumetric_emitters_attached_live;
    bool volumetric_obstacles_live;
    bool full_3d_solver_live;
    bool world_bounds_valid;
    float world_min_x;
    float world_min_y;
    float world_min_z;
    float world_max_x;
    float world_max_y;
    float world_max_z;
    float voxel_size;
    bool scene_up_valid;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    PhysicsSimRuntimeSceneUpSource scene_up_source;
    bool compatibility_view_2d_available;
    bool compatibility_view_2d_derived;
    int compatibility_slice_z;
    bool secondary_debug_slice_stack_live;
    int secondary_debug_slice_stack_radius;
    size_t runtime_allocated_brick_count;
    size_t runtime_active_brick_count;
    size_t runtime_active_region_cell_count;
    size_t runtime_solver_region_cell_count;
    size_t runtime_solver_cluster_count;
    size_t runtime_solver_max_cluster_cell_count;
    size_t runtime_solver_solved_cluster_count;
    size_t runtime_solver_skipped_cluster_count;
    size_t runtime_solver_skipped_solver_cell_count;
    size_t runtime_export_cache_materialization_count;
    size_t runtime_solver_region_cell_budget;
    bool runtime_solver_region_cell_budget_overridden;
    float runtime_solver_max_velocity_displacement_cells_limit;
    bool runtime_solver_max_velocity_displacement_cells_limit_overridden;
    size_t runtime_solver_velocity_clamp_cell_count;
    bool runtime_dense_mirror_live;
    bool runtime_solver_region_guard_triggered;
    bool runtime_solver_cluster_limit_reached;
    float runtime_solver_max_velocity_magnitude_pre_clamp;
    float runtime_solver_max_velocity_magnitude_post_clamp;
    float runtime_solver_max_velocity_displacement_cells_pre_clamp;
    float runtime_solver_max_velocity_displacement_cells_post_clamp;
    float runtime_solver_max_abs_divergence_after_project;
    bool debug_volume_view_3d_available;
    SimRuntimeInitialStateSource initial_state_source;
    bool atmospheric_seeded;
    uint32_t atmospheric_seed;
    size_t atmospheric_seeded_cell_count;
    float atmospheric_seed_max_density;
    float atmospheric_seed_max_velocity_magnitude;
    bool atmospheric_warm_start_loaded;
    int atmospheric_warm_start_source_kind;
    int atmospheric_warm_start_w;
    int atmospheric_warm_start_h;
    int atmospheric_warm_start_d;
    size_t atmospheric_warm_start_cell_count;
    size_t atmospheric_warm_start_active_density_cells;
    size_t atmospheric_warm_start_solid_cells;
    float atmospheric_warm_start_max_density;
    float atmospheric_warm_start_max_velocity_magnitude;
    size_t debug_volume_active_density_cells;
    size_t debug_volume_solid_cells;
    float debug_volume_max_density;
    float debug_volume_max_velocity_magnitude;
    bool debug_volume_scene_up_velocity_valid;
    float debug_volume_scene_up_velocity_avg;
    float debug_volume_scene_up_velocity_peak;
    size_t emitter_step_emitters_applied;
    size_t emitter_step_free_emitters_applied;
    size_t emitter_step_attached_emitters_applied;
    size_t emitter_step_affected_cells;
    size_t emitter_step_last_footprint_cells;
    float emitter_step_density_delta;
    float emitter_step_velocity_magnitude_delta;
    bool wind_analysis_available;
    size_t wind_analysis_sampled_cells;
    float wind_analysis_inlet_pressure_avg;
    float wind_analysis_outlet_pressure_avg;
    float wind_analysis_pressure_delta;
    float wind_analysis_inlet_throughput;
    float wind_analysis_outlet_throughput;
    float wind_analysis_throughput_delta;
    float wind_analysis_drag_pressure_proxy;
    bool wind_analysis_object_drag_available;
    size_t wind_analysis_object_solid_cells;
    float wind_analysis_object_projected_area;
    float wind_analysis_object_upstream_pressure_avg;
    float wind_analysis_object_downstream_pressure_avg;
    float wind_analysis_object_pressure_delta;
    float wind_analysis_object_drag_pressure_proxy;
    float wind_analysis_vorticity_avg;
    float wind_analysis_vorticity_max;
} SimRuntimeBackendReport;

typedef struct SimRuntimeBackend SimRuntimeBackend;

typedef struct SimRuntimeBackendOps {
    void (*destroy)(SimRuntimeBackend *backend);
    bool (*valid)(const SimRuntimeBackend *backend);
    void (*clear)(SimRuntimeBackend *backend);
    bool (*apply_brush_sample)(SimRuntimeBackend *backend,
                               const AppConfig *cfg,
                               const StrokeSample *sample);
    void (*build_static_obstacles)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*build_emitter_masks)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*mark_emitters_dirty)(SimRuntimeBackend *backend);
    void (*build_obstacles)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*mark_obstacles_dirty)(SimRuntimeBackend *backend);
    void (*rasterize_dynamic_obstacles)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*apply_emitters)(SimRuntimeBackend *backend,
                           struct SceneState *scene,
                           double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
    void (*apply_boundary_flows)(SimRuntimeBackend *backend,
                                 struct SceneState *scene,
                                 double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
    void (*enforce_boundary_flows)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*enforce_obstacles)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*step)(SimRuntimeBackend *backend,
                 struct SceneState *scene,
                 const AppConfig *cfg,
                 double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
    void (*inject_object_motion)(SimRuntimeBackend *backend, const struct SceneState *scene);
    void (*reset_transient_state)(SimRuntimeBackend *backend);
    void (*seed_uniform_velocity_2d)(SimRuntimeBackend *backend, float velocity_x, float velocity_y);
    bool (*export_snapshot)(const SimRuntimeBackend *backend, double time, const char *path);
    bool (*get_fluid_view_2d)(const SimRuntimeBackend *backend, SceneFluidFieldView2D *out_view);
    bool (*get_obstacle_view_2d)(const SimRuntimeBackend *backend, SceneObstacleFieldView2D *out_view);
    bool (*get_debug_volume_view_3d)(const SimRuntimeBackend *backend,
                                     SceneDebugVolumeView3D *out_view);
    bool (*get_volume_export_view_3d)(const SimRuntimeBackend *backend,
                                      SceneFluidVolumeExportView3D *out_view);
    bool (*get_report)(const SimRuntimeBackend *backend, SimRuntimeBackendReport *out_report);
    bool (*get_compatibility_slice_activity)(const SimRuntimeBackend *backend,
                                             int slice_z,
                                             bool *out_has_fluid,
                                             bool *out_has_obstacles);
    bool (*step_compatibility_slice)(SimRuntimeBackend *backend, int delta_z);
    bool (*get_domain_desc_3d)(const SimRuntimeBackend *backend,
                               SimRuntime3DDomainDesc *out_desc);
    bool (*debug_zero_dense_mirror_3d)(SimRuntimeBackend *backend);
    bool (*debug_zero_obstacle_dense_cache_3d)(SimRuntimeBackend *backend);
    bool (*debug_write_volume_cell_3d)(SimRuntimeBackend *backend,
                                       int x,
                                       int y,
                                       int z,
                                       float density,
                                       float velocity_x,
                                       float velocity_y,
                                       float velocity_z,
                                       float pressure,
                                       uint8_t solid);
    bool (*debug_reset_volume_truth_3d)(SimRuntimeBackend *backend);
    bool (*debug_note_atmospheric_warm_start_3d)(SimRuntimeBackend *backend,
                                                 int source_kind,
                                                 int width,
                                                 int height,
                                                 int depth,
                                                 size_t cell_count,
                                                 size_t active_density_cells,
                                                 size_t solid_cells,
                                                 float max_density,
                                                 float max_velocity_magnitude);
} SimRuntimeBackendOps;

struct SimRuntimeBackend {
    SimRuntimeBackendKind kind;
    void *impl;
    const SimRuntimeBackendOps *ops;
};

SimRuntimeBackend *sim_runtime_backend_create(const AppConfig *cfg,
                                              const FluidScenePreset *preset,
                                              const SimModeRoute *mode_route,
                                              const PhysicsSimRuntimeVisualBootstrap *runtime_visual);
void sim_runtime_backend_destroy(SimRuntimeBackend *backend);

SimRuntimeBackendKind sim_runtime_backend_kind(const SimRuntimeBackend *backend);
bool sim_runtime_backend_valid(const SimRuntimeBackend *backend);

void sim_runtime_backend_clear(SimRuntimeBackend *backend);
bool sim_runtime_backend_apply_brush_sample(SimRuntimeBackend *backend,
                                            const AppConfig *cfg,
                                            const StrokeSample *sample);

void sim_runtime_backend_build_static_obstacles(SimRuntimeBackend *backend,
                                                struct SceneState *scene);
void sim_runtime_backend_build_emitter_masks(SimRuntimeBackend *backend,
                                             struct SceneState *scene);
void sim_runtime_backend_mark_emitters_dirty(SimRuntimeBackend *backend);
void sim_runtime_backend_build_obstacles(SimRuntimeBackend *backend,
                                         struct SceneState *scene);
void sim_runtime_backend_mark_obstacles_dirty(SimRuntimeBackend *backend);
void sim_runtime_backend_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                     struct SceneState *scene);

void sim_runtime_backend_apply_emitters(SimRuntimeBackend *backend,
                                        struct SceneState *scene,
                                        double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
void sim_runtime_backend_apply_boundary_flows(SimRuntimeBackend *backend,
                                              struct SceneState *scene,
                                              double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
void sim_runtime_backend_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene);
void sim_runtime_backend_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene);
void sim_runtime_backend_step(SimRuntimeBackend *backend,
                              struct SceneState *scene,
                              const AppConfig *cfg,
                              double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);
void sim_runtime_backend_inject_object_motion(SimRuntimeBackend *backend,
                                              const struct SceneState *scene);
void sim_runtime_backend_reset_transient_state(SimRuntimeBackend *backend);

void sim_runtime_backend_seed_uniform_velocity_2d(SimRuntimeBackend *backend,
                                                  float velocity_x,
                                                  float velocity_y);

bool sim_runtime_backend_export_snapshot(const SimRuntimeBackend *backend,
                                         double time,
                                         const char *path);

bool sim_runtime_backend_get_fluid_view_2d(const SimRuntimeBackend *backend,
                                           SceneFluidFieldView2D *out_view);
bool sim_runtime_backend_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                              SceneObstacleFieldView2D *out_view);
bool sim_runtime_backend_get_debug_volume_view_3d(const SimRuntimeBackend *backend,
                                                  SceneDebugVolumeView3D *out_view);
bool sim_runtime_backend_get_volume_export_view_3d(const SimRuntimeBackend *backend,
                                                   SceneFluidVolumeExportView3D *out_view);
bool sim_runtime_backend_get_report(const SimRuntimeBackend *backend,
                                    SimRuntimeBackendReport *out_report);
const char *sim_runtime_initial_state_source_label(SimRuntimeInitialStateSource source);
bool sim_runtime_backend_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                          int slice_z,
                                                          bool *out_has_fluid,
                                                          bool *out_has_obstacles);
bool sim_runtime_backend_step_compatibility_slice(SimRuntimeBackend *backend, int delta_z);
bool sim_runtime_backend_get_domain_desc_3d(const SimRuntimeBackend *backend,
                                            SimRuntime3DDomainDesc *out_desc);
bool sim_runtime_backend_debug_zero_dense_mirror_3d(SimRuntimeBackend *backend);
bool sim_runtime_backend_debug_zero_obstacle_dense_cache_3d(SimRuntimeBackend *backend);
bool sim_runtime_backend_debug_write_volume_cell_3d(SimRuntimeBackend *backend,
                                                    int x,
                                                    int y,
                                                    int z,
                                                    float density,
                                                    float velocity_x,
                                                    float velocity_y,
                                                    float velocity_z,
                                                    float pressure,
                                                    uint8_t solid);
bool sim_runtime_backend_debug_reset_volume_truth_3d(SimRuntimeBackend *backend);
bool sim_runtime_backend_debug_note_atmospheric_warm_start_3d(
    SimRuntimeBackend *backend,
    int source_kind,
    int width,
    int height,
    int depth,
    size_t cell_count,
    size_t active_density_cells,
    size_t solid_cells,
    float max_density,
    float max_velocity_magnitude);

#endif // SIM_RUNTIME_BACKEND_H
