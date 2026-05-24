#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_backend_3d_emitter_shapes.h"

#include "app/scene_state.h"
#include "app/sim_runtime_emitter.h"

#include <math.h>

static const float SCAFFOLD_EMITTER_POWER_BOOST = 40.0f;
static const float SCAFFOLD_REF_FOOTPRINT_CELLS = 128.0f;
static const double SCAFFOLD_IMPORT_DESIRED_FIT = 0.25;
static const float SCAFFOLD_DENSITY_DIRECTIONAL_SCALE_LEGACY = 0.25f;
static const float SCAFFOLD_DENSITY_DIRECTIONAL_SCALE_SURFACE = 1.0f;
static const float SCAFFOLD_THERMAL_BUOYANCY_EMIT_SCALE = 0.12f;
static const float SCAFFOLD_HEATED_OBSTACLE_DENSITY_SCALE = 0.10f;
static const float SCAFFOLD_HEATED_OBSTACLE_DIRECTIONAL_SCALE = 0.15f;
static const float SCAFFOLD_HEATED_OBSTACLE_THERMAL_SCALE = 2.5f;

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

static float backend_3d_scaffold_density_directional_scale(
    const SimRuntimeEmitterResolved *emitter) {
    if (!emitter) return SCAFFOLD_DENSITY_DIRECTIONAL_SCALE_LEGACY;
    switch (emitter->source_mode_3d) {
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return SCAFFOLD_DENSITY_DIRECTIONAL_SCALE_SURFACE;
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
    default:
        break;
    }
    return SCAFFOLD_DENSITY_DIRECTIONAL_SCALE_LEGACY;
}

static bool backend_3d_scaffold_resolve_buoyancy_axis(
    const SimRuntimeBackend3DScaffold *state,
    const SimRuntimeEmitterResolved *emitter,
    float *out_x,
    float *out_y,
    float *out_z) {
    float axis_x = 0.0f;
    float axis_y = 0.0f;
    float axis_z = 0.0f;
    float axis_len = 0.0f;
    if (!out_x || !out_y || !out_z) return false;
    if (state && state->scene_up_valid) {
        axis_x = state->scene_up_x;
        axis_y = state->scene_up_y;
        axis_z = state->scene_up_z;
    } else if (emitter && emitter->direction_has_magnitude) {
        axis_x = emitter->dir_x;
        axis_y = emitter->dir_y;
        axis_z = emitter->dir_z;
    } else {
        axis_z = 1.0f;
    }
    axis_len = sqrtf(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
    if (!(axis_len > 0.0001f)) return false;
    *out_x = axis_x / axis_len;
    *out_y = axis_y / axis_len;
    *out_z = axis_z / axis_len;
    return true;
}

static float backend_3d_scaffold_thermal_buoyancy_delta(
    const SimRuntimeEmitterResolved *emitter,
    float per_cell) {
    float mode_scale = 1.0f;
    if (!emitter || per_cell <= 0.0f || emitter->thermal_buoyancy_3d <= 0.0f) return 0.0f;
    if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
        mode_scale = SCAFFOLD_HEATED_OBSTACLE_THERMAL_SCALE;
    }
    return per_cell * emitter->thermal_buoyancy_3d * SCAFFOLD_THERMAL_BUOYANCY_EMIT_SCALE *
           mode_scale;
}

