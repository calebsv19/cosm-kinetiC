#include "app/scene_controller_util.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "app/scene_state.h"

bool scene_controller_env_flag_enabled(const char *name) {
    if (!name) {
        return false;
    }
    const char *value = getenv(name);
    if (!value || !value[0]) {
        return false;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

bool scene_controller_rs1_diag_enabled(void) {
    return scene_controller_env_flag_enabled("PHYSICS_SIM_RS1_DIAG");
}

bool scene_controller_ir1_diag_enabled(void) {
    return scene_controller_env_flag_enabled("PHYSICS_SIM_IR1_DIAG");
}

bool scene_controller_runtime_interaction_active(const SceneState *scene) {
    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    bool pointer_buttons_active =
        (buttons & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK)) != 0u;
    return pointer_buttons_active ||
           (scene && scene->runtime_viewport.navigation_active);
}

uint32_t scene_controller_count_bool(bool value) {
    return value ? 1u : 0u;
}
