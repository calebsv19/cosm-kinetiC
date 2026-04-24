#include "app/scene_runtime_launch_projection.h"

#include "import/runtime_scene_bridge.h"

#include <stdio.h>

bool scene_runtime_launch_apply_retained_projection(const SceneRuntimeLaunch *runtime_launch,
                                                    AppConfig *in_out_cfg,
                                                    FluidScenePreset *in_out_preset,
                                                    char *out_diagnostics,
                                                    size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};

    if (out_diagnostics && out_diagnostics_size > 0) {
        out_diagnostics[0] = '\0';
    }
    if (!runtime_launch || !runtime_launch->has_retained_scene) return true;
    if (!runtime_launch->retained_runtime_scene_path[0]) return true;
    if (!in_out_cfg || !in_out_preset) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "missing cfg/preset for retained scene projection");
        }
        return false;
    }

    if (!runtime_scene_bridge_apply_file(runtime_launch->retained_runtime_scene_path,
                                         in_out_cfg,
                                         in_out_preset,
                                         &summary)) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "%s",
                     summary.diagnostics[0] ? summary.diagnostics
                                            : "retained runtime scene projection failed");
        }
        return false;
    }

    if (out_diagnostics && out_diagnostics_size > 0 && summary.scene_id[0]) {
        snprintf(out_diagnostics,
                 out_diagnostics_size,
                 "scene_id=%s objects=%d",
                 summary.scene_id,
                 summary.object_count);
    }
    return true;
}
