#ifndef SCENE_RUNTIME_LAUNCH_PROJECTION_H
#define SCENE_RUNTIME_LAUNCH_PROJECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_config.h"
#include "app/scene_controller.h"
#include "app/scene_presets.h"

bool scene_runtime_launch_apply_retained_projection(const SceneRuntimeLaunch *runtime_launch,
                                                    AppConfig *in_out_cfg,
                                                    FluidScenePreset *in_out_preset,
                                                    char *out_diagnostics,
                                                    size_t out_diagnostics_size);

#endif // SCENE_RUNTIME_LAUNCH_PROJECTION_H
