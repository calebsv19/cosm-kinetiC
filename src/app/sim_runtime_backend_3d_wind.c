#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include "app/scene_state.h"

static void wind_face_vector(WindTunnel3DFace face, float speed, float *vx, float *vy, float *vz) {
    if (vx) *vx = 0.0f;
    if (vy) *vy = 0.0f;
    if (vz) *vz = 0.0f;
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
        if (vx) *vx = speed;
        break;
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (vx) *vx = -speed;
        break;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
        if (vy) *vy = speed;
        break;
    case WIND_TUNNEL_3D_FACE_TOP:
        if (vy) *vy = -speed;
        break;
    case WIND_TUNNEL_3D_FACE_FRONT:
        if (vz) *vz = speed;
        break;
    case WIND_TUNNEL_3D_FACE_BACK:
        if (vz) *vz = -speed;
        break;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        break;
    }
}

static void wind_face_outward_vector(WindTunnel3DFace face, float speed, float *vx, float *vy, float *vz) {
    wind_face_vector(face, -speed, vx, vy, vz);
}

static bool wind_face_cell_range(const SimRuntime3DDomainDesc *desc,
                                 WindTunnel3DFace face,
                                 int slab_cells,
                                 int *min_x,
                                 int *max_x,
                                 int *min_y,
                                 int *max_y,
                                 int *min_z,
                                 int *max_z) {
    int slab = slab_cells > 0 ? slab_cells : 1;
    if (!desc || !min_x || !max_x || !min_y || !max_y || !min_z || !max_z) return false;
    *min_x = 0;
    *max_x = desc->grid_w;
    *min_y = 0;
    *max_y = desc->grid_h;
    *min_z = 0;
    *max_z = desc->grid_d;
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
        if (slab > desc->grid_w) slab = desc->grid_w;
        *max_x = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (slab > desc->grid_w) slab = desc->grid_w;
        *min_x = desc->grid_w - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
        if (slab > desc->grid_h) slab = desc->grid_h;
        *max_y = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_TOP:
        if (slab > desc->grid_h) slab = desc->grid_h;
        *min_y = desc->grid_h - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_FRONT:
        if (slab > desc->grid_d) slab = desc->grid_d;
        *max_z = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_BACK:
        if (slab > desc->grid_d) slab = desc->grid_d;
        *min_z = desc->grid_d - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return false;
    }
}

static void wind_write_slab(SimRuntimeBackend3DScaffold *state,
                            WindTunnel3DFace face,
                            int slab_cells,
                            float density,
                            float velocity_x,
                            float velocity_y,
                            float velocity_z,
                            float pressure) {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    if (!state ||
        !wind_face_cell_range(&state->volume.desc,
                              face,
                              slab_cells,
                              &min_x,
                              &max_x,
                              &min_y,
                              &max_y,
                              &min_z,
                              &max_z)) {
        return;
    }
    for (int z = min_z; z < max_z; ++z) {
        for (int y = min_y; y < max_y; ++y) {
            for (int x = min_x; x < max_x; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                (void)sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          density,
                                                          velocity_x,
                                                          velocity_y,
                                                          velocity_z,
                                                          pressure);
                if (backend_3d_scaffold_dense_mirror_live(state)) {
                    state->volume.density[idx] = density;
                    state->volume.velocity_x[idx] = velocity_x;
                    state->volume.velocity_y[idx] = velocity_y;
                    state->volume.velocity_z[idx] = velocity_z;
                    state->volume.pressure[idx] = pressure;
                }
            }
        }
    }
}

