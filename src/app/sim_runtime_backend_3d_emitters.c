#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_backend_3d_emitter_shapes.h"

#include "app/scene_state.h"
#include "app/sim_runtime_emitter.h"

#include <math.h>

static const float SCAFFOLD_EMITTER_POWER_BOOST = 40.0f;
static const float SCAFFOLD_REF_FOOTPRINT_CELLS = 128.0f;
static const double SCAFFOLD_IMPORT_DESIRED_FIT = 0.25;

static double backend_3d_scaffold_axis_span(double world_min, double world_max) {
    double span = world_max - world_min;
    return span > 0.0 ? span : 1.0;
}

static double backend_3d_scaffold_reference_xy_span(const SimRuntime3DDomainDesc *desc) {
    double span_x = 0.0;
    double span_y = 0.0;
    if (!desc) return 1.0;
    span_x = backend_3d_scaffold_axis_span(desc->world_min_x, desc->world_max_x);
    span_y = backend_3d_scaffold_axis_span(desc->world_min_y, desc->world_max_y);
    return span_x < span_y ? span_x : span_y;
}

static double backend_3d_scaffold_resolve_world_size(double authored_size, double world_span) {
    if (!(authored_size > 0.0)) return 0.0;
    if (authored_size <= 1.0) return authored_size * world_span;
    return authored_size;
}

static double backend_3d_scaffold_sphere_world_volume(double world_radius) {
    if (!(world_radius > 0.0)) return 0.0;
    return (4.0 / 3.0) * M_PI * world_radius * world_radius * world_radius;
}

static double backend_3d_scaffold_box_world_volume(double half_x,
                                                   double half_y,
                                                   double half_z) {
    if (!(half_x > 0.0) || !(half_y > 0.0) || !(half_z > 0.0)) return 0.0;
    return 8.0 * half_x * half_y * half_z;
}

static double backend_3d_scaffold_emitter_world_radius(const SimRuntime3DDomainDesc *desc,
                                                       float authored_radius) {
    if (!desc) return 0.0;
    return backend_3d_scaffold_resolve_world_size((double)authored_radius,
                                                  backend_3d_scaffold_reference_xy_span(desc));
}

static double backend_3d_scaffold_object_world_sphere_radius(const SimRuntime3DDomainDesc *desc,
                                                             const PresetObject *object) {
    double half_x = 0.0;
    double half_z = 0.0;
    double size_z = 0.0;
    if (!desc || !object) return 0.0;
    size_z = object->size_z > 0.0f ? (double)object->size_z : (double)object->size_x;
    half_x = backend_3d_scaffold_resolve_world_size((double)object->size_x,
                                                    backend_3d_scaffold_reference_xy_span(desc));
    half_z = backend_3d_scaffold_resolve_world_size(
        size_z,
        backend_3d_scaffold_axis_span(desc->world_min_z, desc->world_max_z));
    return half_z > half_x ? half_z : half_x;
}

static double backend_3d_scaffold_object_world_half_extent_x(const SimRuntime3DDomainDesc *desc,
                                                             const PresetObject *object) {
    if (!desc || !object) return 0.0;
    return backend_3d_scaffold_resolve_world_size(
        (double)object->size_x,
        backend_3d_scaffold_axis_span(desc->world_min_x, desc->world_max_x));
}

static double backend_3d_scaffold_object_world_half_extent_y(const SimRuntime3DDomainDesc *desc,
                                                             const PresetObject *object) {
    double size_y = 0.0;
    if (!desc || !object) return 0.0;
    size_y = object->size_y > 0.0f ? (double)object->size_y : (double)object->size_x;
    return backend_3d_scaffold_resolve_world_size(
        size_y,
        backend_3d_scaffold_axis_span(desc->world_min_y, desc->world_max_y));
}

