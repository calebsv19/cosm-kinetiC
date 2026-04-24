#include "app/sim_runtime_3d_solver.h"

#include <math.h>
#include <string.h>

static float clamp_float_value(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool cell_is_solid(const uint8_t *solid_mask, size_t idx) {
    return solid_mask && solid_mask[idx] != 0u;
}

static float sample_field_trilinear_masked(const float *field,
                                           const SimRuntime3DDomainDesc *desc,
                                           const uint8_t *solid_mask,
                                           float x,
                                           float y,
                                           float z) {
    int x0 = 0;
    int y0 = 0;
    int z0 = 0;
    int x1 = 0;
    int y1 = 0;
    int z1 = 0;
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    float c000 = 0.0f;
    float c100 = 0.0f;
    float c010 = 0.0f;
    float c110 = 0.0f;
    float c001 = 0.0f;
    float c101 = 0.0f;
    float c011 = 0.0f;
    float c111 = 0.0f;
    float c00 = 0.0f;
    float c10 = 0.0f;
    float c01 = 0.0f;
    float c11 = 0.0f;
    float c0 = 0.0f;
    float c1 = 0.0f;

    if (!field || !desc || desc->grid_w <= 0 || desc->grid_h <= 0 || desc->grid_d <= 0) {
        return 0.0f;
    }

    x = clamp_float_value(x, 0.0f, (float)(desc->grid_w - 1));
    y = clamp_float_value(y, 0.0f, (float)(desc->grid_h - 1));
    z = clamp_float_value(z, 0.0f, (float)(desc->grid_d - 1));

    x0 = (int)floorf(x);
    y0 = (int)floorf(y);
    z0 = (int)floorf(z);
    x1 = x0 + 1 < desc->grid_w ? x0 + 1 : x0;
    y1 = y0 + 1 < desc->grid_h ? y0 + 1 : y0;
    z1 = z0 + 1 < desc->grid_d ? z0 + 1 : z0;

    tx = x - (float)x0;
    ty = y - (float)y0;
    tz = z - (float)z0;

    {
        size_t i000 = sim_runtime_3d_volume_index(desc, x0, y0, z0);
        size_t i100 = sim_runtime_3d_volume_index(desc, x1, y0, z0);
        size_t i010 = sim_runtime_3d_volume_index(desc, x0, y1, z0);
        size_t i110 = sim_runtime_3d_volume_index(desc, x1, y1, z0);
        size_t i001 = sim_runtime_3d_volume_index(desc, x0, y0, z1);
        size_t i101 = sim_runtime_3d_volume_index(desc, x1, y0, z1);
        size_t i011 = sim_runtime_3d_volume_index(desc, x0, y1, z1);
        size_t i111 = sim_runtime_3d_volume_index(desc, x1, y1, z1);

        c000 = cell_is_solid(solid_mask, i000) ? 0.0f : field[i000];
        c100 = cell_is_solid(solid_mask, i100) ? 0.0f : field[i100];
        c010 = cell_is_solid(solid_mask, i010) ? 0.0f : field[i010];
        c110 = cell_is_solid(solid_mask, i110) ? 0.0f : field[i110];
        c001 = cell_is_solid(solid_mask, i001) ? 0.0f : field[i001];
        c101 = cell_is_solid(solid_mask, i101) ? 0.0f : field[i101];
        c011 = cell_is_solid(solid_mask, i011) ? 0.0f : field[i011];
        c111 = cell_is_solid(solid_mask, i111) ? 0.0f : field[i111];
    }

    c00 = c000 * (1.0f - tx) + c100 * tx;
    c10 = c010 * (1.0f - tx) + c110 * tx;
    c01 = c001 * (1.0f - tx) + c101 * tx;
    c11 = c011 * (1.0f - tx) + c111 * tx;
    c0 = c00 * (1.0f - ty) + c10 * ty;
    c1 = c01 * (1.0f - ty) + c11 * ty;
    return c0 * (1.0f - tz) + c1 * tz;
}

static void apply_solid_mask(SimRuntime3DVolume *volume, const uint8_t *solid_mask) {
    if (!volume || !solid_mask) return;
    for (size_t i = 0; i < volume->desc.cell_count; ++i) {
        if (!solid_mask[i]) continue;
        volume->density[i] = 0.0f;
        volume->velocity_x[i] = 0.0f;
        volume->velocity_y[i] = 0.0f;
        volume->velocity_z[i] = 0.0f;
        volume->pressure[i] = 0.0f;
    }
}

static void diffuse_scalar_field(float *field,
                                 const float *source,
                                 const SimRuntime3DDomainDesc *desc,
                                 const uint8_t *solid_mask,
                                 float blend) {
    if (!field || !source || !desc || blend <= 0.0f) return;

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                float center = source[idx];
                float neighbor_sum = 0.0f;
                float neighbor_avg = 0.0f;

                if (cell_is_solid(solid_mask, idx)) {
                    field[idx] = 0.0f;
                    continue;
                }

                {
                    size_t n0 = sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z);
                    size_t n1 = sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z);
                    size_t n2 = sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z);
                    size_t n3 = sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z);
                    size_t n4 = sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1);
                    size_t n5 = sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1);
                    neighbor_sum += cell_is_solid(solid_mask, n0) ? 0.0f : source[n0];
                    neighbor_sum += cell_is_solid(solid_mask, n1) ? 0.0f : source[n1];
                    neighbor_sum += cell_is_solid(solid_mask, n2) ? 0.0f : source[n2];
                    neighbor_sum += cell_is_solid(solid_mask, n3) ? 0.0f : source[n3];
                    neighbor_sum += cell_is_solid(solid_mask, n4) ? 0.0f : source[n4];
                    neighbor_sum += cell_is_solid(solid_mask, n5) ? 0.0f : source[n5];
                }
                neighbor_avg = neighbor_sum / 6.0f;
                field[idx] = center * (1.0f - blend) + neighbor_avg * blend;
            }
        }
    }
}

