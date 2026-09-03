#ifndef SCENE_APPLY_H
#define SCENE_APPLY_H

#include <fisics/extensions.h>

#include "app/scene_state.h"

void scene_apply_emitters(SceneState *scene,
                          double dt FISICS_DIM(time) FISICS_UNIT(second));
void scene_apply_boundary_flows(SceneState *scene,
                                double dt FISICS_DIM(time) FISICS_UNIT(second));
void scene_enforce_boundary_flows(SceneState *scene);
void scene_enforce_obstacles(SceneState *scene);

#endif // SCENE_APPLY_H
