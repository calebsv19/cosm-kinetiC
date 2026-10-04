#ifndef PHYSICS_SIM_CFD_OPEN3D_H
#define PHYSICS_SIM_CFD_OPEN3D_H
#include "app/cfd_cartesian3d.h"
typedef struct CfdOpen3dImpl CfdOpen3dImpl;
/* Owned stationary MAC Stokes solve. See docs/cfd_open3d_gate.md for boundary semantics. */
typedef struct {
    CfdCartesian3d grid;
    CfdOpen3dImpl *impl;
    double *velocity, *pressure, *outlet_velocity;
    double density, viscosity, requested_flow, outlet_datum_pa;
    double inlet_flow, outlet_flow, pressure_in_pa, pressure_out_pa, pressure_drop_pa;
    double upstream_gradient_pa_m, wall_force_n[4], wall_relative_error[4];
    double dissipation_w, discrete_diffusion_w, boundary_power_w, energy_imbalance;
    double velocity_relative_l2, pressure_relative_error, energy_relative_error;
    double relative_residual, max_divergence, flux_relative_error;
    double setup_cpu_ms, solve_cpu_ms;
    int iterations, velocity_iterations;
    bool solved;
} CfdOpen3d;
bool cfd_open3d_init(CfdOpen3d *s, const int n[3], const double length[3], double rho, double mu,
                     double flow, double outlet_datum_pa);
bool cfd_open3d_solve(CfdOpen3d *s);
void cfd_open3d_destroy(CfdOpen3d *s);
bool cfd_open3d_gate(const CfdOpen3d *s);
/* Physical MAC face coordinates: X i=0..Nx, Y j=0..Ny, Z k=0..Nz. */
double cfd_open3d_face(const CfdOpen3d *s, int axis, int i, int j, int k);
void cfd_open3d_derivatives(const CfdOpen3d *s, int i, int j, int k, double d[3][3]);
#ifdef CFD_OPEN3D_VERIFY
bool cfd_open3d_verify_operators(CfdOpen3d *s);
#endif
#endif