static void advect_velocity(SimRuntime3DVolume *volume,
                            const SimRuntime3DSolverScratch *scratch,
                            const uint8_t *solid_mask,
                            float dt_cells) {
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!volume || !scratch) return;
    desc = &volume->desc;

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                float prev_vx = scratch->velocity_x_prev[idx];
                float prev_vy = scratch->velocity_y_prev[idx];
                float prev_vz = scratch->velocity_z_prev[idx];
                float sample_x = (float)x - prev_vx * dt_cells;
                float sample_y = (float)y - prev_vy * dt_cells;
                float sample_z = (float)z - prev_vz * dt_cells;

                if (cell_is_solid(solid_mask, idx)) {
                    volume->velocity_x[idx] = 0.0f;
                    volume->velocity_y[idx] = 0.0f;
                    volume->velocity_z[idx] = 0.0f;
                    continue;
                }

                volume->velocity_x[idx] = sample_field_trilinear_masked(
                    scratch->velocity_x_prev, desc, solid_mask, sample_x, sample_y, sample_z);
                volume->velocity_y[idx] = sample_field_trilinear_masked(
                    scratch->velocity_y_prev, desc, solid_mask, sample_x, sample_y, sample_z);
                volume->velocity_z[idx] = sample_field_trilinear_masked(
                    scratch->velocity_z_prev, desc, solid_mask, sample_x, sample_y, sample_z);
            }
        }
    }
}

static void diffuse_velocity_fields(SimRuntime3DVolume *volume,
                                    SimRuntime3DSolverScratch *scratch,
                                    const uint8_t *solid_mask,
                                    float viscosity_blend) {
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!volume || !scratch || viscosity_blend <= 0.0f) return;
    desc = &volume->desc;

    memcpy(scratch->velocity_x_prev, volume->velocity_x, desc->cell_count * sizeof(float));
    memcpy(scratch->velocity_y_prev, volume->velocity_y, desc->cell_count * sizeof(float));
    memcpy(scratch->velocity_z_prev, volume->velocity_z, desc->cell_count * sizeof(float));

    diffuse_scalar_field(volume->velocity_x,
                         scratch->velocity_x_prev,
                         desc,
                         solid_mask,
                         viscosity_blend);
    diffuse_scalar_field(volume->velocity_y,
                         scratch->velocity_y_prev,
                         desc,
                         solid_mask,
                         viscosity_blend);
    diffuse_scalar_field(volume->velocity_z,
                         scratch->velocity_z_prev,
                         desc,
                         solid_mask,
                         viscosity_blend);
}

