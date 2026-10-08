#ifndef SIM_RUNTIME_BACKEND_2D_INTERNAL_H
#define SIM_RUNTIME_BACKEND_2D_INTERNAL_H

#include <fisics/extensions.h>

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
    int allocation_w;
    int allocation_h;
    uint8_t *static_mask;
    uint8_t *obstacle_mask;
    float *obstacle_vel_x;
    float *obstacle_vel_y;
    float *obstacle_distance;
    bool obstacle_mask_dirty;
    SceneEmitterMask2D emitter_masks[MAX_FLUID_EMITTERS];
    bool emitter_masks_dirty;
    int wind_ramp_steps;
    bool atmospheric_seeded;
    uint32_t atmospheric_seed;
    size_t atmospheric_seeded_cell_count;
    float atmospheric_seed_max_density;
    float atmospheric_seed_max_velocity_magnitude;
} SimRuntimeBackend2D;

/* Immutable-by-owner allocation extent. Native pointers/arrays must remain
 * initialized and truthful; this does not authenticate arbitrary memory. */
static inline bool backend_2d_storage_grid_valid(const SimRuntimeBackend2D *state) {
    const size_t max_cells = 64u * 1024u * 1024u;
    return state && state->allocation_w >= 2 && state->allocation_h >= 2 &&
        (size_t)state->allocation_w <= max_cells / (size_t)state->allocation_h &&
        (!state->fluid || (state->fluid->w == state->allocation_w &&
                           state->fluid->h == state->allocation_h));
}

static inline bool backend_2d_config_grid_matches(const SimRuntimeBackend2D *state,
                                                 const AppConfig *cfg) {
    return backend_2d_storage_grid_valid(state) && cfg &&
        cfg->grid_w == state->allocation_w && cfg->grid_h == state->allocation_h;
}

static inline bool backend_2d_scene_grid_matches(const SimRuntimeBackend2D *state,
                                                const SceneState *scene) {
    return scene && backend_2d_config_grid_matches(state, scene->config);
}

static inline SimRuntimeBackend2D *backend_2d_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend2D *)backend->impl : NULL;
}

static inline const SimRuntimeBackend2D *backend_2d_state_const(const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend2D *)backend->impl : NULL;
}

/* Read-only admission for the complete initial allocation set; failure leaves
 * out_bytes unchanged. Does not include later emitters, raster scratch or logs. */
bool backend_2d_initial_storage_bytes(int w, int h, size_t *out_bytes);

void backend_2d_free_emitter_masks(SimRuntimeBackend2D *state);

float backend_2d_import_pos_to_unit(float pos, float span);
void backend_2d_apply_mask_or(uint8_t *dst, const uint8_t *src, size_t count);
/* Initialized scene/library and truthful caller buffer required. mask_count must
 * equal the admitted grid cell count; failure leaves caller bytes unchanged. */
bool backend_2d_rasterize_import_to_mask(const SceneState *scene,
                                         const ImportedShape *imp,
                                         uint8_t *out_mask,
                                         size_t mask_count);
void backend_2d_compute_obstacle_distance(const SceneState *scene,
                                          SimRuntimeBackend2D *state);

void backend_2d_build_emitter_masks(SimRuntimeBackend *backend, SceneState *scene);
void backend_2d_rasterize_dynamic_obstacles(SimRuntimeBackend *backend, SceneState *scene);
void backend_2d_apply_emitters(SimRuntimeBackend *backend,
                               SceneState *scene,
                               double dt FISICS_DIM(time) FISICS_UNIT(second));

#endif // SIM_RUNTIME_BACKEND_2D_INTERNAL_H
