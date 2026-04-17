#ifndef PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_SPACE_H
#define PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_SPACE_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "core_scene.h"

typedef struct SceneState SceneState;

bool retained_runtime_overlay_compute_visual_bounds(const SceneState *scene,
                                                    CoreObjectVec3 *out_min,
                                                    CoreObjectVec3 *out_max);
double retained_runtime_overlay_slice_world_z_for_index(const SceneState *scene,
                                                        CoreObjectVec3 visual_min,
                                                        CoreObjectVec3 visual_max,
                                                        int slice_z);
double retained_runtime_overlay_slice_z(const SceneState *scene,
                                        CoreObjectVec3 visual_min,
                                        CoreObjectVec3 visual_max);
double retained_runtime_overlay_slice_tolerance(const SceneState *scene);
bool retained_runtime_overlay_object_slice_intersects(const SceneState *scene,
                                                      const CoreSceneObjectContract *object,
                                                      double slice_z);
bool retained_runtime_overlay_emitter_actual_and_slice_points(const SceneState *scene,
                                                              const FluidEmitter *emitter,
                                                              CoreObjectVec3 *out_actual,
                                                              CoreObjectVec3 *out_slice);

#endif
