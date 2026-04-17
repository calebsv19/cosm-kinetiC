#include "app/sim_runtime_3d_solver.h"

#include <stdbool.h>
#include <stdio.h>

static bool nearly_equal(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0001f;
}

static SimRuntime3DDomainDesc test_desc(void) {
    SimRuntime3DDomainDesc desc = {0};
    desc.grid_w = 4;
    desc.grid_h = 4;
    desc.grid_d = 4;
    desc.slice_cell_count = 16;
    desc.cell_count = 64;
    desc.voxel_size = 1.0f;
    desc.world_max_x = 4.0f;
    desc.world_max_y = 4.0f;
    desc.world_max_z = 4.0f;
    return desc;
}

static bool test_solver_scratch_capture_and_clear(void) {
    SimRuntime3DDomainDesc desc = test_desc();
    SimRuntime3DVolume volume = {0};
    SimRuntime3DSolverScratch scratch = {0};
    size_t idx = 0;

    if (!sim_runtime_3d_volume_init(&volume, &desc)) return false;
    if (!sim_runtime_3d_solver_scratch_init(&scratch, &desc)) return false;

    idx = sim_runtime_3d_volume_index(&desc, 1, 2, 3);
    volume.density[idx] = 9.0f;
    volume.velocity_x[idx] = 1.0f;
    volume.velocity_y[idx] = 2.0f;
    volume.velocity_z[idx] = 3.0f;
    volume.pressure[idx] = 4.0f;

    if (!sim_runtime_3d_solver_capture_previous_fields(&scratch, &volume)) return false;
    if (!nearly_equal(scratch.density_prev[idx], 9.0f)) return false;
    if (!nearly_equal(scratch.velocity_x_prev[idx], 1.0f)) return false;
    if (!nearly_equal(scratch.velocity_y_prev[idx], 2.0f)) return false;
    if (!nearly_equal(scratch.velocity_z_prev[idx], 3.0f)) return false;
    if (!nearly_equal(scratch.pressure_prev[idx], 4.0f)) return false;

    scratch.divergence[idx] = 11.0f;
    sim_runtime_3d_solver_scratch_clear(&scratch);
    if (!nearly_equal(scratch.density_prev[idx], 0.0f)) return false;
    if (!nearly_equal(scratch.velocity_x_prev[idx], 0.0f)) return false;
    if (!nearly_equal(scratch.velocity_y_prev[idx], 0.0f)) return false;
    if (!nearly_equal(scratch.velocity_z_prev[idx], 0.0f)) return false;
    if (!nearly_equal(scratch.pressure_prev[idx], 0.0f)) return false;
    if (!nearly_equal(scratch.divergence[idx], 0.0f)) return false;

    sim_runtime_3d_solver_scratch_destroy(&scratch);
    sim_runtime_3d_volume_destroy(&volume);
    return true;
}

static bool test_trilinear_field_sampling_reproduces_linear_gradient(void) {
    SimRuntime3DDomainDesc desc = test_desc();
    float field[64] = {0};
    float sample = 0.0f;

    for (int z = 0; z < desc.grid_d; ++z) {
        for (int y = 0; y < desc.grid_h; ++y) {
            for (int x = 0; x < desc.grid_w; ++x) {
                field[sim_runtime_3d_volume_index(&desc, x, y, z)] =
                    (float)x + (float)y * 10.0f + (float)z * 100.0f;
            }
        }
    }

    sample = sim_runtime_3d_sample_field_trilinear(field, &desc, 1.25f, 1.5f, 2.0f);
    return nearly_equal(sample, 216.25f);
}

