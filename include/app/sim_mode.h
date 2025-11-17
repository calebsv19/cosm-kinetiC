#ifndef SIM_MODE_H
#define SIM_MODE_H

#include "app/app_config.h"
#include "app/scene_presets.h"

struct SceneState;

typedef struct SimModeHooks {
    void (*configure_app)(AppConfig *cfg, FluidScenePreset *preset);
    void (*prepare_scene)(struct SceneState *scene);
    void (*pre_substep)(struct SceneState *scene, double dt);
    void (*post_substep)(struct SceneState *scene, double dt);
} SimModeHooks;

const SimModeHooks *sim_mode_get_hooks(SimulationMode mode);

#endif // SIM_MODE_H
