#ifndef PHYSICS_SIM_CFD_PERIODIC3D_H
#define PHYSICS_SIM_CFD_PERIODIC3D_H
#include "app/cfd_cartesian3d.h"
typedef struct {
    CfdCartesian3d grid;
    CfdCartesian3dLinear *momentum, *poisson;
    double *storage, *velocity, *previous, *pressure, *rhs, *star, *gradient;
    double *phi, *divergence, *transport, *extrapolated, *forcing;
    double rho, nu, time, dt, mass;
    double true_residual, max_divergence, cfl, velocity_l2_error, pressure_l2_error;
    double kinetic_j, physical_dissipation_w, forcing_power_w, energy_rate_w, energy_residual_w;
    double setup_cpu_ms, transport_cpu_ms, solve_cpu_ms;
    double previous_energy, older_energy;
    int steps, iterations;
    bool failed, unforced;
} CfdPeriodic3d;
/* Bounded verification flow: fully periodic manufactured Navier-Stokes.
 * It is deliberately separate from arbitrary wall/open-domain CFD. */
bool cfd_periodic3d_init(CfdPeriodic3d *s, const int n[3], const double length[3], double rho,
                         double mu, double dt);
/* Unforced incompressible Navier-Stokes on the same uniform periodic grid.
 * velocity contains 3 * cells lower-face values in m/s. Initial pressure is
 * zero; subsequent pressure is solved in Pa. The initial field must be finite
 * and divergence-free to 1e-8 /s. Analytic error fields are unavailable (NaN).
 * Caller owns the input and may release it after successful initialization. */
bool cfd_periodic3d_init_unforced(CfdPeriodic3d *s, const int n[3],
                                  const double length[3], double rho, double mu,
                                  double dt, const double *velocity, size_t values);
bool cfd_periodic3d_step(CfdPeriodic3d *s);
void cfd_periodic3d_destroy(CfdPeriodic3d *s);
void cfd_periodic3d_exact(const double length[3], const double xyz[3], double t, double rho,
                          double nu, double velocity[3], double *pressure, double forcing[3]);
/* Shared dual-face central transport. Sum of transport over the periodic domain
 * is zero for each component; no independent cell flux or artificial wake. */
void cfd_periodic3d_transport(const CfdCartesian3d *g, const double *velocity, double *out);
#endif
