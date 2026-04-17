#include "app/scene_apply.h"

#include "app/sim_runtime_backend.h"

void scene_apply_emitters(SceneState *scene, double dt) {
    if (!scene) return;
    sim_runtime_backend_apply_emitters(scene->backend, scene, dt);
}

void scene_apply_boundary_flows(SceneState *scene, double dt) {
    if (!scene) return;
    sim_runtime_backend_apply_boundary_flows(scene->backend, scene, dt);
}

void scene_enforce_boundary_flows(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_enforce_boundary_flows(scene->backend, scene);
}

void scene_enforce_obstacles(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_enforce_obstacles(scene->backend, scene);
}
