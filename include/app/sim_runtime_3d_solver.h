#ifndef SIM_RUNTIME_3D_SOLVER_H
#define SIM_RUNTIME_3D_SOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_3d_domain.h"

typedef struct SimRuntime3DSolverScratch {
    SimRuntime3DDomainDesc desc;
    float *density_prev;
    float *velocity_x_prev;
    float *velocity_y_prev;
    float *velocity_z_prev;
    float *pressure_prev;
    float *divergence;
} SimRuntime3DSolverScratch;

bool sim_runtime_3d_solver_scratch_init(SimRuntime3DSolverScratch *scratch,
                                        const SimRuntime3DDomainDesc *desc);
void sim_runtime_3d_solver_scratch_destroy(SimRuntime3DSolverScratch *scratch);
void sim_runtime_3d_solver_scratch_clear(SimRuntime3DSolverScratch *scratch);
bool sim_runtime_3d_solver_capture_previous_fields(SimRuntime3DSolverScratch *scratch,
                                                   const SimRuntime3DVolume *volume);

size_t sim_runtime_3d_volume_index_clamped(const SimRuntime3DDomainDesc *desc,
                                           int x,
                                           int y,
                                           int z);
float sim_runtime_3d_sample_field_trilinear(const float *field,
                                            const SimRuntime3DDomainDesc *desc,
                                            float x,
                                            float y,
                                            float z);
bool sim_runtime_3d_sample_velocity_trilinear(const SimRuntime3DVolume *volume,
                                              float x,
                                              float y,
                                              float z,
                                              float *out_vx,
                                              float *out_vy,
                                              float *out_vz);
bool sim_runtime_3d_solver_step_first_pass(SimRuntime3DVolume *volume,
                                           SimRuntime3DSolverScratch *scratch,
                                           const uint8_t *solid_mask,
                                           const AppConfig *cfg,
                                           double dt);

#endif
