#ifndef PHYSICS_SIM_RUNTIME_SCENE_BRIDGE_H
#define PHYSICS_SIM_RUNTIME_SCENE_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "core_scene.h"

typedef struct RuntimeSceneBridgePreflight {
    bool valid_contract;
    char scene_id[128];
    int object_count;
    int material_count;
    int light_count;
    int camera_count;
    char diagnostics[256];
} RuntimeSceneBridgePreflight;

#define PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS 128

typedef struct PhysicsSimRuntimeSceneBounds {
    bool enabled;
    bool clamp_on_edit;
    CoreObjectVec3 min;
    CoreObjectVec3 max;
} PhysicsSimRuntimeSceneBounds;

typedef struct PhysicsSimRuntimeConstructionPlane {
    bool valid;
    CoreObjectPlane axis_plane;
    double offset;
    CoreSceneFrame3 custom_frame;
} PhysicsSimRuntimeConstructionPlane;

typedef struct PhysicsSimRetainedRuntimeScene {
    bool valid_contract;
    char diagnostics[256];
    CoreSceneRootContract root;
    int object_count;
    int retained_object_count;
    bool object_capacity_clamped;
    int invalid_object_count;
    int primitive_object_count;
    int material_count;
    int light_count;
    int camera_count;
    int hierarchy_edge_count;
    bool has_line_drawing_scene3d;
    PhysicsSimRuntimeSceneBounds bounds;
    PhysicsSimRuntimeConstructionPlane construction_plane;
    CoreSceneObjectContract objects[PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS];
} PhysicsSimRetainedRuntimeScene;

typedef struct PhysicsSimRuntimeVisualBootstrap {
    bool valid;
    PhysicsSimRetainedRuntimeScene retained_scene;
    PhysicsSimRuntimeSceneBounds scene_domain;
    bool scene_domain_authored;
} PhysicsSimRuntimeVisualBootstrap;

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight);
bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight);

bool runtime_scene_bridge_apply_json(const char *runtime_scene_json,
                                     AppConfig *in_out_cfg,
                                     FluidScenePreset *in_out_preset,
                                     RuntimeSceneBridgePreflight *out_summary);
bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     AppConfig *in_out_cfg,
                                     FluidScenePreset *in_out_preset,
                                     RuntimeSceneBridgePreflight *out_summary);

void runtime_scene_bridge_get_last_retained_scene(PhysicsSimRetainedRuntimeScene *out_scene);
bool runtime_scene_bridge_load_visual_bootstrap_json(const char *runtime_scene_json,
                                                     PhysicsSimRuntimeVisualBootstrap *out_bootstrap,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size);
bool runtime_scene_bridge_load_visual_bootstrap_file(const char *runtime_scene_path,
                                                     PhysicsSimRuntimeVisualBootstrap *out_bootstrap,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size);

bool runtime_scene_bridge_writeback_physics_overlay_json(const char *runtime_scene_json,
                                                         const char *overlay_json,
                                                         char **out_runtime_scene_json,
                                                         char *out_diagnostics,
                                                         size_t out_diagnostics_size);

#endif