static void apply_buoyancy(SimRuntime3DVolume *volume,
                           const SimRuntime3DSolverScratch *scratch,
                           const uint8_t *solid_mask,
                           const SimRuntime3DForceAxis *scene_up_axis,
                           float buoyancy_force,
                           float dt) {
    float axis_x = 0.0f;
    float axis_y = -1.0f;
    float axis_z = 0.0f;
    float axis_len = 0.0f;
    if (!volume || !scratch || buoyancy_force == 0.0f || dt <= 0.0f) return;
    if (scene_up_axis && scene_up_axis->valid) {
        axis_x = scene_up_axis->x;
        axis_y = scene_up_axis->y;
        axis_z = scene_up_axis->z;
        axis_len = sqrtf(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
        if (axis_len > 0.0001f) {
            axis_x /= axis_len;
            axis_y /= axis_len;
            axis_z /= axis_len;
        } else {
            axis_x = 0.0f;
            axis_y = -1.0f;
            axis_z = 0.0f;
        }
    }
    for (size_t i = 0; i < volume->desc.cell_count; ++i) {
        float impulse = 0.0f;
        if (cell_is_solid(solid_mask, i)) continue;
        impulse = scratch->density_prev[i] * buoyancy_force * dt;
        volume->velocity_x[i] += axis_x * impulse;
        volume->velocity_y[i] += axis_y * impulse;
        volume->velocity_z[i] += axis_z * impulse;
    }
}

static void compute_divergence(SimRuntime3DVolume *volume,
                               SimRuntime3DSolverScratch *scratch,
                               const uint8_t *solid_mask) {
    const SimRuntime3DDomainDesc *desc = NULL;
    float inv_two_h = 0.5f;
    if (!volume || !scratch) return;
    desc = &volume->desc;
    inv_two_h = 0.5f / (desc->voxel_size > 0.0f ? desc->voxel_size : 1.0f);

    memset(scratch->divergence, 0, desc->cell_count * sizeof(float));
    memset(volume->pressure, 0, desc->cell_count * sizeof(float));
    memset(scratch->pressure_prev, 0, desc->cell_count * sizeof(float));

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                size_t i_r = sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z);
                size_t i_l = sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z);
                size_t i_u = sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z);
                size_t i_d = sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z);
                size_t i_f = sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1);
                size_t i_b = sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1);
                float vx_r = 0.0f;
                float vx_l = 0.0f;
                float vy_u = 0.0f;
                float vy_d = 0.0f;
                float vz_f = 0.0f;
                float vz_b = 0.0f;

                if (cell_is_solid(solid_mask, idx)) {
                    scratch->divergence[idx] = 0.0f;
                    volume->pressure[idx] = 0.0f;
                    scratch->pressure_prev[idx] = 0.0f;
                    continue;
                }

                vx_r = cell_is_solid(solid_mask, i_r) ? 0.0f : volume->velocity_x[i_r];
                vx_l = cell_is_solid(solid_mask, i_l) ? 0.0f : volume->velocity_x[i_l];
                vy_u = cell_is_solid(solid_mask, i_u) ? 0.0f : volume->velocity_y[i_u];
                vy_d = cell_is_solid(solid_mask, i_d) ? 0.0f : volume->velocity_y[i_d];
                vz_f = cell_is_solid(solid_mask, i_f) ? 0.0f : volume->velocity_z[i_f];
                vz_b = cell_is_solid(solid_mask, i_b) ? 0.0f : volume->velocity_z[i_b];

                scratch->divergence[idx] =
                    ((vx_r - vx_l) + (vy_d - vy_u) + (vz_f - vz_b)) * inv_two_h;
            }
        }
    }
}

static void project_velocity(SimRuntime3DVolume *volume,
                             SimRuntime3DSolverScratch *scratch,
                             const uint8_t *solid_mask,
                             int iterations) {
    const SimRuntime3DDomainDesc *desc = NULL;
    float h = 1.0f;
    float inv_two_h = 0.5f;
    if (!volume || !scratch) return;
    desc = &volume->desc;
    h = desc->voxel_size > 0.0f ? desc->voxel_size : 1.0f;
    inv_two_h = 0.5f / h;

    for (int iter = 0; iter < iterations; ++iter) {
        for (int z = 0; z < desc->grid_d; ++z) {
            for (int y = 0; y < desc->grid_h; ++y) {
                for (int x = 0; x < desc->grid_w; ++x) {
                    size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                    float p_self = scratch->pressure_prev[idx];
                    float p_sum =
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z)]) +
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z)]) +
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z)]) +
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z)]) +
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1)]) +
                        (cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1))
                             ? p_self
                             : scratch->pressure_prev[sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1)]);
                    if (cell_is_solid(solid_mask, idx)) {
                        volume->pressure[idx] = 0.0f;
                        continue;
                    }
                    volume->pressure[idx] = (p_sum - scratch->divergence[idx] * h * h) / 6.0f;
                }
            }
        }
        memcpy(scratch->pressure_prev, volume->pressure, desc->cell_count * sizeof(float));
    }

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                float p_self = volume->pressure[idx];
                float p_r = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x + 1, y, z)];
                float p_l = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x - 1, y, z)];
                float p_u = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x, y - 1, z)];
                float p_d = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x, y + 1, z)];
                float p_f = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x, y, z + 1)];
                float p_b = cell_is_solid(solid_mask, sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1))
                                ? p_self
                                : volume->pressure[sim_runtime_3d_volume_index_clamped(desc, x, y, z - 1)];

                if (cell_is_solid(solid_mask, idx)) {
                    volume->velocity_x[idx] = 0.0f;
                    volume->velocity_y[idx] = 0.0f;
                    volume->velocity_z[idx] = 0.0f;
                    continue;
                }

                volume->velocity_x[idx] -= (p_r - p_l) * inv_two_h;
                volume->velocity_y[idx] -= (p_d - p_u) * inv_two_h;
                volume->velocity_z[idx] -= (p_f - p_b) * inv_two_h;
            }
        }
    }
}