static bool backend_3d_scaffold_cell_in_heated_release_band(
    const SimRuntimeBackend3DScaffold *state,
    const SimRuntimeEmitterResolved *emitter,
    const SimRuntimeEmitterOrientedBox3D *box,
    int x,
    int y,
    int z) {
    float axis_x = 0.0f;
    float axis_y = 0.0f;
    float axis_z = 0.0f;
    float signed_distance = 0.0f;
    if (!box) return false;
    if (!state || !emitter) return true;
    if (emitter->surface_3d != EMITTER_3D_SURFACE_ALL_FACES &&
        emitter->surface_3d != EMITTER_3D_SURFACE_AUTO) {
        return true;
    }
    if (!backend_3d_scaffold_resolve_buoyancy_axis(state,
                                                   emitter,
                                                   &axis_x,
                                                   &axis_y,
                                                   &axis_z)) {
        return true;
    }
    signed_distance = ((float)x - (float)box->center_x) * axis_x +
                      ((float)y - (float)box->center_y) * axis_y +
                      ((float)z - (float)box->center_z) * axis_z;
    return signed_distance >= 0.0f;
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
    float directional_scale = 0.0f;
    float thermal_delta = 0.0f;
    float per_cell = 0.0f;
    if (!state || !emitter || affected_cells == 0 || total_strength <= 0.0f) return;
    per_cell = total_strength / (float)affected_cells;
    directional_scale = backend_3d_scaffold_density_directional_scale(emitter);
    thermal_delta = backend_3d_scaffold_thermal_buoyancy_delta(emitter, per_cell) * (float)affected_cells;

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
        if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
            density_delta = total_strength * SCAFFOLD_HEATED_OBSTACLE_DENSITY_SCALE;
            velocity_delta = total_strength * SCAFFOLD_HEATED_OBSTACLE_DIRECTIONAL_SCALE;
            velocity_delta += thermal_delta;
        } else {
            density_delta = total_strength;
            velocity_delta = emitter->direction_has_magnitude ? total_strength * directional_scale : 0.0f;
            velocity_delta += thermal_delta;
        }
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

static size_t backend_3d_scaffold_count_surface_patch_cells(
    const SimRuntimeEmitterOrientedBox3D *box,
    FluidEmitter3DSurface surface) {
    size_t cells = 0;
    if (!box) return 0;
    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                if (!backend_3d_scaffold_cell_in_surface_patch(box, surface, x, y, z)) continue;
                ++cells;
            }
        }
    }
    return cells;
}

static size_t backend_3d_scaffold_count_surface_shell_cells(
    const SimRuntimeBackend3DScaffold *state,
    const SimRuntimeEmitterResolved *emitter,
    const SimRuntimeEmitterOrientedBox3D *box,
    FluidEmitter3DSurface surface) {
    size_t cells = 0;
    if (!box) return 0;
    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                bool in_shell = false;
                if (emitter && emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
                    in_shell = backend_3d_scaffold_cell_in_surface_shell_exterior(box, surface, x, y, z);
                    if (in_shell &&
                        !backend_3d_scaffold_cell_in_heated_release_band(state, emitter, box, x, y, z)) {
                        in_shell = false;
                    }
                } else {
                    in_shell = backend_3d_scaffold_cell_in_surface_shell(box, surface, x, y, z);
                }
                if (!in_shell) continue;
                ++cells;
            }
        }
    }
    return cells;
}

