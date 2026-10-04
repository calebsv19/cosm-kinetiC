#ifndef PHYSICS_SIM_CFD_MAC2D_H
#define PHYSICS_SIM_CFD_MAC2D_H
#include "app/cfd_channel.h"
typedef struct CfdMac2D {
    CfdChannel parameters;
    int nx, ny, pressure_iterations, substeps;
    double *u, *v, *p, *ut, *vt, *r, *d, *a;
    unsigned char *solid;
    int fluid_cells;
    double obstacle_pressure_force_x_n; /* final projection discrete reaction */
    double obstacle_surface_pressure_force_x_n, obstacle_mean_surface_pressure_force_x_n;
    double continuum_drive_force_x_n, unresolved_drive_force_x_n;
    bool surface_pressure_valid;
    double obstacle_mean_pressure_force_x_n, obstacle_viscous_force_x_n;
    double outer_wall_force_on_fluid_x_n, mean_drive_force_x_n;
    double time, divergence_before, divergence_after, pressure_residual;
    double projection_energy_change, momentum_residual, max_acceleration;
} CfdMac2D;
bool cfd_mac2d_init(CfdMac2D *c, int nx, int ny, const double dimensions[3], double rho, double mu,
                    double gradient, double bottom, double top, double perturbation);
/* Stationary rectangle, half-open cell bounds. Install at rest before stepping.
 * Masked donor-cell momentum uses a stair-step dual-volume wall closure. */
bool cfd_mac2d_set_obstacle(CfdMac2D *c, int x0, int y0, int x1, int y1);
void cfd_mac2d_destroy(CfdMac2D *c);
bool cfd_mac2d_project(CfdMac2D *c, double dt);
/* Reconstruct periodic-correction pressure onto exposed X-normal solid faces.
 * Distinct from the momentum operator reaction; no affine mean-pressure term. */
bool cfd_mac2d_surface_pressure(const CfdMac2D *c, double *force_x_n);
/* Optional prescribed acceleration in m/s^2, sampled at faces and substep start.
 * NULL preserves the ordinary unforced model. Not exposed through scene authoring. */
typedef double (*CfdMac2DAcceleration)(void *context, int component, double x, double y,
                                       double time);
bool cfd_mac2d_step_forced(CfdMac2D *c, double dt, CfdMac2DAcceleration force, void *context);
bool cfd_mac2d_step(CfdMac2D *c, double dt);
double cfd_mac2d_divergence(const CfdMac2D *c);
void cfd_mac2d_values(const CfdMac2D *c, double x, double y, double values[11]);
#endif
