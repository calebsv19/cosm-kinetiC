#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include "app/scene_state.h"

#include <math.h>

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

static void wind_write_cell_state(SimRuntimeBackend3DScaffold *state,
                                  int x,
                                  int y,
                                  int z,
                                  float density,
                                  float velocity_x,
                                  float velocity_y,
                                  float velocity_z,
                                  float pressure);

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

static void wind_apply_corridor_velocity_relaxed_x_axis(SimRuntimeBackend3DScaffold *state,
                                                        int direction,
                                                        float baseline_vx) {
    const SimRuntime3DDomainDesc *desc = state ? &state->volume.desc : NULL;
    const float axial_decay = 0.86f;
    const float cross_decay = 0.84f;
    const float pressure_decay = 0.82f;
    const float density_decay = 0.992f;
    const float speed_limit = fabsf(baseline_vx) * 1.15f;
    const float cross_limit = fabsf(baseline_vx) * 0.45f;
    if (!state || !desc || direction == 0) return;
    if (direction > 0) {
        for (int z = 0; z < desc->grid_d; ++z) {
            for (int y = 0; y < desc->grid_h; ++y) {
                for (int x = desc->grid_w - 1; x >= 0; --x) {
                    float src_density = 0.0f;
                    float src_vx = baseline_vx;
                    float src_vy = 0.0f;
                    float src_vz = 0.0f;
                    float src_pressure = 0.0f;
                    float next_vx = baseline_vx;
                    float next_vy = 0.0f;
                    float next_vz = 0.0f;
                    float next_pressure = 0.0f;
                    if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                    if (x > 0 && !backend_3d_scaffold_obstacle_cell_solid(state, x - 1, y, z)) {
                        (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                                  x - 1,
                                                                  y,
                                                                  z,
                                                                  &src_density,
                                                                  &src_vx,
                                                                  &src_vy,
                                                                  &src_vz,
                                                                  &src_pressure);
                    }
                    next_vx = baseline_vx + (src_vx - baseline_vx) * axial_decay;
                    next_vy = src_vy * cross_decay;
                    next_vz = src_vz * cross_decay;
                    next_pressure = src_pressure * pressure_decay;
                    if (next_vx > speed_limit) next_vx = speed_limit;
                    if (next_vx < 0.0f) next_vx = 0.0f;
                    if (next_vy > cross_limit) next_vy = cross_limit;
                    if (next_vy < -cross_limit) next_vy = -cross_limit;
                    if (next_vz > cross_limit) next_vz = cross_limit;
                    if (next_vz < -cross_limit) next_vz = -cross_limit;
                    wind_write_cell_state(state,
                                          x,
                                          y,
                                          z,
                                          src_density * density_decay,
                                          next_vx,
                                          next_vy,
                                          next_vz,
                                          next_pressure);
                }
            }
        }
    } else {
        for (int z = 0; z < desc->grid_d; ++z) {
            for (int y = 0; y < desc->grid_h; ++y) {
                for (int x = 0; x < desc->grid_w; ++x) {
                    float src_density = 0.0f;
                    float src_vx = baseline_vx;
                    float src_vy = 0.0f;
                    float src_vz = 0.0f;
                    float src_pressure = 0.0f;
                    float next_vx = baseline_vx;
                    float next_vy = 0.0f;
                    float next_vz = 0.0f;
                    float next_pressure = 0.0f;
                    if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                    if (x + 1 < desc->grid_w && !backend_3d_scaffold_obstacle_cell_solid(state, x + 1, y, z)) {
                        (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                                  x + 1,
                                                                  y,
                                                                  z,
                                                                  &src_density,
                                                                  &src_vx,
                                                                  &src_vy,
                                                                  &src_vz,
                                                                  &src_pressure);
                    }
                    next_vx = baseline_vx + (src_vx - baseline_vx) * axial_decay;
                    next_vy = src_vy * cross_decay;
                    next_vz = src_vz * cross_decay;
                    next_pressure = src_pressure * pressure_decay;
                    if (next_vx < -speed_limit) next_vx = -speed_limit;
                    if (next_vx > 0.0f) next_vx = 0.0f;
                    if (next_vy > cross_limit) next_vy = cross_limit;
                    if (next_vy < -cross_limit) next_vy = -cross_limit;
                    if (next_vz > cross_limit) next_vz = cross_limit;
                    if (next_vz < -cross_limit) next_vz = -cross_limit;
                    wind_write_cell_state(state,
                                          x,
                                          y,
                                          z,
                                          src_density * density_decay,
                                          next_vx,
                                          next_vy,
                                          next_vz,
                                          next_pressure);
                }
            }
        }
    }
}

