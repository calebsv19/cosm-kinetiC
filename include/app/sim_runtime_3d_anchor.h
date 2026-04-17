#ifndef SIM_RUNTIME_3D_ANCHOR_H
#define SIM_RUNTIME_3D_ANCHOR_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_emitter.h"
#include "core_object.h"
#include "core_scene.h"

struct SceneState;

CoreObjectVec3 sim_runtime_3d_anchor_retained_object_origin(const CoreSceneObjectContract *object);
bool sim_runtime_3d_anchor_resolve_emitter_world_anchor(const struct SceneState *scene,
                                                        int attached_object,
                                                        int attached_import,
                                                        double position_x,
                                                        double position_y,
                                                        double position_z,
                                                        const CoreObjectVec3 *world_min,
                                                        const CoreObjectVec3 *world_max,
                                                        CoreObjectVec3 *out_world);
bool sim_runtime_3d_anchor_resolve_preset_emitter_world_anchor(const struct SceneState *scene,
                                                               const FluidEmitter *emitter,
                                                               const CoreObjectVec3 *world_min,
                                                               const CoreObjectVec3 *world_max,
                                                               CoreObjectVec3 *out_world);
bool sim_runtime_3d_anchor_resolve_resolved_emitter_world_anchor(
    const struct SceneState *scene,
    const SimRuntime3DDomainDesc *desc,
    const SimRuntimeEmitterResolved *emitter,
    CoreObjectVec3 *out_world);

#endif // SIM_RUNTIME_3D_ANCHOR_H