static void wind_write_receive_outlet_slab(SimRuntimeBackend3DScaffold *state,
                                           WindTunnel3DFace face,
                                           int slab_cells,
                                           float velocity_x,
                                           float velocity_y,
                                           float velocity_z) {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    if (!state ||
        !wind_face_cell_range(&state->volume.desc,
                              face,
                              slab_cells,
                              &min_x,
                              &max_x,
                              &min_y,
                              &max_y,
                              &min_z,
                              &max_z)) {
        return;
    }
    for (int z = min_z; z < max_z; ++z) {
        for (int y = min_y; y < max_y; ++y) {
            for (int x = min_x; x < max_x; ++x) {
                float density = 0.0f;
                float pressure = 0.0f;
                int sample_x = x;
                int sample_y = y;
                int sample_z = z;
                size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                switch (face) {
                case WIND_TUNNEL_3D_FACE_RIGHT:
                    sample_x = x > 0 ? x - 1 : x;
                    break;
                case WIND_TUNNEL_3D_FACE_LEFT:
                    sample_x = x + 1 < state->volume.desc.grid_w ? x + 1 : x;
                    break;
                case WIND_TUNNEL_3D_FACE_TOP:
                    sample_y = y > 0 ? y - 1 : y;
                    break;
                case WIND_TUNNEL_3D_FACE_BOTTOM:
                    sample_y = y + 1 < state->volume.desc.grid_h ? y + 1 : y;
                    break;
                case WIND_TUNNEL_3D_FACE_BACK:
                    sample_z = z > 0 ? z - 1 : z;
                    break;
                case WIND_TUNNEL_3D_FACE_FRONT:
                    sample_z = z + 1 < state->volume.desc.grid_d ? z + 1 : z;
                    break;
                case WIND_TUNNEL_3D_FACE_NONE:
                default:
                    break;
                }
                (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                          sample_x,
                                                          sample_y,
                                                          sample_z,
                                                          &density,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          &pressure);
                if (backend_3d_scaffold_dense_mirror_live(state)) {
                    size_t sample_idx =
                        sim_runtime_3d_volume_index(&state->volume.desc, sample_x, sample_y, sample_z);
                    density = state->volume.density[sample_idx];
                    pressure = state->volume.pressure[sample_idx];
                }
                (void)sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          density,
                                                          velocity_x,
                                                          velocity_y,
                                                          velocity_z,
                                                          pressure);
                if (backend_3d_scaffold_dense_mirror_live(state)) {
                    state->volume.density[idx] = density;
                    state->volume.velocity_x[idx] = velocity_x;
                    state->volume.velocity_y[idx] = velocity_y;
                    state->volume.velocity_z[idx] = velocity_z;
                    state->volume.pressure[idx] = pressure;
                }
            }
        }
    }
}

static void wind_apply_corridor_velocity(SimRuntimeBackend3DScaffold *state,
                                         WindTunnel3DFace inlet_face,
                                         WindTunnel3DFace outlet_face,
                                         float velocity_x,
                                         float velocity_y,
                                         float velocity_z) {
    if (!state) return;
    switch (inlet_face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (outlet_face != WIND_TUNNEL_3D_FACE_LEFT &&
            outlet_face != WIND_TUNNEL_3D_FACE_RIGHT) return;
        break;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
    case WIND_TUNNEL_3D_FACE_TOP:
        if (outlet_face != WIND_TUNNEL_3D_FACE_BOTTOM &&
            outlet_face != WIND_TUNNEL_3D_FACE_TOP) return;
        break;
    case WIND_TUNNEL_3D_FACE_FRONT:
    case WIND_TUNNEL_3D_FACE_BACK:
        if (outlet_face != WIND_TUNNEL_3D_FACE_FRONT &&
            outlet_face != WIND_TUNNEL_3D_FACE_BACK) return;
        break;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return;
    }

    for (int z = 0; z < state->volume.desc.grid_d; ++z) {
        for (int y = 0; y < state->volume.desc.grid_h; ++y) {
            for (int x = 0; x < state->volume.desc.grid_w; ++x) {
                float density = 0.0f;
                float pressure = 0.0f;
                size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          &density,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          &pressure);
                (void)sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          density,
                                                          velocity_x,
                                                          velocity_y,
                                                          velocity_z,
                                                          pressure);
                if (backend_3d_scaffold_dense_mirror_live(state)) {
                    state->volume.velocity_x[idx] = velocity_x;
                    state->volume.velocity_y[idx] = velocity_y;
                    state->volume.velocity_z[idx] = velocity_z;
                }
            }
        }
    }
}

void backend_3d_scaffold_apply_wind_tunnel_boundary(SimRuntimeBackend3DScaffold *state,
                                                    const struct SceneState *scene) {
    const WindTunnel3DConfig *config = NULL;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    if (!state || !state->wind_tunnel_active) return;
    if (!scene || !scene->mode_route.wind_tunnel_3d_active) return;
    config = &state->wind_tunnel;
    if (!wind_tunnel_3d_config_validate(config)) return;

    wind_face_vector(config->inlet_face, config->inflow_speed, &vx, &vy, &vz);
    wind_apply_corridor_velocity(state,
                                 config->inlet_face,
                                 config->outlet_face,
                                 vx,
                                 vy,
                                 vz);
    wind_write_slab(state,
                    config->inlet_face,
                    config->inlet_slab_cells,
                    config->inflow_density,
                    vx,
                    vy,
                    vz,
                    0.0f);

    wind_face_outward_vector(config->outlet_face, config->inflow_speed, &vx, &vy, &vz);
    if (config->outlet_policy == WIND_TUNNEL_3D_OUTLET_CLEAR) {
        wind_write_slab(state,
                        config->outlet_face,
                        1,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f);
    } else {
        wind_write_receive_outlet_slab(state,
                                       config->outlet_face,
                                       1,
                                       vx,
                                       vy,
                                       vz);
    }
    backend_3d_scaffold_mark_fluid_dirty(state);
}
