#ifndef PHYSICS_SIM_CFD_SURFACE_FORCE_H
#define PHYSICS_SIM_CFD_SURFACE_FORCE_H
#include <stdbool.h>
/* Normal points OUT of the fluid control volume. grad[i][j]=du_i/dx_j.
 * Surface motion is prescribed in m/s; flux is relative to that surface.
 * Body reaction is the negative of traction on fluid, not momentum flux. */
typedef struct CfdSurfaceFlux {
    double pressure_on_fluid_n[3], viscous_on_fluid_n[3];
    double outward_momentum_n[3], outward_mass_kg_s;
} CfdSurfaceFlux;
bool cfd_surface_flux(double rho, double mu, double pressure, double area, const double normal[3],
                      const double velocity[3], const double surface_velocity[3],
                      const double grad[3][3], CfdSurfaceFlux *out);
#endif
