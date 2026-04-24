#ifndef SIM_RUNTIME_BACKEND_2D_INTERNAL_H
#define SIM_RUNTIME_BACKEND_2D_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "physics/fluid2d/fluid2d.h"

typedef struct SceneEmitterMask2D {
    uint8_t *mask;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
} SceneEmitterMask2D;

typedef struct SimRuntimeBackend2D {
    Fluid2D *fluid;
    uint8_t *static_mask;
    uint8_t *obstacle_mask;
    float *obstacle_vel_x;
    float *obstacle_vel_y;
    float *obstacle_distance;
    bool obstacle_mask_dirty;
    SceneEmitterMask2D emitter_masks[MAX_FLUID_EMITTERS];
    bool emitter_masks_dirty;
    int wind_ramp_steps;
} SimRuntimeBackend2D;

static inline SimRuntimeBackend2D *backend_2d_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend2D *)backend->impl : NULL;
}

static inline const SimRuntimeBackend2D *backend_2d_state_const(const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend2D *)backend->impl : NULL;
}

void backend_2d_free_emitter_masks(SimRuntimeBackend2D *state);

float backend_2d_import_pos_to_unit(float pos, float span);
void backend_2d_apply_mask_or(uint8_t *dst, const uint8_t *src, size_t count);
bool backend_2d_rasterize_import_to_mask(const SceneState *scene,
                                         const ImportedShape *imp,
                                         uint8_t *out_mask,
                                         size_t mask_count);
void backend_2d_compute_obstacle_distance(const SceneState *scene,
                                          SimRuntimeBackend2D *state);

void backend_2d_build_emitter_masks(SimRuntimeBackend *backend, SceneState *scene);
void backend_2d_rasterize_dynamic_obstacles(SimRuntimeBackend *backend, SceneState *scene);
void backend_2d_apply_emitters(SimRuntimeBackend *backend, SceneState *scene, double dt);

#endif // SIM_RUNTIME_BACKEND_2D_INTERNAL_H
