#ifndef PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_H
#define PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_H

#include "core_scene.h"
void retained_runtime_overlay_fill_plane_corners(const CoreScenePlanePrimitive *plane,
                                                 CoreObjectVec3 out_corners[4]);
void retained_runtime_overlay_fill_prism_corners(const CoreSceneRectPrismPrimitive *prism,
                                                 CoreObjectVec3 out_corners[8]);

#endif
