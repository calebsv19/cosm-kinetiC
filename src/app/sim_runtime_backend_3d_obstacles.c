#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include <string.h>

static size_t backend_3d_scaffold_slice_offset(const SimRuntimeBackend3DScaffold *state) {
    if (!state) return 0;
    return (size_t)state->compatibility_slice_z * state->volume.desc.slice_cell_count;
}

static void backend_3d_scaffold_zero_obstacle_slice(SimRuntimeBackend3DScaffold *state) {
    size_t slice_cells = 0;
    if (!state) return;
    slice_cells = state->volume.desc.slice_cell_count;
    if (slice_cells == 0) return;
    if (state->slice_solid_mask) memset(state->slice_solid_mask, 0, slice_cells * sizeof(uint8_t));
    if (state->slice_obstacle_velocity_x) {
        memset(state->slice_obstacle_velocity_x, 0, slice_cells * sizeof(float));
    }
    if (state->slice_obstacle_velocity_y) {
        memset(state->slice_obstacle_velocity_y, 0, slice_cells * sizeof(float));
    }
    if (state->slice_obstacle_distance) {
        for (size_t i = 0; i < slice_cells; ++i) {
            state->slice_obstacle_distance[i] = 1.0f;
        }
    }
}

static void backend_3d_scaffold_mark_bounds_solid(SimRuntimeBackend3DScaffold *state,
                                                  const SimRuntimeObstacleBounds3D *bounds) {
    if (!state || !bounds || !state->obstacle_occupancy) return;
    for (int z = bounds->min_z; z <= bounds->max_z; ++z) {
        for (int y = bounds->min_y; y <= bounds->max_y; ++y) {
            for (int x = bounds->min_x; x <= bounds->max_x; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                state->obstacle_occupancy[idx] = 1u;
            }
        }
    }
}

static void backend_3d_scaffold_rebuild_obstacle_volume(SimRuntimeBackend3DScaffold *state,
                                                        const struct SceneState *scene) {
    SimRuntimeObstacleBounds3D bounds = {0};
    if (!state || !state->obstacle_occupancy || state->volume.desc.cell_count == 0) return;
    if (scene) {
        state->scene_ref = scene;
    } else {
        scene = state->scene_ref;
    }

    memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    for (int face = 0; face < SIM_RUNTIME_BOUNDARY_FACE_COUNT; ++face) {
        if (!state->obstacle_contract.domain_walls_enabled[face]) continue;
        if (!sim_runtime_obstacle_domain_face_bounds(&state->volume.desc,
                                                     (SimRuntimeBoundaryFace)face,
                                                     &bounds)) {
            continue;
        }
        backend_3d_scaffold_mark_bounds_solid(state, &bounds);
    }
    backend_3d_scaffold_rasterize_retained_object_obstacles(state, scene);
    backend_3d_scaffold_rasterize_retained_import_obstacles(state, scene);

    state->obstacle_volume_dirty = false;
    state->obstacle_slice_dirty = true;
}

static void backend_3d_scaffold_sync_obstacle_slice(SimRuntimeBackend3DScaffold *state) {
    const SimRuntime3DDomainDesc *desc = NULL;
    size_t slice_start = 0;
    float max_distance = 1.0f;
    if (!state) return;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_rebuild_obstacle_volume(state, NULL);
    }
    if (!state->obstacle_slice_dirty) return;
    if (!state->obstacle_occupancy ||
        !state->slice_solid_mask ||
        !state->slice_obstacle_velocity_x ||
        !state->slice_obstacle_velocity_y ||
        !state->slice_obstacle_distance ||
        state->compatibility_slice_z < 0 ||
        state->compatibility_slice_z >= desc->grid_d) {
        return;
    }

    backend_3d_scaffold_zero_obstacle_slice(state);
    slice_start = backend_3d_scaffold_slice_offset(state);
    memcpy(state->slice_solid_mask,
           state->obstacle_occupancy + slice_start,
           desc->slice_cell_count * sizeof(uint8_t));

    if (desc->grid_w > 2 && desc->grid_h > 2) {
        int shorter = (desc->grid_w < desc->grid_h) ? desc->grid_w : desc->grid_h;
        max_distance = (float)(shorter / 2);
        if (max_distance < 1.0f) max_distance = 1.0f;
    }

    for (int y = 0; y < desc->grid_h; ++y) {
        for (int x = 0; x < desc->grid_w; ++x) {
            size_t slice_idx = (size_t)y * (size_t)desc->grid_w + (size_t)x;
            if (state->slice_solid_mask[slice_idx]) {
                state->slice_obstacle_distance[slice_idx] = 0.0f;
                continue;
            }

            {
                int dist_x_min = x;
                int dist_x_max = (desc->grid_w - 1) - x;
                int dist_y_min = y;
                int dist_y_max = (desc->grid_h - 1) - y;
                int dist_cells = dist_x_min;
                if (dist_x_max < dist_cells) dist_cells = dist_x_max;
                if (dist_y_min < dist_cells) dist_cells = dist_y_min;
                if (dist_y_max < dist_cells) dist_cells = dist_y_max;
                if (dist_cells < 0) dist_cells = 0;
                state->slice_obstacle_distance[slice_idx] = (float)dist_cells / max_distance;
                if (state->slice_obstacle_distance[slice_idx] > 1.0f) {
                    state->slice_obstacle_distance[slice_idx] = 1.0f;
                }
            }
        }
    }

    state->obstacle_slice_dirty = false;
}

static void backend_3d_scaffold_apply_obstacle_enforcement(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_rebuild_obstacle_volume(state, NULL);
    }
    if (!state->obstacle_occupancy) return;

    for (size_t i = 0; i < state->volume.desc.cell_count; ++i) {
        if (!state->obstacle_occupancy[i]) continue;
        state->volume.density[i] = 0.0f;
        state->volume.velocity_x[i] = 0.0f;
        state->volume.velocity_y[i] = 0.0f;
        state->volume.velocity_z[i] = 0.0f;
        state->volume.pressure[i] = 0.0f;
    }

    state->fluid_slice_dirty = true;
    state->obstacle_slice_dirty = true;
}

void backend_3d_scaffold_reset_obstacles(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    if (state->obstacle_occupancy && state->volume.desc.cell_count > 0) {
        memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    backend_3d_scaffold_zero_obstacle_slice(state);
    state->obstacle_volume_dirty = true;
    state->obstacle_slice_dirty = true;
}

void backend_3d_scaffold_build_static_obstacles(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_build_obstacles(SimRuntimeBackend *backend,
                                         struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_mark_obstacles_dirty(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    state->obstacle_volume_dirty = true;
    state->obstacle_slice_dirty = true;
}

void backend_3d_scaffold_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                     struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    (void)scene;
    backend_3d_scaffold_apply_obstacle_enforcement(
        backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL);
}

void backend_3d_scaffold_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene) {
    (void)scene;
    backend_3d_scaffold_apply_obstacle_enforcement(
        backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL);
}

bool backend_3d_scaffold_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                              SceneObstacleFieldView2D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state || !out_view) return false;
    backend_3d_scaffold_sync_obstacle_slice(state);
    out_view->width = state->volume.desc.grid_w;
    out_view->height = state->volume.desc.grid_h;
    out_view->cell_count = state->volume.desc.slice_cell_count;
    out_view->solid_mask = state->slice_solid_mask;
    out_view->velocity_x = state->slice_obstacle_velocity_x;
    out_view->velocity_y = state->slice_obstacle_velocity_y;
    out_view->distance = state->slice_obstacle_distance;
    return true;
}