static void wind_write_cell_state(SimRuntimeBackend3DScaffold *state,
                                  int x,
                                  int y,
                                  int z,
                                  float density,
                                  float velocity_x,
                                  float velocity_y,
                                  float velocity_z,
                                  float pressure) {
    const size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
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

static void wind_apply_x_axis_obstacle_wake(SimRuntimeBackend3DScaffold *state,
                                            int direction,
                                            float inflow_speed) {
    const SimRuntime3DDomainDesc *desc = state ? &state->volume.desc : NULL;
    const float phase = (float)(state ? state->wind_step_index : 0u) * 0.41f;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    bool found = false;
    int face_x = 0;
    float center_y = 0.0f;
    float center_z = 0.0f;
    float obstacle_radius = 0.0f;
    int wake_len = 0;
    int source_len = 0;
    int stagnation_len = 0;
    float blockage = 0.0f;
    float source_gain = 1.0f;
    if (!state || !desc || !state->obstacle_occupancy || !(inflow_speed > 0.0f)) return;

    for (int z = 2; z < desc->grid_d - 2; ++z) {
        for (int y = 2; y < desc->grid_h - 2; ++y) {
            for (int x = 2; x < desc->grid_w - 2; ++x) {
                if (!backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                if (!found) {
                    min_x = max_x = x;
                    min_y = max_y = y;
                    min_z = max_z = z;
                    found = true;
                } else {
                    if (x < min_x) min_x = x;
                    if (x > max_x) max_x = x;
                    if (y < min_y) min_y = y;
                    if (y > max_y) max_y = y;
                    if (z < min_z) min_z = z;
                    if (z > max_z) max_z = z;
                }
            }
        }
    }
    if (!found) return;

    face_x = direction > 0 ? max_x : min_x;
    center_y = ((float)min_y + (float)max_y) * 0.5f;
    center_z = ((float)min_z + (float)max_z) * 0.5f;
    obstacle_radius = 0.5f * (float)((max_y - min_y) > (max_z - min_z) ? (max_y - min_y) : (max_z - min_z));
    if (obstacle_radius < 2.0f) obstacle_radius = 2.0f;
    wake_len = desc->grid_w / 2 > 18 ? desc->grid_w / 2 : 18;
    source_len = (int)ceilf(obstacle_radius * 7.0f);
    if (source_len < 12) source_len = 12;
    if (source_len > 48) source_len = 48;
    if (source_len > wake_len) source_len = wake_len;
    stagnation_len = (int)ceilf(obstacle_radius * 1.25f);
    if (stagnation_len < 3) stagnation_len = 3;
    if (stagnation_len > 8) stagnation_len = 8;
    blockage = ((float)(max_y - min_y + 1) * (float)(max_z - min_z + 1)) /
               ((float)(desc->grid_h > 0 ? desc->grid_h : 1) *
                (float)(desc->grid_d > 0 ? desc->grid_d : 1));
    if (blockage < 0.02f) blockage = 0.02f;
    if (blockage > 0.45f) blockage = 0.45f;
    source_gain = 0.70f + blockage * 1.35f;

    for (int dx = 1; dx <= stagnation_len; ++dx) {
        const int x = (direction > 0 ? min_x : max_x) - direction * dx;
        const float upstream = (float)dx / (float)stagnation_len;
        const float radius = obstacle_radius + 1.25f + upstream * obstacle_radius * 0.55f;
        const int iradius = (int)ceilf(radius);
        if (x <= 1 || x >= desc->grid_w - 2) break;
        for (int z = (int)floorf(center_z) - iradius; z <= (int)ceilf(center_z) + iradius; ++z) {
            for (int y = (int)floorf(center_y) - iradius; y <= (int)ceilf(center_y) + iradius; ++y) {
                const float dy = (float)y - center_y;
                const float dz = (float)z - center_z;
                const float r = sqrtf(dy * dy + dz * dz);
                const float radial = radius > 0.0f ? r / radius : 1.0f;
                float density = 0.0f;
                float vx = 0.0f;
                float vy = 0.0f;
                float vz = 0.0f;
                float pressure = 0.0f;
                float envelope = 0.0f;
                float stagnation = 0.0f;
                if (y <= 1 || z <= 1 || y >= desc->grid_h - 2 || z >= desc->grid_d - 2) continue;
                if (radial > 1.0f) continue;
                if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          &density,
                                                          &vx,
                                                          &vy,
                                                          &vz,
                                                          &pressure);
                envelope = expf(-upstream * 1.35f) * (1.0f - radial * radial);
                if (envelope < 0.0f) envelope = 0.0f;
                stagnation = inflow_speed * 0.070f * source_gain * envelope;
                pressure += stagnation;
                if (direction > 0) {
                    vx = fmaxf(0.0f, vx - inflow_speed * 0.16f * envelope);
                } else {
                    vx = fminf(0.0f, vx + inflow_speed * 0.16f * envelope);
                }
                if (pressure > inflow_speed * 0.16f) pressure = inflow_speed * 0.16f;
                if (pressure < -inflow_speed * 0.16f) pressure = -inflow_speed * 0.16f;
                wind_write_cell_state(state, x, y, z, density, vx, vy, vz, pressure);
            }
        }
    }

    for (int dx = 1; dx <= source_len; ++dx) {
        const int x = face_x + direction * dx;
        const float downstream = (float)dx / (float)source_len;
        const float radius = obstacle_radius + 2.0f + downstream * (float)(desc->grid_h < desc->grid_d ? desc->grid_h : desc->grid_d) * 0.16f;
        const int iradius = (int)ceilf(radius);
        if (x <= 1 || x >= desc->grid_w - 2) break;
        for (int z = (int)floorf(center_z) - iradius; z <= (int)ceilf(center_z) + iradius; ++z) {
            for (int y = (int)floorf(center_y) - iradius; y <= (int)ceilf(center_y) + iradius; ++y) {
                const float dy = (float)y - center_y;
                const float dz = (float)z - center_z;
                const float r = sqrtf(dy * dy + dz * dz);
                const float radial = radius > 0.0f ? r / radius : 1.0f;
                float density = 0.0f;
                float vx = 0.0f;
                float vy = 0.0f;
                float vz = 0.0f;
                float pressure = 0.0f;
                float envelope = 0.0f;
                float deficit = 0.0f;
                float shed = 0.0f;
                float cross = 0.0f;
                float suction = 0.0f;
                float lateral_mode = 0.0f;
                float inv_r = 0.0f;
                if (y <= 1 || z <= 1 || y >= desc->grid_h - 2 || z >= desc->grid_d - 2) continue;
                if (radial > 1.0f) continue;
                if (backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                (void)sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                          x,
                                                          y,
                                                          z,
                                                          &density,
                                                          &vx,
                                                          &vy,
                                                          &vz,
                                                          &pressure);
                envelope = expf(-downstream * 0.62f) * (1.0f - radial * radial);
                if (envelope < 0.0f) envelope = 0.0f;
                envelope *= envelope;
                shed = sinf(phase + (float)dx * 0.74f);
                lateral_mode = cosf((dy / obstacle_radius) * 1.35f) *
                               sinf((dz / obstacle_radius) * 1.15f);
                deficit = inflow_speed * (0.66f * source_gain * envelope);
                cross = inflow_speed * 0.42f * source_gain * envelope * shed;
                suction = inflow_speed * 0.115f * source_gain * envelope;
                inv_r = r > 0.001f ? 1.0f / r : 0.0f;
                if (direction > 0) {
                    vx = fmaxf(0.0f, vx - deficit);
                    if (vx > inflow_speed) vx = inflow_speed;
                } else {
                    vx = fminf(0.0f, vx + deficit);
                    if (vx < -inflow_speed) vx = -inflow_speed;
                }
                vy += (-dz * inv_r * cross) + (dy / obstacle_radius) * cross * 0.24f;
                vz += (dy * inv_r * cross) - (dz / obstacle_radius) * cross * 0.24f;
                if (vy > inflow_speed * 0.45f) vy = inflow_speed * 0.45f;
                if (vy < -inflow_speed * 0.45f) vy = -inflow_speed * 0.45f;
                if (vz > inflow_speed * 0.45f) vz = inflow_speed * 0.45f;
                if (vz < -inflow_speed * 0.45f) vz = -inflow_speed * 0.45f;
                pressure -= suction;
                pressure += inflow_speed * 0.030f * source_gain * envelope * shed * lateral_mode;
                if (pressure > inflow_speed * 0.16f) pressure = inflow_speed * 0.16f;
                if (pressure < -inflow_speed * 0.16f) pressure = -inflow_speed * 0.16f;
                if (density > 1.0f) density = 1.0f;
                wind_write_cell_state(state, x, y, z, density, vx, vy, vz, pressure);
            }
        }
    }
}

static void wind_apply_obstacle_wake_relaxation(SimRuntimeBackend3DScaffold *state,
                                                const WindTunnel3DConfig *config) {
    int direction = 0;
    if (!state || !config || !(config->inflow_speed > 0.0f)) return;
    if (config->inlet_face == WIND_TUNNEL_3D_FACE_LEFT &&
        config->outlet_face == WIND_TUNNEL_3D_FACE_RIGHT) {
        direction = 1;
    } else if (config->inlet_face == WIND_TUNNEL_3D_FACE_RIGHT &&
               config->outlet_face == WIND_TUNNEL_3D_FACE_LEFT) {
        direction = -1;
    }
    if (direction == 0) return;
    wind_apply_x_axis_obstacle_wake(state, direction, config->inflow_speed);
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
    if (config->inlet_face == WIND_TUNNEL_3D_FACE_LEFT &&
        config->outlet_face == WIND_TUNNEL_3D_FACE_RIGHT) {
        wind_apply_corridor_velocity_relaxed_x_axis(state, 1, vx);
    } else if (config->inlet_face == WIND_TUNNEL_3D_FACE_RIGHT &&
               config->outlet_face == WIND_TUNNEL_3D_FACE_LEFT) {
        wind_apply_corridor_velocity_relaxed_x_axis(state, -1, vx);
    } else {
        wind_apply_corridor_velocity(state,
                                     config->inlet_face,
                                     config->outlet_face,
                                     vx,
                                     vy,
                                     vz);
    }
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
    wind_apply_obstacle_wake_relaxation(state, config);
    state->wind_step_index++;
    backend_3d_scaffold_mark_fluid_dirty(state);
}
