#ifndef PHYSICS_SIM_MATCHED_HEAT_H
#define PHYSICS_SIM_MATCHED_HEAT_H
#include "app/cfd_open_atmosphere3d.h"
/* Qualification only: no worker API. Linear normal-face interpolation and
 * constant tangential prolongation preserve each coarse face flux and cell
 * divergence. This prescribes the coarse flow, not a fine momentum solution. */
bool matched_heat_velocity(const CfdOpenAtmosphere3d *coarse,
                           CfdOpenAtmosphere3d *fine, double *max_divergence);
bool matched_heat_advance(CfdOpenAtmosphere3d *state, const double *source_j,
                          unsigned cycles, double *courant_bound);
#endif
