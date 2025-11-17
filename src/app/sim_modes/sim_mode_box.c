#include "app/sim_mode.h"

#include "app/scene_state.h"

static void box_configure(AppConfig *cfg, FluidScenePreset *preset) {
    (void)cfg;
    (void)preset;
}

static void box_prepare(SceneState *scene) {
    (void)scene;
}

static void box_pre_substep(SceneState *scene, double dt) {
    (void)scene;
    (void)dt;
}

static void box_post_substep(SceneState *scene, double dt) {
    (void)scene;
    (void)dt;
}

const SimModeHooks g_sim_mode_box = {
    .configure_app = box_configure,
    .prepare_scene = box_prepare,
    .pre_substep   = box_pre_substep,
    .post_substep  = box_post_substep,
};