static void backend_3d_scaffold_apply_cell(SimRuntimeBackend3DScaffold *state,
                                           const SimRuntimeEmitterResolved *emitter,
                                           int x,
                                           int y,
                                           int z,
                                           float per_cell) {
    float density_delta = 0.0f;
    float velocity_x_delta = 0.0f;
    float velocity_y_delta = 0.0f;
    float velocity_z_delta = 0.0f;
    float directional_scale = 0.0f;
    float buoyancy_axis_x = 0.0f;
    float buoyancy_axis_y = 0.0f;
    float buoyancy_axis_z = 0.0f;
    float thermal_delta = 0.0f;
    size_t idx = 0;
    if (!state || !emitter || per_cell <= 0.0f) return;
    directional_scale = backend_3d_scaffold_density_directional_scale(emitter);
    thermal_delta = backend_3d_scaffold_thermal_buoyancy_delta(emitter, per_cell);
    if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
        density_delta = per_cell * SCAFFOLD_HEATED_OBSTACLE_DENSITY_SCALE;
        if (emitter->direction_has_magnitude) {
            velocity_x_delta = emitter->dir_x * per_cell * SCAFFOLD_HEATED_OBSTACLE_DIRECTIONAL_SCALE;
            velocity_y_delta = emitter->dir_y * per_cell * SCAFFOLD_HEATED_OBSTACLE_DIRECTIONAL_SCALE;
            velocity_z_delta = emitter->dir_z * per_cell * SCAFFOLD_HEATED_OBSTACLE_DIRECTIONAL_SCALE;
        }
        if (thermal_delta > 0.0f &&
            backend_3d_scaffold_resolve_buoyancy_axis(state,
                                                      emitter,
                                                      &buoyancy_axis_x,
                                                      &buoyancy_axis_y,
                                                      &buoyancy_axis_z)) {
            velocity_x_delta = buoyancy_axis_x * thermal_delta;
            velocity_y_delta = buoyancy_axis_y * thermal_delta;
            velocity_z_delta = buoyancy_axis_z * thermal_delta;
        }
        sim_runtime_3d_brick_store_add_cell(&state->brick_store,
                                            x,
                                            y,
                                            z,
                                            density_delta,
                                            velocity_x_delta,
                                            velocity_y_delta,
                                            velocity_z_delta,
                                            0.0f);
        if (!backend_3d_scaffold_dense_mirror_live(state)) return;
        idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
        state->volume.density[idx] += density_delta;
        state->volume.velocity_x[idx] += velocity_x_delta;
        state->volume.velocity_y[idx] += velocity_y_delta;
        state->volume.velocity_z[idx] += velocity_z_delta;
        return;
    }
    switch (emitter->type) {
    case EMITTER_DENSITY_SOURCE:
        density_delta = per_cell;
        if (emitter->direction_has_magnitude) {
            velocity_x_delta = emitter->dir_x * per_cell * directional_scale;
            velocity_y_delta = emitter->dir_y * per_cell * directional_scale;
            velocity_z_delta = emitter->dir_z * per_cell * directional_scale;
        }
        if (thermal_delta > 0.0f &&
            backend_3d_scaffold_resolve_buoyancy_axis(state,
                                                      emitter,
                                                      &buoyancy_axis_x,
                                                      &buoyancy_axis_y,
                                                      &buoyancy_axis_z)) {
            velocity_x_delta += buoyancy_axis_x * thermal_delta;
            velocity_y_delta += buoyancy_axis_y * thermal_delta;
            velocity_z_delta += buoyancy_axis_z * thermal_delta;
        }
        break;
    case EMITTER_VELOCITY_JET:
        velocity_x_delta = emitter->dir_x * per_cell;
        velocity_y_delta = emitter->dir_y * per_cell;
        velocity_z_delta = emitter->dir_z * per_cell;
        density_delta = per_cell * 0.3f;
        break;
    case EMITTER_SINK:
        density_delta = -per_cell * 0.5f;
        velocity_x_delta = -emitter->dir_x * per_cell * 0.4f;
        velocity_y_delta = -emitter->dir_y * per_cell * 0.4f;
        velocity_z_delta = -emitter->dir_z * per_cell * 0.4f;
        break;
    }
    sim_runtime_3d_brick_store_add_cell(&state->brick_store,
                                        x,
                                        y,
                                        z,
                                        density_delta,
                                        velocity_x_delta,
                                        velocity_y_delta,
                                        velocity_z_delta,
                                        0.0f);
    if (!backend_3d_scaffold_dense_mirror_live(state)) return;
    idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
    state->volume.density[idx] += density_delta;
    state->volume.velocity_x[idx] += velocity_x_delta;
    state->volume.velocity_y[idx] += velocity_y_delta;
    state->volume.velocity_z[idx] += velocity_z_delta;
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
                if (!backend_3d_scaffold_cell_in_sphere(placement, x, y, z)) continue;
                backend_3d_scaffold_apply_cell(state, emitter, x, y, z, per_cell);
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
                if (!backend_3d_scaffold_cell_in_oriented_box(box, x, y, z)) continue;
                backend_3d_scaffold_apply_cell(state, emitter, x, y, z, per_cell);
            }
        }
    }
}

static void backend_3d_scaffold_apply_surface_patch(SimRuntimeBackend3DScaffold *state,
                                                    const SceneState *scene,
                                                    const SimRuntimeEmitterResolved *emitter,
                                                    const SimRuntimeEmitterOrientedBox3D *box,
                                                    double full_box_world_volume,
                                                    double dt) {
    size_t full_cells = 0;
    size_t patch_cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    double patch_world_volume = 0.0;
    FluidEmitter3DSurface surface = EMITTER_3D_SURFACE_TOP;
    if (!state || !scene || !emitter || !box) return;
    surface = backend_3d_scaffold_resolve_surface_3d(emitter->surface_3d);
    full_cells = backend_3d_scaffold_count_oriented_box_cells(box);
    patch_cells = backend_3d_scaffold_count_surface_patch_cells(box, surface);
    if (full_cells == 0 || patch_cells == 0) return;
    patch_world_volume = full_box_world_volume * ((double)patch_cells / (double)full_cells);
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                &state->volume.desc,
                                                                emitter,
                                                                (float)dt,
                                                                patch_cells,
                                                                patch_world_volume);
    per_cell = total_strength / (float)patch_cells;
    if (per_cell <= 0.0f) return;
    backend_3d_scaffold_accumulate_emitter_step_stats(state, emitter, patch_cells, total_strength);

    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                if (!backend_3d_scaffold_cell_in_surface_patch(box, surface, x, y, z)) continue;
                backend_3d_scaffold_apply_cell(state, emitter, x, y, z, per_cell);
            }
        }
    }
}

