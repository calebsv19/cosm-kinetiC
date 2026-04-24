#include "app/sim_runtime_backend.h"

#include <stdlib.h>

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual);
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *cfg,
                                                          const FluidScenePreset *preset,
                                                          const SimModeRoute *mode_route,
                                                          const PhysicsSimRuntimeVisualBootstrap *runtime_visual);

SimRuntimeBackend *sim_runtime_backend_create(const AppConfig *cfg,
                                              const FluidScenePreset *preset,
                                              const SimModeRoute *mode_route,
                                              const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    if (!cfg) {
        return NULL;
    }
    if (mode_route && mode_route->backend_lane == SIM_BACKEND_CONTROLLED_3D) {
        return sim_runtime_backend_3d_scaffold_create(cfg, preset, mode_route, runtime_visual);
    }
    return sim_runtime_backend_2d_create(cfg, preset, mode_route, runtime_visual);
}

void sim_runtime_backend_destroy(SimRuntimeBackend *backend) {
    if (!backend) return;
    if (backend->ops && backend->ops->destroy) {
        backend->ops->destroy(backend);
        return;
    }
    free(backend);
}

SimRuntimeBackendKind sim_runtime_backend_kind(const SimRuntimeBackend *backend) {
    return backend ? backend->kind : SIM_RUNTIME_BACKEND_KIND_NONE;
}

bool sim_runtime_backend_valid(const SimRuntimeBackend *backend) {
    return backend && backend->ops && backend->ops->valid && backend->ops->valid(backend);
}

void sim_runtime_backend_clear(SimRuntimeBackend *backend) {
    if (backend && backend->ops && backend->ops->clear) {
        backend->ops->clear(backend);
    }
}

bool sim_runtime_backend_apply_brush_sample(SimRuntimeBackend *backend,
                                            const AppConfig *cfg,
                                            const StrokeSample *sample) {
    return backend && backend->ops && backend->ops->apply_brush_sample &&
           backend->ops->apply_brush_sample(backend, cfg, sample);
}

void sim_runtime_backend_build_static_obstacles(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->build_static_obstacles) {
        backend->ops->build_static_obstacles(backend, scene);
    }
}

void sim_runtime_backend_build_emitter_masks(SimRuntimeBackend *backend,
                                             struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->build_emitter_masks) {
        backend->ops->build_emitter_masks(backend, scene);
    }
}

void sim_runtime_backend_mark_emitters_dirty(SimRuntimeBackend *backend) {
    if (backend && backend->ops && backend->ops->mark_emitters_dirty) {
        backend->ops->mark_emitters_dirty(backend);
    }
}

void sim_runtime_backend_build_obstacles(SimRuntimeBackend *backend,
                                         struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->build_obstacles) {
        backend->ops->build_obstacles(backend, scene);
    }
}

void sim_runtime_backend_mark_obstacles_dirty(SimRuntimeBackend *backend) {
    if (backend && backend->ops && backend->ops->mark_obstacles_dirty) {
        backend->ops->mark_obstacles_dirty(backend);
    }
}

void sim_runtime_backend_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                     struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->rasterize_dynamic_obstacles) {
        backend->ops->rasterize_dynamic_obstacles(backend, scene);
    }
}

void sim_runtime_backend_apply_emitters(SimRuntimeBackend *backend,
                                        struct SceneState *scene,
                                        double dt) {
    if (backend && backend->ops && backend->ops->apply_emitters) {
        backend->ops->apply_emitters(backend, scene, dt);
    }
}

void sim_runtime_backend_apply_boundary_flows(SimRuntimeBackend *backend,
                                              struct SceneState *scene,
                                              double dt) {
    if (backend && backend->ops && backend->ops->apply_boundary_flows) {
        backend->ops->apply_boundary_flows(backend, scene, dt);
    }
}

void sim_runtime_backend_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->enforce_boundary_flows) {
        backend->ops->enforce_boundary_flows(backend, scene);
    }
}

void sim_runtime_backend_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->enforce_obstacles) {
        backend->ops->enforce_obstacles(backend, scene);
    }
}

void sim_runtime_backend_step(SimRuntimeBackend *backend,
                              struct SceneState *scene,
                              const AppConfig *cfg,
                              double dt) {
    if (backend && backend->ops && backend->ops->step) {
        backend->ops->step(backend, scene, cfg, dt);
    }
}

void sim_runtime_backend_inject_object_motion(SimRuntimeBackend *backend,
                                              const struct SceneState *scene) {
    if (backend && backend->ops && backend->ops->inject_object_motion) {
        backend->ops->inject_object_motion(backend, scene);
    }
}

void sim_runtime_backend_reset_transient_state(SimRuntimeBackend *backend) {
    if (backend && backend->ops && backend->ops->reset_transient_state) {
        backend->ops->reset_transient_state(backend);
    }
}

void sim_runtime_backend_seed_uniform_velocity_2d(SimRuntimeBackend *backend,
                                                  float velocity_x,
                                                  float velocity_y) {
    if (backend && backend->ops && backend->ops->seed_uniform_velocity_2d) {
        backend->ops->seed_uniform_velocity_2d(backend, velocity_x, velocity_y);
    }
}

bool sim_runtime_backend_export_snapshot(const SimRuntimeBackend *backend,
                                         double time,
                                         const char *path) {
    return backend && backend->ops && backend->ops->export_snapshot &&
           backend->ops->export_snapshot(backend, time, path);
}

bool sim_runtime_backend_get_fluid_view_2d(const SimRuntimeBackend *backend,
                                           SceneFluidFieldView2D *out_view) {
    return backend && backend->ops && backend->ops->get_fluid_view_2d &&
           backend->ops->get_fluid_view_2d(backend, out_view);
}

bool sim_runtime_backend_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                              SceneObstacleFieldView2D *out_view) {
    return backend && backend->ops && backend->ops->get_obstacle_view_2d &&
           backend->ops->get_obstacle_view_2d(backend, out_view);
}

bool sim_runtime_backend_get_debug_volume_view_3d(const SimRuntimeBackend *backend,
                                                  SceneDebugVolumeView3D *out_view) {
    return backend && backend->ops && backend->ops->get_debug_volume_view_3d &&
           backend->ops->get_debug_volume_view_3d(backend, out_view);
}

bool sim_runtime_backend_get_volume_export_view_3d(const SimRuntimeBackend *backend,
                                                   SceneFluidVolumeExportView3D *out_view) {
    return backend && backend->ops && backend->ops->get_volume_export_view_3d &&
           backend->ops->get_volume_export_view_3d(backend, out_view);
}

bool sim_runtime_backend_get_report(const SimRuntimeBackend *backend,
                                    SimRuntimeBackendReport *out_report) {
    return backend && backend->ops && backend->ops->get_report &&
           backend->ops->get_report(backend, out_report);
}

bool sim_runtime_backend_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                          int slice_z,
                                                          bool *out_has_fluid,
                                                          bool *out_has_obstacles) {
    return backend && backend->ops && backend->ops->get_compatibility_slice_activity &&
           backend->ops->get_compatibility_slice_activity(
               backend, slice_z, out_has_fluid, out_has_obstacles);
}

bool sim_runtime_backend_step_compatibility_slice(SimRuntimeBackend *backend, int delta_z) {
    return backend && backend->ops && backend->ops->step_compatibility_slice &&
           backend->ops->step_compatibility_slice(backend, delta_z);
}
