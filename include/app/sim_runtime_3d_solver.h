#ifndef SIM_RUNTIME_3D_SOLVER_H
#define SIM_RUNTIME_3D_SOLVER_H

#include <fisics/extensions.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_3d_domain.h"

typedef struct SimRuntime3DProjectionBoundary {
    bool prescribed_inlet;
    int inlet_axis; /* X=0, Y=1, Z=2 */
    bool inlet_at_max;
    float inflow_speed;
} SimRuntime3DProjectionBoundary;

typedef struct SimRuntime3DSolverScratch {
    SimRuntime3DProjectionBoundary boundary;
    SimRuntime3DDomainDesc desc;
    float *density_prev;
    float *velocity_x_prev;
    float *velocity_y_prev;
    float *velocity_z_prev;
    float *pressure_prev;
    float *divergence;
} SimRuntime3DSolverScratch;

typedef struct SimRuntime3DForceAxis {
    bool valid;
    float x;
    float y;
    float z;
} SimRuntime3DForceAxis;

typedef struct SimRuntime3DSolverStepMetrics {
    size_t transport_corrected_components;
    size_t transport_limited_components;
    size_t transport_fallback_components;
    int projection_iterations_used;
    bool projection_converged;
    size_t velocity_clamp_cell_count;
    float max_velocity_magnitude_pre_clamp;
    float max_velocity_magnitude_post_clamp;
    float max_velocity_displacement_cells_pre_clamp;
    float max_velocity_displacement_cells_post_clamp;
    float max_abs_divergence_before_project;
    float pressure_residual_linf;
    float max_abs_divergence_after_project;
} SimRuntime3DSolverStepMetrics;

int sim_runtime_3d_solver_iterations_for_requested(int requested_iterations);
int sim_runtime_3d_solver_iterations_for_config(const AppConfig *cfg);

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
                                           const SimRuntime3DForceAxis *scene_up_axis,
                                           const AppConfig *cfg,
                                           double dt FISICS_DIM(time) FISICS_UNIT(second),
                                           float max_velocity_displacement_cells_limit,
                                           SimRuntime3DSolverStepMetrics *out_metrics);

typedef struct SimRuntime3DConservation {
    bool valid;
    double boundary_flux_m3_s[6]; /* outward, min/max X, Y, Z */
    double net_outward_flux_m3_s;
    double integrated_divergence_m3_s;
    double max_abs_divergence_s_inv;
    double kinetic_energy_per_density;
} SimRuntime3DConservation;

bool sim_runtime_3d_measure_conservation(const SimRuntime3DDomainDesc *desc,
    const float *vx, const float *vy, const float *vz, const uint8_t *solid,
    SimRuntime3DConservation *out);

bool sim_runtime_3d_project_boundary(SimRuntime3DVolume *volume,
    const uint8_t *solid_mask, int iterations,
    const SimRuntime3DProjectionBoundary *boundary, SimRuntime3DSolverStepMetrics *metrics);

// Matched centered divergence/transpose projection for qualification only.
bool sim_runtime_3d_project_compatible(SimRuntime3DVolume *volume,
    const uint8_t *solid_mask, int iterations, SimRuntime3DSolverStepMetrics *metrics);

// Bounded MacCormack transport; captured previous fields, pressure scratch reused.
void sim_runtime_3d_advect_velocity_bounded(SimRuntime3DVolume *volume,
    SimRuntime3DSolverScratch *scratch, const uint8_t *solid_mask, float dt_cells, SimRuntime3DSolverStepMetrics *metrics);

// SI diffusion: nu in m^2/s; bounded explicit substeps (alpha <= 1/6).
bool sim_runtime_3d_diffuse_velocity_si(SimRuntime3DVolume *volume,
    SimRuntime3DSolverScratch *scratch, const uint8_t *solid_mask, double nu, double dt);
float sim_runtime_3d_pressure_residual(const SimRuntime3DVolume *volume,
    const SimRuntime3DSolverScratch *scratch, const uint8_t *solid_mask);
#endif
