#ifndef PHYSICS_SIM_SCENE_CONTROLLER_UI_H
#define PHYSICS_SIM_SCENE_CONTROLLER_UI_H
#include "app/scene_state.h"
struct StrokeSampler;
typedef struct SceneControllerUiOwner {
    SceneState *scene;
    struct StrokeSampler *sampler;
} SceneControllerUiOwner;
void scene_controller_ui_cancel(void *);
#endif
