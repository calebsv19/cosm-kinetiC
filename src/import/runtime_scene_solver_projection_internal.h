#ifndef PHYSICS_SIM_RUNTIME_SCENE_SOLVER_PROJECTION_INTERNAL_H
#define PHYSICS_SIM_RUNTIME_SCENE_SOLVER_PROJECTION_INTERNAL_H

#include <stdbool.h>

#include "import/runtime_scene_solver_projection.h"

typedef struct SolverProjectionPhysicsOverlay {
    bool found;
    bool has_motion_mode;
    bool is_static;
    bool has_initial_velocity;
    double initial_velocity_x;
    double initial_velocity_y;
    double initial_velocity_z;
    bool has_emitter;
    FluidEmitterType emitter_type;
    double emitter_radius;
    double emitter_strength;
    bool has_emitter_direction;
    double emitter_dir_x;
    double emitter_dir_y;
    double emitter_dir_z;
} SolverProjectionPhysicsOverlay;

typedef struct SolverProjectionSceneDomain {
    bool found;
    bool active;
    double min_x;
    double min_y;
    double min_z;
    double max_x;
    double max_y;
    double max_z;
} SolverProjectionSceneDomain;

typedef struct SolverProjectionXYDomainMapping {
    bool valid;
    double min_x;
    double min_y;
    double max_x;
    double max_y;
    double span_x;
    double span_y;
} SolverProjectionXYDomainMapping;

float runtime_scene_solver_projection_clampf_dim(float v, float min_v, float max_v);
float runtime_scene_solver_projection_domain_dimension(double extent, double world_scale, float fallback);
float runtime_scene_solver_projection_scaled_size(double dimension, double world_scale, float fallback);
float runtime_scene_solver_projection_scaled_position(double coord, double world_scale);
float runtime_scene_solver_projection_normalize_velocity(double value, double span);
bool runtime_scene_solver_projection_parse_vec3(json_object *root,
                                                const char *key,
                                                double *out_x,
                                                double *out_y,
                                                double *out_z);
bool runtime_scene_solver_projection_parse_bool(json_object *root,
                                                const char *key,
                                                bool *out_value);

void runtime_scene_solver_projection_apply_space_mode(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                      AppConfig *in_out_cfg,
                                                      FluidScenePreset *in_out_preset);
bool runtime_scene_solver_projection_overlay_scene_domain(json_object *runtime_root,
                                                          SolverProjectionSceneDomain *out_domain);
void runtime_scene_solver_projection_resolve_xy_domain_mapping(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    SolverProjectionXYDomainMapping *out_mapping);
bool runtime_scene_solver_projection_overlay_for_object(json_object *runtime_root,
                                                        const char *object_id,
                                                        SolverProjectionPhysicsOverlay *out_overlay);
void runtime_scene_solver_projection_apply_scene_domain(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    FluidScenePreset *in_out_preset);
void runtime_scene_solver_projection_apply_objects(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                   json_object *runtime_root,
                                                   FluidScenePreset *in_out_preset,
                                                   RuntimeSceneBridgePreflight *out_summary);
void runtime_scene_solver_projection_apply_runtime_root_objects(json_object *runtime_root,
                                                                double world_scale,
                                                                FluidScenePreset *in_out_preset,
                                                                RuntimeSceneBridgePreflight *out_summary);
void runtime_scene_solver_projection_apply_emitters_from_lights(json_object *runtime_root,
                                                                double world_scale,
                                                                FluidSceneDimensionMode dimension_mode,
                                                                FluidScenePreset *in_out_preset);
int runtime_scene_solver_projection_apply_emitters_from_retained_objects(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    double world_scale,
    FluidScenePreset *in_out_preset);
int runtime_scene_solver_projection_apply_emitters_from_runtime_root_objects(
    json_object *runtime_root,
    double world_scale,
    FluidSceneDimensionMode dimension_mode,
    FluidScenePreset *in_out_preset);

#endif