static double backend_3d_scaffold_object_world_half_extent_z(const SimRuntime3DDomainDesc *desc,
                                                             const PresetObject *object) {
    double size_z = 0.0;
    if (!desc || !object) return 0.0;
    size_z = object->size_z > 0.0f ? (double)object->size_z : (double)object->size_x;
    return backend_3d_scaffold_resolve_world_size(
        size_z,
        backend_3d_scaffold_axis_span(desc->world_min_z, desc->world_max_z));
}

static double backend_3d_scaffold_import_world_half_extent_x(const SimRuntime3DDomainDesc *desc,
                                                             const ImportedShape *imp) {
    double scale = 1.0;
    double scale_factor = 0.0;
    if (!desc || !imp) return 0.0;
    scale = imp->scale > 0.0f ? (double)imp->scale : 1.0;
    scale_factor = scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5;
    return backend_3d_scaffold_axis_span(desc->world_min_x, desc->world_max_x) * scale_factor;
}

static double backend_3d_scaffold_import_world_half_extent_y(const SimRuntime3DDomainDesc *desc,
                                                             const ImportedShape *imp) {
    double scale = 1.0;
    double scale_factor = 0.0;
    if (!desc || !imp) return 0.0;
    scale = imp->scale > 0.0f ? (double)imp->scale : 1.0;
    scale_factor = scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5;
    return backend_3d_scaffold_axis_span(desc->world_min_y, desc->world_max_y) * scale_factor;
}

static double backend_3d_scaffold_import_world_half_extent_z(const SimRuntime3DDomainDesc *desc,
                                                             const ImportedShape *imp) {
    double scale = 1.0;
    double scale_factor = 0.0;
    if (!desc || !imp) return 0.0;
    scale = imp->scale > 0.0f ? (double)imp->scale : 1.0;
    scale_factor = scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5;
    return backend_3d_scaffold_axis_span(desc->world_min_z, desc->world_max_z) * scale_factor;
}

static void backend_3d_scaffold_reset_emitter_step_stats(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->emitter_step_emitters_applied = 0;
    state->emitter_step_free_emitters_applied = 0;
    state->emitter_step_attached_emitters_applied = 0;
    state->emitter_step_affected_cells = 0;
    state->emitter_step_last_footprint_cells = 0;
    state->emitter_step_density_delta = 0.0f;
    state->emitter_step_velocity_magnitude_delta = 0.0f;
}