static void backend_3d_scaffold_apply_surface_shell(SimRuntimeBackend3DScaffold *state,
                                                    const SceneState *scene,
                                                    const SimRuntimeEmitterResolved *emitter,
                                                    const SimRuntimeEmitterOrientedBox3D *box,
                                                    double full_box_world_volume,
                                                    double dt) {
    size_t full_cells = 0;
    size_t shell_cells = 0;
    float total_strength = 0.0f;
    float per_cell = 0.0f;
    double shell_world_volume = 0.0;
    FluidEmitter3DSurface surface = EMITTER_3D_SURFACE_TOP;
    if (!state || !scene || !emitter || !box) return;
    surface = backend_3d_scaffold_resolve_surface_3d(emitter->surface_3d);
    full_cells = backend_3d_scaffold_count_oriented_box_cells(box);
    shell_cells = backend_3d_scaffold_count_surface_shell_cells(state, emitter, box, surface);
    if (full_cells == 0 || shell_cells == 0) return;
    shell_world_volume = full_box_world_volume * ((double)shell_cells / (double)full_cells);
    total_strength = backend_3d_scaffold_emitter_total_strength(scene,
                                                                &state->volume.desc,
                                                                emitter,
                                                                (float)dt,
                                                                shell_cells,
                                                                shell_world_volume);
    per_cell = total_strength / (float)shell_cells;
    if (per_cell <= 0.0f) return;
    backend_3d_scaffold_accumulate_emitter_step_stats(state, emitter, shell_cells, total_strength);

    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                bool in_shell = false;
                if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
                    in_shell = backend_3d_scaffold_cell_in_surface_shell_exterior(box, surface, x, y, z);
                    if (in_shell &&
                        !backend_3d_scaffold_cell_in_heated_release_band(state, emitter, box, x, y, z)) {
                        in_shell = false;
                    }
                } else {
                    in_shell = backend_3d_scaffold_cell_in_surface_shell(box, surface, x, y, z);
                }
                if (!in_shell) continue;
                backend_3d_scaffold_apply_cell(state, emitter, x, y, z, per_cell);
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
    if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_PATCH) {
        backend_3d_scaffold_apply_surface_patch(
            state,
            scene,
            emitter,
            &box,
            backend_3d_scaffold_box_world_volume(half_x, half_y, half_z),
            dt);
        return;
    }
    if (emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_SHELL ||
        emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
        backend_3d_scaffold_apply_surface_shell(
            state,
            scene,
            emitter,
            &box,
            backend_3d_scaffold_box_world_volume(half_x, half_y, half_z),
            dt);
        return;
    }
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
    if (rotated.source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_PATCH) {
        backend_3d_scaffold_apply_surface_patch(
            state,
            scene,
            &rotated,
            &box,
            backend_3d_scaffold_box_world_volume(half_x, half_y, half_z),
            dt);
        return;
    }
    if (rotated.source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_SHELL ||
        rotated.source_mode_3d == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
        backend_3d_scaffold_apply_surface_shell(
            state,
            scene,
            &rotated,
            &box,
            backend_3d_scaffold_box_world_volume(half_x, half_y, half_z),
            dt);
        return;
    }
    backend_3d_scaffold_apply_oriented_box(
        state, scene, &rotated, &box, backend_3d_scaffold_box_world_volume(half_x, half_y, half_z), dt);
}

void backend_3d_scaffold_apply_emitters(SimRuntimeBackend *backend,
                                        SceneState *scene,
                                        double dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    SimRuntimeBackend3DScaffold *state = NULL;
    double zero_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0;
    if (!backend || !scene || !scene->preset || dt <= zero_seconds) return;
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
    state->export_volume_cache_dirty = true;
    state->fluid_slice_dirty = true;
}
