#ifndef PHYSICS_SIM_RUNTIME_SCENE_BRIDGE_H
#define PHYSICS_SIM_RUNTIME_SCENE_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_config.h"
#include "app/scene_presets.h"

typedef struct RuntimeSceneBridgePreflight {
    bool valid_contract;
    char scene_id[128];
    int object_count;
    int material_count;
    int light_count;
    int camera_count;
    char diagnostics[256];
} RuntimeSceneBridgePreflight;

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

bool runtime_scene_bridge_writeback_physics_overlay_json(const char *runtime_scene_json,
                                                         const char *overlay_json,
                                                         char **out_runtime_scene_json,
                                                         char *out_diagnostics,
                                                         size_t out_diagnostics_size);

#endif
