#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_backend_3d_emitter_shapes.h"

#include "app/scene_state.h"
#include "app/sim_runtime_emitter.h"

#include <math.h>

static const float SCAFFOLD_EMITTER_POWER_BOOST = 40.0f;
static const float SCAFFOLD_REF_VOLUME_CELLS = 96.0f * 96.0f * 96.0f;

static void rotate_xy(float *x, float *y, float radians) {
    float in_x = 0.0f;
    float in_y = 0.0f;
    float c = 0.0f;
    float s = 0.0f;
    if (!x || !y) return;
    in_x = *x;
    in_y = *y;
    c = cosf(radians);
    s = sinf(radians);
    *x = in_x * c - in_y * s;
    *y = in_x * s + in_y * c;
}

static float backend_3d_scaffold_emitter_strength_scale(const SceneState *scene,
                                                        FluidEmitterType type) {
    if (!scene || !scene->config) return 1.0f;
    switch (type) {
    case EMITTER_DENSITY_SOURCE:
        return scene->config->emitter_density_multiplier;
    case EMITTER_VELOCITY_JET:
        return scene->config->emitter_velocity_multiplier;
    case EMITTER_SINK:
        return scene->config->emitter_sink_multiplier;
    default:
        break;
    }
    return 1.0f;
}

static float backend_3d_scaffold_emitter_total_strength(const SceneState *scene,
                                                        const SimRuntimeEmitterResolved *emitter,
                                                        float dt,
                                                        size_t volume_cells) {
    float scale = 1.0f;
    float volume_scale = 1.0f;
    if (!emitter || dt <= 0.0f) return 0.0f;
    scale = backend_3d_scaffold_emitter_strength_scale(scene, emitter->type);
    if (volume_cells > 0) {
        volume_scale = (float)volume_cells / SCAFFOLD_REF_VOLUME_CELLS;
        if (volume_scale < 0.25f) volume_scale = 0.25f;
        if (volume_scale > 8.0f) volume_scale = 8.0f;
    }
    return emitter->strength * scale * SCAFFOLD_EMITTER_POWER_BOOST * dt * volume_scale;
}

static bool backend_3d_scaffold_cell_in_sphere(const SimRuntimeEmitterPlacement3D *placement,
                                               int x,
                                               int y,
                                               int z) {
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float radius = 0.0f;
    if (!placement) return false;
    dx = (float)(x - placement->center_x);
    dy = (float)(y - placement->center_y);
    dz = (float)(z - placement->center_z);
    radius = (float)placement->radius_cells;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

static bool backend_3d_scaffold_cell_in_oriented_box(const SimRuntimeEmitterOrientedBox3D *box,
                                                     int x,
                                                     int y,
                                                     int z) {
    float dx = 0.0f;
    float dy = 0.0f;
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (!box) return false;
    if (fabsf((float)(z - box->center_z)) > box->half_z) return false;
    dx = (float)(x - box->center_x);
    dy = (float)(y - box->center_y);
    local_x = dx * box->cos_a + dy * box->sin_a;
    local_y = -dx * box->sin_a + dy * box->cos_a;
    return fabsf(local_x) <= box->half_x && fabsf(local_y) <= box->half_y;
}

static size_t backend_3d_scaffold_count_sphere_cells(const SimRuntimeEmitterPlacement3D *placement) {
    size_t cells = 0;
    if (!placement) return 0;
    for (int z = placement->min_z; z <= placement->max_z; ++z) {
        for (int y = placement->min_y; y <= placement->max_y; ++y) {
            for (int x = placement->min_x; x <= placement->max_x; ++x) {
                if (backend_3d_scaffold_cell_in_sphere(placement, x, y, z)) ++cells;
            }
        }
    }
    return cells;
}

static size_t backend_3d_scaffold_count_oriented_box_cells(const SimRuntimeEmitterOrientedBox3D *box) {
    size_t cells = 0;
    if (!box) return 0;
    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                if (backend_3d_scaffold_cell_in_oriented_box(box, x, y, z)) ++cells;
            }
        }
    }
    return cells;
}

static void backend_3d_scaffold_apply_cell(SimRuntimeBackend3DScaffold *state,
                                           const SimRuntimeEmitterResolved *emitter,
                                           size_t idx,
                                           float per_cell) {
    if (!state || !emitter || per_cell <= 0.0f) return;
    switch (emitter->type) {
    case EMITTER_DENSITY_SOURCE:
        state->volume.density[idx] += per_cell;
        if (emitter->direction_has_magnitude) {
            state->volume.velocity_x[idx] += emitter->dir_x * per_cell * 0.25f;
            state->volume.velocity_y[idx] += emitter->dir_y * per_cell * 0.25f;
            state->volume.velocity_z[idx] += emitter->dir_z * per_cell * 0.25f;
        }
        break;
    case EMITTER_VELOCITY_JET:
        state->volume.velocity_x[idx] += emitter->dir_x * per_cell;
        state->volume.velocity_y[idx] += emitter->dir_y * per_cell;
        state->volume.velocity_z[idx] += emitter->dir_z * per_cell;
        state->volume.density[idx] += per_cell * 0.3f;
        break;
    case EMITTER_SINK:
        state->volume.density[idx] -= per_cell * 0.5f;
        state->volume.velocity_x[idx] -= emitter->dir_x * per_cell * 0.4f;
        state->volume.velocity_y[idx] -= emitter->dir_y * per_cell * 0.4f;
        state->volume.velocity_z[idx] -= emitter->dir_z * per_cell * 0.4f;
        break;
    }
}

