#include "import/runtime_scene_solver_projection.h"

#include "import/runtime_scene_solver_projection_internal.h"

bool runtime_scene_solver_projection_apply_runtime(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                   json_object *runtime_root,
                                                   AppConfig *in_out_cfg,
                                                   FluidScenePreset *in_out_preset,
                                                   RuntimeSceneBridgePreflight *out_summary) {
    double world_scale = 1.0;
    if (!retained_scene || !runtime_root || !in_out_cfg || !in_out_preset || !out_summary) return false;
    world_scale = retained_scene->root.world_scale;
    if (world_scale <= 0.0) world_scale = 1.0;

    runtime_scene_solver_projection_apply_space_mode(retained_scene, in_out_cfg, in_out_preset);
    runtime_scene_solver_projection_apply_scene_domain(retained_scene, runtime_root, in_out_preset);
    if (retained_scene->retained_object_count > 0) {
        runtime_scene_solver_projection_apply_objects(retained_scene,
                                                      runtime_root,
                                                      in_out_preset,
                                                      out_summary);
        if (runtime_scene_solver_projection_apply_emitters_from_retained_objects(retained_scene,
                                                                                 runtime_root,
                                                                                 world_scale,
                                                                                 in_out_preset) == 0) {
            runtime_scene_solver_projection_apply_emitters_from_lights(runtime_root,
                                                                       world_scale,
                                                                       in_out_preset);
        }
    } else {
        runtime_scene_solver_projection_apply_runtime_root_objects(runtime_root,
                                                                   world_scale,
                                                                   in_out_preset,
                                                                   out_summary);
        if (runtime_scene_solver_projection_apply_emitters_from_runtime_root_objects(runtime_root,
                                                                                     world_scale,
                                                                                     in_out_preset) == 0) {
            runtime_scene_solver_projection_apply_emitters_from_lights(runtime_root,
                                                                       world_scale,
                                                                       in_out_preset);
        }
    }
    return true;
}
