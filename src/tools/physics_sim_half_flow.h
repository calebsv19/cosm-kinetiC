#ifndef PHYSICS_SIM_HALF_FLOW_H
#define PHYSICS_SIM_HALF_FLOW_H
#include "app/cfd_open_atmosphere3d.h"
/* Qualification-only change of step representation. No physical advance. */
bool physics_sim_half_flow(CfdOpenAtmosphere3d *s,double *source);
#endif
