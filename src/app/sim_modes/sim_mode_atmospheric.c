#include "app/sim_mode.h"

#include "app/atmospheric/atmospheric_field.h"
#include "app/scene_state.h"

static void atmospheric_configure(AppConfig *cfg, FluidScenePreset *preset) {
    if (!cfg || !preset) return;
    preset->domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset->dimension_mode = (cfg->space_mode == SPACE_MODE_3D)
                                  ? SCENE_DIMENSION_MODE_3D
                                  : SCENE_DIMENSION_MODE_2D;
    if (!preset->atmosphere.enabled) {
        preset->atmosphere = atmospheric_preset_default_settings();
    }
    atmospheric_preset_sanitize(&preset->atmosphere);
}

static void atmospheric_prepare(SceneState *scene) {
    (void)scene;
}

static void atmospheric_pre_substep(SceneState *scene, double dt) {
    (void)scene;
    (void)dt;
}

static void atmospheric_post_substep(SceneState *scene, double dt) {
    (void)scene;
    (void)dt;
}

const SimModeHooks g_sim_mode_atmospheric = {
    .configure_app = atmospheric_configure,
    .prepare_scene = atmospheric_prepare,
    .pre_substep = atmospheric_pre_substep,
    .post_substep = atmospheric_post_substep,
};
