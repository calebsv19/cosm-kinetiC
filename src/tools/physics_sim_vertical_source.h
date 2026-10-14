#ifndef PHYSICS_SIM_VERTICAL_SOURCE_H
#define PHYSICS_SIM_VERTICAL_SOURCE_H
#include <stdbool.h>
/* Extensive lower-1/8 support; horizontal samples stay unchanged. */
bool physics_sim_vertical_source(const double *support, int horizontal,
                                int vertical, double *full);
#endif