static void advect_density(SimRuntime3DVolume *volume,
                           const SimRuntime3DSolverScratch *scratch,
                           const uint8_t *solid_mask,
                           float dt_cells,
                           float diffusion_blend,
                           float density_decay,
                           float dt) {
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!volume || !scratch) return;
    desc = &volume->desc;

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(desc, x, y, z);
                float vx = volume->velocity_x[idx];
                float vy = volume->velocity_y[idx];
                float vz = volume->velocity_z[idx];
                float sample_x = (float)x - vx * dt_cells;
                float sample_y = (float)y - vy * dt_cells;
                float sample_z = (float)z - vz * dt_cells;

                if (cell_is_solid(solid_mask, idx)) {
                    volume->density[idx] = 0.0f;
                    continue;
                }
                volume->density[idx] = sample_field_trilinear_masked(
                    scratch->density_prev, desc, solid_mask, sample_x, sample_y, sample_z);
            }
        }
    }

    if (diffusion_blend > 0.0f) {
        memcpy(scratch->density_prev, volume->density, desc->cell_count * sizeof(float));
        diffuse_scalar_field(volume->density, scratch->density_prev, desc, solid_mask, diffusion_blend);
    }

    if (density_decay > 0.0f && dt > 0.0f) {
        float decay_mul = 1.0f - density_decay * (float)dt;
        if (decay_mul < 0.0f) decay_mul = 0.0f;
        for (size_t i = 0; i < desc->cell_count; ++i) {
            volume->density[i] *= decay_mul;
        }
    }
}

bool sim_runtime_3d_solver_step_first_pass(SimRuntime3DVolume *volume,
                                           SimRuntime3DSolverScratch *scratch,
                                           const uint8_t *solid_mask,
                                           const SimRuntime3DForceAxis *scene_up_axis,
                                           const AppConfig *cfg,
                                           double dt) {
    float dt_f = 0.0f;
    float voxel_size = 1.0f;
    float dt_cells = 0.0f;
    float diffusion_blend = 0.0f;
    float viscosity_blend = 0.0f;
    int iterations = 0;
    if (!volume || !scratch || !cfg || dt <= 0.0) return false;
    if (volume->desc.cell_count == 0 || scratch->desc.cell_count != volume->desc.cell_count) {
        return false;
    }
    if (!sim_runtime_3d_solver_capture_previous_fields(scratch, volume)) return false;

    dt_f = (float)dt;
    voxel_size = volume->desc.voxel_size > 0.0f ? volume->desc.voxel_size : 1.0f;
    dt_cells = dt_f / voxel_size;
    diffusion_blend = clamp_float_value(cfg->density_diffusion * dt_f, 0.0f, 0.2f);
    viscosity_blend = clamp_float_value(cfg->velocity_damping * dt_f, 0.0f, 0.25f);
    iterations = cfg->fluid_solver_iterations > 0 ? cfg->fluid_solver_iterations : 0;
    if (iterations < 8) iterations = 8;
    if (iterations > 48) iterations = 48;

    apply_solid_mask(volume, solid_mask);
    advect_velocity(volume, scratch, solid_mask, dt_cells);
    diffuse_velocity_fields(volume, scratch, solid_mask, viscosity_blend);
    apply_buoyancy(volume,
                   scratch,
                   solid_mask,
                   scene_up_axis,
                   cfg->fluid_buoyancy_force,
                   dt_f);
    compute_divergence(volume, scratch, solid_mask);
    project_velocity(volume, scratch, solid_mask, iterations);
    advect_density(volume, scratch, solid_mask, dt_cells, diffusion_blend, cfg->density_decay, dt);
    apply_solid_mask(volume, solid_mask);
    return true;
}