static void backend_3d_scaffold_apply_sphere(SimRuntimeBackend3DScaffold *state,
                                             const SceneState *scene,
                                             const SimRuntimeEmitterResolved *emitter,
                                             const SimRuntimeEmitterPlacement3D *placement,
                                             double dt) {
    size_t cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    if (!state || !scene || !emitter || !placement) return;
    cells = backend_3d_scaffold_count_sphere_cells(placement);
    if (cells == 0) return;
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                emitter,
                                                                (float)dt,
                                                                state->volume.desc.cell_count);
    per_cell = total_strength / (float)cells;
    if (per_cell <= 0.0f) return;

    for (int z = placement->min_z; z <= placement->max_z; ++z) {
        for (int y = placement->min_y; y <= placement->max_y; ++y) {
            for (int x = placement->min_x; x <= placement->max_x; ++x) {
                size_t idx = 0;
                if (!backend_3d_scaffold_cell_in_sphere(placement, x, y, z)) continue;
                idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                backend_3d_scaffold_apply_cell(state, emitter, idx, per_cell);
            }
        }
    }
}

static void backend_3d_scaffold_apply_oriented_box(SimRuntimeBackend3DScaffold *state,
                                                   const SceneState *scene,
                                                   const SimRuntimeEmitterResolved *emitter,
                                                   const SimRuntimeEmitterOrientedBox3D *box,
                                                   double dt) {
    size_t cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    if (!state || !scene || !emitter || !box) return;
    cells = backend_3d_scaffold_count_oriented_box_cells(box);
    if (cells == 0) return;
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                emitter,
                                                                (float)dt,
                                                                state->volume.desc.cell_count);
    per_cell = total_strength / (float)cells;
    if (per_cell <= 0.0f) return;

    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                size_t idx = 0;
                if (!backend_3d_scaffold_cell_in_oriented_box(box, x, y, z)) continue;
                idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                backend_3d_scaffold_apply_cell(state, emitter, idx, per_cell);
            }
        }
    }
}

static void backend_3d_scaffold_apply_free_emitter(SimRuntimeBackend3DScaffold *state,
                                                   const SceneState *scene,
                                                   const SimRuntimeEmitterResolved *emitter,
                                                   double dt) {
    SimRuntimeEmitterPlacement3D placement = {0};
    if (!state || !scene || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
}

static void backend_3d_scaffold_apply_attached_object_emitter(SimRuntimeBackend3DScaffold *state,
                                                              const SceneState *scene,
                                                              const SimRuntimeEmitterResolved *emitter,
                                                              double dt) {
    SimRuntimeEmitterPlacement3D placement = {0};
    SimRuntimeEmitterOrientedBox3D box = {0};
    const PresetObject *object = NULL;
    int index = -1;
    if (!state || !scene || !scene->preset || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    index = emitter->attached_object;
    if (index < 0 || index >= (int)scene->preset->object_count) {
        backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
        return;
    }
    object = &scene->preset->objects[index];
    if (object->type == PRESET_OBJECT_CIRCLE) {
        SimRuntimeEmitterPlacement3D object_sphere = placement;
        if (!backend_3d_scaffold_build_attached_object_sphere(
                &state->volume.desc, &placement, object, &object_sphere)) {
            backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
            return;
        }
        backend_3d_scaffold_apply_sphere(state, scene, emitter, &object_sphere, dt);
        return;
    }
    if (!backend_3d_scaffold_build_object_box(&state->volume.desc, &placement, object, &box)) {
        backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
        return;
    }
    backend_3d_scaffold_apply_oriented_box(state, scene, emitter, &box, dt);
}

static void backend_3d_scaffold_apply_attached_import_emitter(SimRuntimeBackend3DScaffold *state,
                                                              const SceneState *scene,
                                                              const SimRuntimeEmitterResolved *emitter,
                                                              double dt) {
    SimRuntimeEmitterPlacement3D placement = {0};
    SimRuntimeEmitterOrientedBox3D box = {0};
    SimRuntimeEmitterResolved rotated = {0};
    const ImportedShape *imp = NULL;
    int index = -1;
    if (!state || !scene || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    index = emitter->attached_import;
    if (index < 0 || index >= (int)scene->import_shape_count) {
        backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
        return;
    }
    imp = &scene->import_shapes[index];
    if (!imp->enabled) {
        backend_3d_scaffold_apply_sphere(state, scene, emitter, &placement, dt);
        return;
    }

    rotated = *emitter;
    rotate_xy(&rotated.dir_x, &rotated.dir_y, imp->rotation_deg * (float)M_PI / 180.0f);
    if (!backend_3d_scaffold_build_import_box(&state->volume.desc, &placement, imp, &box)) {
        backend_3d_scaffold_apply_sphere(state, scene, &rotated, &placement, dt);
        return;
    }
    backend_3d_scaffold_apply_oriented_box(state, scene, &rotated, &box, dt);
}

void backend_3d_scaffold_apply_emitters(SimRuntimeBackend *backend,
                                        SceneState *scene,
                                        double dt) {
    SimRuntimeBackend3DScaffold *state = NULL;
    if (!backend || !scene || !scene->preset || dt <= 0.0) return;
    if (!scene->emitters_enabled) return;
    state = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!state) return;

    for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        SimRuntimeEmitterResolved emitter = {0};
        if (!sim_runtime_emitter_resolve(scene->preset, i, &emitter)) continue;
        switch (emitter.source_kind) {
        case SIM_RUNTIME_EMITTER_SOURCE_FREE:
            backend_3d_scaffold_apply_free_emitter(state, scene, &emitter, dt);
            break;
        case SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT:
            backend_3d_scaffold_apply_attached_object_emitter(state, scene, &emitter, dt);
            break;
        case SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT:
            backend_3d_scaffold_apply_attached_import_emitter(state, scene, &emitter, dt);
            break;
        default:
            break;
        }
    }

    state->fluid_slice_dirty = true;
}