static void backend_3d_scaffold_accumulate_emitter_step_stats(
    SimRuntimeBackend3DScaffold *state,
    const SimRuntimeEmitterResolved *emitter,
    size_t affected_cells,
    float total_strength) {
    float density_delta = 0.0f;
    float velocity_delta = 0.0f;
    if (!state || !emitter || affected_cells == 0 || total_strength <= 0.0f) return;

    state->emitter_step_emitters_applied += 1;
    if (emitter->source_kind == SIM_RUNTIME_EMITTER_SOURCE_FREE) {
        state->emitter_step_free_emitters_applied += 1;
    } else {
        state->emitter_step_attached_emitters_applied += 1;
    }
    state->emitter_step_affected_cells += affected_cells;
    state->emitter_step_last_footprint_cells = affected_cells;

    switch (emitter->type) {
    case EMITTER_DENSITY_SOURCE:
        density_delta = total_strength;
        velocity_delta = emitter->direction_has_magnitude ? total_strength * 0.25f : 0.0f;
        break;
    case EMITTER_VELOCITY_JET:
        density_delta = total_strength * 0.3f;
        velocity_delta = emitter->direction_has_magnitude ? total_strength : 0.0f;
        break;
    case EMITTER_SINK:
        density_delta = -total_strength * 0.5f;
        velocity_delta = emitter->direction_has_magnitude ? total_strength * 0.4f : 0.0f;
        break;
    default:
        break;
    }

    state->emitter_step_density_delta += density_delta;
    state->emitter_step_velocity_magnitude_delta += velocity_delta;
}

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
                                                        const SimRuntime3DDomainDesc *desc,
                                                        const SimRuntimeEmitterResolved *emitter,
                                                        float dt,
                                                        size_t footprint_cells,
                                                        double footprint_world_volume) {
    float scale = 1.0f;
    float footprint_scale = 1.0f;
    double equivalent_cells = 0.0;
    double voxel_volume = 0.0;
    if (!emitter || dt <= 0.0f) return 0.0f;
    if (footprint_cells == 0) return 0.0f;
    scale = backend_3d_scaffold_emitter_strength_scale(scene, emitter->type);
    equivalent_cells = (double)footprint_cells;
    if (desc && desc->voxel_size > 0.0f && footprint_world_volume > 0.0) {
        voxel_volume = (double)desc->voxel_size *
                       (double)desc->voxel_size *
                       (double)desc->voxel_size;
        equivalent_cells = footprint_world_volume / voxel_volume;
        if (equivalent_cells < 1.0) equivalent_cells = 1.0;
    }
    footprint_scale = (float)(equivalent_cells / (double)SCAFFOLD_REF_FOOTPRINT_CELLS);
    return emitter->strength * scale * SCAFFOLD_EMITTER_POWER_BOOST * dt * footprint_scale;
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
    return sim_runtime_backend_3d_cell_in_oriented_box(box, x, y, z);
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
                                             double footprint_world_volume,
                                             double dt) {
    size_t cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    if (!state || !scene || !emitter || !placement) return;
    cells = backend_3d_scaffold_count_sphere_cells(placement);
    if (cells == 0) return;
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                &state->volume.desc,
                                                                emitter,
                                                                (float)dt,
                                                                cells,
                                                                footprint_world_volume);
    per_cell = total_strength / (float)cells;
    if (per_cell <= 0.0f) return;
    backend_3d_scaffold_accumulate_emitter_step_stats(state, emitter, cells, total_strength);

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
                                                   double footprint_world_volume,
                                                   double dt) {
    size_t cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    if (!state || !scene || !emitter || !box) return;
    cells = backend_3d_scaffold_count_oriented_box_cells(box);
    if (cells == 0) return;
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                &state->volume.desc,
                                                                emitter,
                                                                (float)dt,
                                                                cells,
                                                                footprint_world_volume);
    per_cell = total_strength / (float)cells;
    if (per_cell <= 0.0f) return;
    backend_3d_scaffold_accumulate_emitter_step_stats(state, emitter, cells, total_strength);

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
    double world_radius = 0.0;
    if (!state || !scene || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
    backend_3d_scaffold_apply_sphere(
        state, scene, emitter, &placement, backend_3d_scaffold_sphere_world_volume(world_radius), dt);
}

