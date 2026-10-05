#ifndef PHYSICS_SIM_CFD_ATMOSPHERE3D_H
#define PHYSICS_SIM_CFD_ATMOSPHERE3D_H
#include "app/cfd_periodic3d.h"
#include "app/cfd_passive3d.h"
/* Body-free periodic evolving flow + passive energy/smoke. Fixed momentum dt.
 * Single owner, no pointer copying. No buoyancy or open-boundary policy. */
typedef struct {
    CfdPeriodic3d flow;
    CfdPassive3d scalar;
    double viscosity, conductivity;
} CfdAtmosphere3d;
bool cfd_atmosphere3d_init(CfdAtmosphere3d *s, const int n[3], const double length[3],
    double rho, double mu, double dt, const double *velocity, double cp,
    double reference_k, double conductivity, double diffusivity);
void cfd_atmosphere3d_destroy(CfdAtmosphere3d *s);
/* Whole pair publishes only after both candidates pass. Failure preserves
 * every accepted array, history, clock and budget, including failed flags.
 * Scalar uses the newly projected velocity for this fixed step (first-order split). */
bool cfd_atmosphere3d_step(CfdAtmosphere3d *s, const double *energy_j, const double *smoke_kg);
#endif
