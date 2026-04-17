#include "app/sim_runtime_3d_solver.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool sim_runtime_3d_solver_scratch_init(SimRuntime3DSolverScratch *scratch,
                                        const SimRuntime3DDomainDesc *desc) {
    SimRuntime3DSolverScratch next = {0};
    if (!scratch || !desc || desc->cell_count == 0) return false;

    next.desc = *desc;
    next.density_prev = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_x_prev = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_y_prev = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_z_prev = (float *)calloc(desc->cell_count, sizeof(float));
    next.pressure_prev = (float *)calloc(desc->cell_count, sizeof(float));
    next.divergence = (float *)calloc(desc->cell_count, sizeof(float));
    if (!next.density_prev ||
        !next.velocity_x_prev ||
        !next.velocity_y_prev ||
        !next.velocity_z_prev ||
        !next.pressure_prev ||
        !next.divergence) {
        sim_runtime_3d_solver_scratch_destroy(&next);
        return false;
    }

    *scratch = next;
    return true;
}

void sim_runtime_3d_solver_scratch_destroy(SimRuntime3DSolverScratch *scratch) {
    if (!scratch) return;
    free(scratch->density_prev);
    free(scratch->velocity_x_prev);
    free(scratch->velocity_y_prev);
    free(scratch->velocity_z_prev);
    free(scratch->pressure_prev);
    free(scratch->divergence);
    memset(scratch, 0, sizeof(*scratch));
}

void sim_runtime_3d_solver_scratch_clear(SimRuntime3DSolverScratch *scratch) {
    if (!scratch || scratch->desc.cell_count == 0) return;
    if (scratch->density_prev) {
        memset(scratch->density_prev, 0, scratch->desc.cell_count * sizeof(float));
    }
    if (scratch->velocity_x_prev) {
        memset(scratch->velocity_x_prev, 0, scratch->desc.cell_count * sizeof(float));
    }
    if (scratch->velocity_y_prev) {
        memset(scratch->velocity_y_prev, 0, scratch->desc.cell_count * sizeof(float));
    }
    if (scratch->velocity_z_prev) {
        memset(scratch->velocity_z_prev, 0, scratch->desc.cell_count * sizeof(float));
    }
    if (scratch->pressure_prev) {
        memset(scratch->pressure_prev, 0, scratch->desc.cell_count * sizeof(float));
    }
    if (scratch->divergence) {
        memset(scratch->divergence, 0, scratch->desc.cell_count * sizeof(float));
    }
}

bool sim_runtime_3d_solver_capture_previous_fields(SimRuntime3DSolverScratch *scratch,
                                                   const SimRuntime3DVolume *volume) {
    if (!scratch || !volume) return false;
    if (scratch->desc.cell_count != volume->desc.cell_count ||
        !scratch->density_prev ||
        !scratch->velocity_x_prev ||
        !scratch->velocity_y_prev ||
        !scratch->velocity_z_prev ||
        !scratch->pressure_prev ||
        !volume->density ||
        !volume->velocity_x ||
        !volume->velocity_y ||
        !volume->velocity_z ||
        !volume->pressure) {
        return false;
    }

    memcpy(scratch->density_prev, volume->density, scratch->desc.cell_count * sizeof(float));
    memcpy(scratch->velocity_x_prev, volume->velocity_x, scratch->desc.cell_count * sizeof(float));
    memcpy(scratch->velocity_y_prev, volume->velocity_y, scratch->desc.cell_count * sizeof(float));
    memcpy(scratch->velocity_z_prev, volume->velocity_z, scratch->desc.cell_count * sizeof(float));
    memcpy(scratch->pressure_prev, volume->pressure, scratch->desc.cell_count * sizeof(float));
    return true;
}

size_t sim_runtime_3d_volume_index_clamped(const SimRuntime3DDomainDesc *desc,
                                           int x,
                                           int y,
                                           int z) {
    if (!desc) return 0;
    x = clamp_int(x, 0, desc->grid_w - 1);
    y = clamp_int(y, 0, desc->grid_h - 1);
    z = clamp_int(z, 0, desc->grid_d - 1);
    return sim_runtime_3d_volume_index(desc, x, y, z);
}

float sim_runtime_3d_sample_field_trilinear(const float *field,
                                            const SimRuntime3DDomainDesc *desc,
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
    float c00 = 0.0f;
    float c10 = 0.0f;
    float c01 = 0.0f;
    float c11 = 0.0f;
    float c0 = 0.0f;
    float c1 = 0.0f;

    if (!field || !desc || desc->grid_w <= 0 || desc->grid_h <= 0 || desc->grid_d <= 0) {
        return 0.0f;
    }

    x = clamp_float(x, 0.0f, (float)(desc->grid_w - 1));
    y = clamp_float(y, 0.0f, (float)(desc->grid_h - 1));
    z = clamp_float(z, 0.0f, (float)(desc->grid_d - 1));

    x0 = (int)floorf(x);
    y0 = (int)floorf(y);
    z0 = (int)floorf(z);
    x1 = clamp_int(x0 + 1, 0, desc->grid_w - 1);
    y1 = clamp_int(y0 + 1, 0, desc->grid_h - 1);
    z1 = clamp_int(z0 + 1, 0, desc->grid_d - 1);

    tx = x - (float)x0;
    ty = y - (float)y0;
    tz = z - (float)z0;

    c00 = field[sim_runtime_3d_volume_index(desc, x0, y0, z0)] * (1.0f - tx) +
          field[sim_runtime_3d_volume_index(desc, x1, y0, z0)] * tx;
    c10 = field[sim_runtime_3d_volume_index(desc, x0, y1, z0)] * (1.0f - tx) +
          field[sim_runtime_3d_volume_index(desc, x1, y1, z0)] * tx;
    c01 = field[sim_runtime_3d_volume_index(desc, x0, y0, z1)] * (1.0f - tx) +
          field[sim_runtime_3d_volume_index(desc, x1, y0, z1)] * tx;
    c11 = field[sim_runtime_3d_volume_index(desc, x0, y1, z1)] * (1.0f - tx) +
          field[sim_runtime_3d_volume_index(desc, x1, y1, z1)] * tx;

    c0 = c00 * (1.0f - ty) + c10 * ty;
    c1 = c01 * (1.0f - ty) + c11 * ty;
    return c0 * (1.0f - tz) + c1 * tz;
}

bool sim_runtime_3d_sample_velocity_trilinear(const SimRuntime3DVolume *volume,
                                              float x,
                                              float y,
                                              float z,
                                              float *out_vx,
                                              float *out_vy,
                                              float *out_vz) {
    if (!volume || !out_vx || !out_vy || !out_vz) return false;
    *out_vx = sim_runtime_3d_sample_field_trilinear(
        volume->velocity_x, &volume->desc, x, y, z);
    *out_vy = sim_runtime_3d_sample_field_trilinear(
        volume->velocity_y, &volume->desc, x, y, z);
    *out_vz = sim_runtime_3d_sample_field_trilinear(
        volume->velocity_z, &volume->desc, x, y, z);
    return true;
}
