#ifndef PHYSICS_SIM_CFD_STARTUP3D_H
#define PHYSICS_SIM_CFD_STARTUP3D_H
#include "app/cfd_mixed3d.h"
typedef struct {
    CfdCartesian3d grid;
    CfdMixed3d *mixed;
    double *storage, *velocity, *previous, *pressure, *candidate, *candidate_pressure, *rhs,
        *steady_profile;
    double rho, mu, dt, time, gradient;
    int steps, count, iterations, inner_iterations;
    double true_residual, max_divergence, flow, pressure_drop, wall_force[4], dissipation, kinetic,
        boundary_power, energy_rate, energy_residual;
    double reference_flow, reference_wall[4], reference_dissipation, reference_kinetic,
        reference_energy_rate, reference_power;
    double velocity_error, flow_error, pressure_error, wall_error[4], dissipation_error;
    double setup_cpu_ms, solve_cpu_ms, previous_energy, older_energy;
    bool failed;
    const char *error;
} CfdStartup3d;
bool cfd_startup3d_init(CfdStartup3d *s, const int n[3], const double length[3], double rho,
                        double mu, double dt, double steady_flow);
bool cfd_startup3d_step(CfdStartup3d *s);
void cfd_startup3d_destroy(CfdStartup3d *s);
void cfd_startup3d_reference(CfdStartup3d *s, double time, int terms, double *profile);
#endif
