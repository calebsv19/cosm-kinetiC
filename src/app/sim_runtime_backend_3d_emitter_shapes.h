#ifndef SIM_RUNTIME_BACKEND_3D_EMITTER_SHAPES_H
#define SIM_RUNTIME_BACKEND_3D_EMITTER_SHAPES_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "app/sim_runtime_backend_3d_oriented_box.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_emitter.h"

struct SceneState;
typedef SimRuntimeOrientedBox3DCells SimRuntimeEmitterOrientedBox3D;

void backend_3d_scaffold_fill_sphere_bounds(const SimRuntime3DDomainDesc *desc,
                                            SimRuntimeEmitterPlacement3D *placement);
bool backend_3d_scaffold_resolve_emitter_placement(const struct SceneState *scene,
                                                   const SimRuntime3DDomainDesc *desc,
                                                   const SimRuntimeEmitterResolved *emitter,
                                                   SimRuntimeEmitterPlacement3D *out_placement);
bool backend_3d_scaffold_build_attached_object_sphere(
    const SimRuntime3DDomainDesc *desc,
    const SimRuntimeEmitterPlacement3D *placement,
    const PresetObject *object,
    SimRuntimeEmitterPlacement3D *out_placement);
bool backend_3d_scaffold_build_object_box(const SimRuntime3DDomainDesc *desc,
                                          const SimRuntimeEmitterPlacement3D *placement,
                                          const PresetObject *object,
                                          SimRuntimeEmitterOrientedBox3D *out_box);
bool backend_3d_scaffold_build_import_box(const SimRuntime3DDomainDesc *desc,
                                          const SimRuntimeEmitterPlacement3D *placement,
                                          const ImportedShape *imp,
                                          SimRuntimeEmitterOrientedBox3D *out_box);

#endif
