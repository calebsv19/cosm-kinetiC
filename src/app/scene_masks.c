#include "app/scene_masks.h"

#include "app/sim_runtime_backend.h"

void scene_masks_build_static(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_build_static_obstacles(scene->backend, scene);
}

void scene_masks_build_emitter(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_build_emitter_masks(scene->backend, scene);
}

void scene_masks_free_emitter_masks(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_mark_emitters_dirty(scene->backend);
}

void scene_masks_mark_emitters_dirty(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_mark_emitters_dirty(scene->backend);
}

void scene_masks_build_obstacle(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_build_obstacles(scene->backend, scene);
}

void scene_masks_rasterize_dynamic(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_rasterize_dynamic_obstacles(scene->backend, scene);
}