static void backend_3d_scaffold_apply_attached_object_emitter(SimRuntimeBackend3DScaffold *state,
                                                              const SceneState *scene,
                                                              const SimRuntimeEmitterResolved *emitter,
                                                              double dt) {
    SimRuntimeEmitterPlacement3D placement = {0};
    SimRuntimeEmitterOrientedBox3D box = {0};
    const PresetObject *object = NULL;
    double world_radius = 0.0;
    double half_x = 0.0;
    double half_y = 0.0;
    double half_z = 0.0;
    int index = -1;
    if (!state || !scene || !scene->preset || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    index = emitter->attached_object;
    if (index < 0 || index >= (int)scene->preset->object_count) {
        world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
        backend_3d_scaffold_apply_sphere(
            state, scene, emitter, &placement, backend_3d_scaffold_sphere_world_volume(world_radius), dt);
        return;
    }
    object = &scene->preset->objects[index];
    if (object->type == PRESET_OBJECT_CIRCLE) {
        SimRuntimeEmitterPlacement3D object_sphere = placement;
        if (!backend_3d_scaffold_build_attached_object_sphere(
                &state->volume.desc, &placement, object, &object_sphere)) {
            world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
            backend_3d_scaffold_apply_sphere(state,
                                             scene,
                                             emitter,
                                             &placement,
                                             backend_3d_scaffold_sphere_world_volume(world_radius),
                                             dt);
            return;
        }
        world_radius = backend_3d_scaffold_object_world_sphere_radius(&state->volume.desc, object);
        backend_3d_scaffold_apply_sphere(state,
                                         scene,
                                         emitter,
                                         &object_sphere,
                                         backend_3d_scaffold_sphere_world_volume(world_radius),
                                         dt);
        return;
    }
    if (!backend_3d_scaffold_build_object_box(&state->volume.desc, &placement, object, &box)) {
        world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
        backend_3d_scaffold_apply_sphere(
            state, scene, emitter, &placement, backend_3d_scaffold_sphere_world_volume(world_radius), dt);
        return;
    }
    half_x = backend_3d_scaffold_object_world_half_extent_x(&state->volume.desc, object);
    half_y = backend_3d_scaffold_object_world_half_extent_y(&state->volume.desc, object);
    half_z = backend_3d_scaffold_object_world_half_extent_z(&state->volume.desc, object);
    backend_3d_scaffold_apply_oriented_box(
        state, scene, emitter, &box, backend_3d_scaffold_box_world_volume(half_x, half_y, half_z), dt);
}

static void backend_3d_scaffold_apply_attached_import_emitter(SimRuntimeBackend3DScaffold *state,
                                                              const SceneState *scene,
                                                              const SimRuntimeEmitterResolved *emitter,
                                                              double dt) {
    SimRuntimeEmitterPlacement3D placement = {0};
    SimRuntimeEmitterOrientedBox3D box = {0};
    SimRuntimeEmitterResolved rotated = {0};
    const ImportedShape *imp = NULL;
    double world_radius = 0.0;
    double half_x = 0.0;
    double half_y = 0.0;
    double half_z = 0.0;
    int index = -1;
    if (!state || !scene || !emitter) return;
    if (!backend_3d_scaffold_resolve_emitter_placement(scene, &state->volume.desc, emitter, &placement)) {
        return;
    }
    index = emitter->attached_import;
    if (index < 0 || index >= (int)scene->import_shape_count) {
        world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
        backend_3d_scaffold_apply_sphere(
            state, scene, emitter, &placement, backend_3d_scaffold_sphere_world_volume(world_radius), dt);
        return;
    }
    imp = &scene->import_shapes[index];
    if (!imp->enabled) {
        world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
        backend_3d_scaffold_apply_sphere(
            state, scene, emitter, &placement, backend_3d_scaffold_sphere_world_volume(world_radius), dt);
        return;
    }

    rotated = *emitter;
    rotate_xy(&rotated.dir_x, &rotated.dir_y, imp->rotation_deg * (float)M_PI / 180.0f);
    if (!backend_3d_scaffold_build_import_box(&state->volume.desc, &placement, imp, &box)) {
        world_radius = backend_3d_scaffold_emitter_world_radius(&state->volume.desc, emitter->radius);
        backend_3d_scaffold_apply_sphere(state,
                                         scene,
                                         &rotated,
                                         &placement,
                                         backend_3d_scaffold_sphere_world_volume(world_radius),
                                         dt);
        return;
    }
    half_x = backend_3d_scaffold_import_world_half_extent_x(&state->volume.desc, imp);
    half_y = backend_3d_scaffold_import_world_half_extent_y(&state->volume.desc, imp);
    half_z = backend_3d_scaffold_import_world_half_extent_z(&state->volume.desc, imp);
    backend_3d_scaffold_apply_oriented_box(
        state, scene, &rotated, &box, backend_3d_scaffold_box_world_volume(half_x, half_y, half_z), dt);
}

void backend_3d_scaffold_apply_emitters(SimRuntimeBackend *backend,
                                        SceneState *scene,
                                        double dt) {
    SimRuntimeBackend3DScaffold *state = NULL;
    if (!backend || !scene || !scene->preset || dt <= 0.0) return;
    state = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!state) return;
    backend_3d_scaffold_reset_emitter_step_stats(state);
    if (!scene->emitters_enabled) return;

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

    state->debug_volume_stats_dirty = true;
    state->fluid_slice_dirty = true;
}
