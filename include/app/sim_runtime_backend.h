#ifndef SIM_RUNTIME_BACKEND_H
#define SIM_RUNTIME_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/app_config.h"
#include "app/sim_mode.h"
#include "import/runtime_scene_bridge.h"
#include "input/stroke_buffer.h"

struct SceneState;

typedef enum SimRuntimeBackendKind {
    SIM_RUNTIME_BACKEND_KIND_NONE = 0,
    SIM_RUNTIME_BACKEND_KIND_FLUID_2D = 1,
    SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD = 2
} SimRuntimeBackendKind;

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
    bool debug_volume_view_3d_available;
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
    void (*apply_emitters)(SimRuntimeBackend *backend, struct SceneState *scene, double dt);
    void (*apply_boundary_flows)(SimRuntimeBackend *backend, struct SceneState *scene, double dt);
    void (*enforce_boundary_flows)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*enforce_obstacles)(SimRuntimeBackend *backend, struct SceneState *scene);
    void (*step)(SimRuntimeBackend *backend,
                 struct SceneState *scene,
                 const AppConfig *cfg,
                 double dt);
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
                                        double dt);
void sim_runtime_backend_apply_boundary_flows(SimRuntimeBackend *backend,
                                              struct SceneState *scene,
                                              double dt);
void sim_runtime_backend_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene);
void sim_runtime_backend_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene);
void sim_runtime_backend_step(SimRuntimeBackend *backend,
                              struct SceneState *scene,
                              const AppConfig *cfg,
                              double dt);
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
bool sim_runtime_backend_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                          int slice_z,
                                                          bool *out_has_fluid,
                                                          bool *out_has_obstacles);
bool sim_runtime_backend_step_compatibility_slice(SimRuntimeBackend *backend, int delta_z);

#endif // SIM_RUNTIME_BACKEND_H
