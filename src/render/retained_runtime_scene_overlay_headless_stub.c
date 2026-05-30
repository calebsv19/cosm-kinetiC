#include "render/retained_runtime_scene_overlay.h"

#include <stdbool.h>

bool retained_runtime_scene_overlay_active(const SceneState *scene) {
    (void)scene;
    return false;
}

bool retained_runtime_scene_overlay_slice_debug_enabled(const SceneState *scene) {
    (void)scene;
    return false;
}

bool retained_runtime_scene_overlay_frame_view(SceneState *scene,
                                               int window_w,
                                               int window_h) {
    (void)scene;
    (void)window_w;
    (void)window_h;
    return false;
}

void retained_runtime_scene_overlay_draw(const SceneState *scene,
                                         SDL_Renderer *renderer,
                                         int window_w,
                                         int window_h) {
    (void)scene;
    (void)renderer;
    (void)window_w;
    (void)window_h;
}
