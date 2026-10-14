#ifndef PHYSICS_SIM_PRESCRIBED_FLOW_H
#define PHYSICS_SIM_PRESCRIBED_FLOW_H
#include "app/cfd_open_atmosphere3d.h"
/* Qualification only. Extensive energy cell averages prescribe buoyancy;
 * they never replace, add to or transport the state's accepted heat. */
bool prescribed_flow_step(CfdOpenAtmosphere3d *state, const double *forcing_j,
                          const double *source_j, const double *source_kg);
/* Nested 2:1, constant intensive temperature within each coarse cell. */
bool prescribed_flow_prolong(const CfdOpenAtmosphere3d *coarse,
                             const CfdOpenAtmosphere3d *fine, double *forcing_j);
/* Conservative bounded linear temperature reconstruction; qualification only. */
bool prescribed_flow_reconstruct(const CfdOpenAtmosphere3d *coarse,
                                const CfdOpenAtmosphere3d *fine, double *forcing_j);
#endif
