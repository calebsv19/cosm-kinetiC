#ifndef PHYSICS_SIM_CFD_WALL3D_H
#define PHYSICS_SIM_CFD_WALL3D_H
#include "app/cfd_mixed3d.h"
typedef struct {
    CfdCartesian3d grid;
    CfdMixed3d *mixed;
    double *storage, *velocity, *previous, *pressure, *rhs, *candidate, *candidate_pressure;
    double *face_reference, *dual_reference, *lap_reference, *grad_pressure_reference,
        *pressure_reference;
    double rho, mu, dt, time, reference_length[3], boundary_power_w;
    double *traction_velocity, *traction_pressure;
    int steps, count, iterations, inner_iterations;
    double true_residual, max_divergence, velocity_error, pressure_error, wall_error[8];
    double kinetic_j, dissipation_w, forcing_power_w, energy_rate_w, energy_residual_w;
    double reference_kinetic_j, reference_dissipation_w, reference_power_w, reference_energy_rate_w;
    double kinetic_base, dissipation_base, previous_energy, older_energy, amplitude,
        orthogonal_error;
    double setup_cpu_ms, solve_cpu_ms, cfl, transport_power_w, transport_self_power_w;
    double *transport_reference, *transport_work, *extrapolated;
    bool failed, transport, open;
    const char *error;
} CfdWall3d;
bool cfd_wall3d_init(CfdWall3d *s, const int n[3], const double length[3], double rho, double mu,
                     double dt, bool transport);
/* Open unsteady Stokes with fixed 4 m X wavelength and continuous natural
 * vector-Laplacian tractions. No nonlinear wake/backflow qualification. */
bool cfd_wall3d_init_open(CfdWall3d *s, const int n[3], const double length[3], double rho,
                          double mu, double dt);
bool cfd_wall3d_step(CfdWall3d *s);
void cfd_wall3d_destroy(CfdWall3d *s);
void cfd_wall3d_reference(const double length[3], const double xyz[3], const double width[3],
                          const int derivative[3], double velocity[3]);
void cfd_wall3d_reference_transport(const double length[3], const double xyz[3],
                                    const double width[3], double out[3]);
void cfd_wall3d_transport(const CfdWall3d *s, const double *velocity, double *out);
double cfd_wall3d_reference_pressure(const double length[3], const double xyz[3],
                                     const double width[3], int derivative);
double cfd_wall3d_face(const CfdWall3d *s, const double *u, int axis, int i, int j, int k);
void cfd_wall3d_derivatives(const CfdWall3d *s, int i, int j, int k, double d[3][3]);
#endif