static bool test_trilinear_velocity_sampling_reads_all_components(void) {
    SimRuntime3DDomainDesc desc = test_desc();
    SimRuntime3DVolume volume = {0};
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;

    if (!sim_runtime_3d_volume_init(&volume, &desc)) return false;
    for (int z = 0; z < desc.grid_d; ++z) {
        for (int y = 0; y < desc.grid_h; ++y) {
            for (int x = 0; x < desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&desc, x, y, z);
                volume.velocity_x[idx] = (float)x;
                volume.velocity_y[idx] = (float)y * 2.0f;
                volume.velocity_z[idx] = (float)z * 3.0f;
            }
        }
    }

    if (!sim_runtime_3d_sample_velocity_trilinear(&volume, 1.5f, 2.0f, 0.5f, &vx, &vy, &vz)) {
        return false;
    }
    sim_runtime_3d_volume_destroy(&volume);
    return nearly_equal(vx, 1.5f) && nearly_equal(vy, 4.0f) && nearly_equal(vz, 1.5f);
}

static bool test_clamped_index_bounds(void) {
    SimRuntime3DDomainDesc desc = test_desc();
    size_t idx = sim_runtime_3d_volume_index_clamped(&desc, -5, 9, 2);
    return idx == sim_runtime_3d_volume_index(&desc, 0, 3, 2);
}

static bool test_first_pass_step_evolves_density_and_velocity(void) {
    SimRuntime3DDomainDesc desc = test_desc();
    SimRuntime3DVolume volume = {0};
    SimRuntime3DSolverScratch scratch = {0};
    AppConfig cfg = {0};
    size_t center = 0;
    bool ok = false;
    bool found_spread_density = false;
    bool found_solver_response = false;

    desc.grid_w = 6;
    desc.grid_h = 6;
    desc.grid_d = 6;
    desc.slice_cell_count = 36;
    desc.cell_count = 216;
    desc.world_max_x = 6.0f;
    desc.world_max_y = 6.0f;
    desc.world_max_z = 6.0f;

    if (!sim_runtime_3d_volume_init(&volume, &desc)) return false;
    if (!sim_runtime_3d_solver_scratch_init(&scratch, &desc)) return false;

    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.98f;
    cfg.density_diffusion = 0.10f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 1.0f;

    center = sim_runtime_3d_volume_index(&desc, 2, 3, 3);
    volume.density[center] = 12.0f;
    volume.velocity_x[center] = 2.0f;

    if (!sim_runtime_3d_solver_step_first_pass(&volume, &scratch, NULL, &cfg, 0.5)) {
        return false;
    }

    for (size_t i = 0; i < desc.cell_count; ++i) {
        if (i != center && volume.density[i] > 0.001f) {
            found_spread_density = true;
        }
        if (fabsf(volume.pressure[i]) > 0.0001f || fabsf(volume.velocity_y[i]) > 0.0001f) {
            found_solver_response = true;
        }
        if (found_spread_density && found_solver_response) break;
    }
    ok = found_spread_density &&
         found_solver_response &&
         volume.density[center] < 12.0f;

    sim_runtime_3d_solver_scratch_destroy(&scratch);
    sim_runtime_3d_volume_destroy(&volume);
    return ok;
}

int main(void) {
    if (!test_solver_scratch_capture_and_clear()) {
        fprintf(stderr, "sim_runtime_3d_solver_contract_test: scratch capture/clear failed\n");
        return 1;
    }
    if (!test_trilinear_field_sampling_reproduces_linear_gradient()) {
        fprintf(stderr, "sim_runtime_3d_solver_contract_test: field sampling failed\n");
        return 1;
    }
    if (!test_trilinear_velocity_sampling_reads_all_components()) {
        fprintf(stderr, "sim_runtime_3d_solver_contract_test: velocity sampling failed\n");
        return 1;
    }
    if (!test_clamped_index_bounds()) {
        fprintf(stderr, "sim_runtime_3d_solver_contract_test: clamped index failed\n");
        return 1;
    }
    if (!test_first_pass_step_evolves_density_and_velocity()) {
        fprintf(stderr, "sim_runtime_3d_solver_contract_test: first-pass step failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_3d_solver_contract_test: success\n");
    return 0;
}
