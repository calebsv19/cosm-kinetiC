/* Qualification input adapter: nested cell-integrated amounts, not densities. */
#ifndef PHYSICS_SIM_FLOW_SOURCE_H
#define PHYSICS_SIM_FLOW_SOURCE_H
#include <stdbool.h>
bool physics_sim_flow_source(const double *parent, int side, double *fine, int fine_side);
bool physics_sim_flow_support(const double *support, int side, double *full);
#endif
