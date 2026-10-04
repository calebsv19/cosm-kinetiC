#ifndef PHYSICS_SIM_CFD_OPEN2D_BUDGET_H
#define PHYSICS_SIM_CFD_OPEN2D_BUDGET_H
#include "app/cfd_open2d.h"
typedef struct CfdOpen2DBudget {
    double momentum_x_kg_m_s, kinetic_energy_j;
    double pressure_force_x_n, normal_viscous_force_x_n, wall_force_x_n;
    double outward_momentum_flux_n;
    double pressure_work_w, viscous_work_w, outward_kinetic_flux_w, dissipation_w;
} CfdOpen2DBudget;
/* Independent physical-space quadrature, not an exact discrete conservation
 * identity. Currently requires an empty channel; returns false for solids.
 * Time derivatives must be formed from two observations by the caller. */
bool cfd_open2d_budget(const CfdOpen2D *c, CfdOpen2DBudget *out);
typedef struct CfdOpen2DEnergy {
    double kinetic_energy_j, pressure_work_w, viscous_work_w;
    double outward_kinetic_flux_w, dissipation_w;
} CfdOpen2DEnergy;
/* Physical fluid-cell strain quadrature, including stationary no-slip masks.
 * Body velocity and body power are zero. No body momentum budget is implied. */
bool cfd_open2d_energy(const CfdOpen2D *c, CfdOpen2DEnergy *out);
#endif
