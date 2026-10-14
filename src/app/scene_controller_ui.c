#include "app/scene_controller_ui.h"
#include "app/scene_controller_internal.h"
void scene_controller_ui_cancel(void *user) {
    SceneControllerUiOwner *owner=user;
    if (!owner) return;
    if (owner->sampler) {
        owner->sampler->pointer_down=false;
        owner->sampler->accumulator=0;
    }
    if (owner->scene) {
        owner->scene->runtime_view.viewport.navigation_active=false;
        owner->scene->runtime_view.viewport.navigation_mode=SCENE_EDITOR_VIEWPORT_NAV_NONE;
        owner->scene->runtime_view.viewport.alt_modifier_down=false;
    }
}
