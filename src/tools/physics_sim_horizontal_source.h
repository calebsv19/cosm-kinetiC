#ifndef PHYSICS_SIM_HORIZONTAL_SOURCE_H
#define PHYSICS_SIM_HORIZONTAL_SOURCE_H
#include <stdbool.h>
/* Split original N*N*(N/8) extensive support into four children on 2N*N*2N.
 * Summing X children recovers the accepted vertical source exactly. */
bool physics_sim_horizontal_source(const double *support, int parent, double *full);
#endif
