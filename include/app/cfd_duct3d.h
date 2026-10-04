#ifndef PHYSICS_SIM_CFD_DUCT3D_H
#define PHYSICS_SIM_CFD_DUCT3D_H
#include "app/cfd_cartesian3d.h"
typedef struct {
    CfdCartesian3d grid;
    CfdCartesian3dLinear *linear;
    double *velocity, *pressure, *rhs;
    double density, viscosity, requested_flow;
    double gradient_pa_m, flow_m3_s, wall_force_n[4], dissipation_w, discrete_dissipation_w;
    double velocity_relative_l2, pressure_relative_error, wall_relative_error[4];
    double energy_relative_error, relative_residual, max_divergence, setup_cpu_ms, solve_cpu_ms;
    int iterations;
    bool solved;
} CfdDuct3d;
bool cfd_duct3d_init(CfdDuct3d *s, const int n[3], const double length[3], double rho, double mu,
                     double flow);
bool cfd_duct3d_solve(CfdDuct3d *s);
void cfd_duct3d_destroy(CfdDuct3d *s);
/* Continuous Fourier solution evaluated independently of grid/native stencil.
 * response units m^2, velocity=(G/mu)*response. mean response controls flow. */
double cfd_duct3d_reference_response(double h, double w, double y, double z, int terms);
double cfd_duct3d_reference_mean(double h, double w, int terms);
void cfd_duct3d_reference_walls(double h, double w, double derivative_integral[4], int terms);
#endif
