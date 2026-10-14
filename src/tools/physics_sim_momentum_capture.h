#ifndef PHYSICS_SIM_MOMENTUM_CAPTURE_H
#define PHYSICS_SIM_MOMENTUM_CAPTURE_H
#include "app/cfd_open_atmosphere3d.h"
#include <stdint.h>
uint64_t physics_sim_accepted_fingerprint(const CfdOpenAtmosphere3d *s);
bool physics_sim_momentum_capture(const CfdOpenAtmosphere3d *s,const double *source,
    const char *prefix,int frame,int step,uint64_t before,const CfdMemoryBudget *budget);
bool physics_sim_accepted_load(const char *path,CfdOpenAtmosphere3d *s);
#endif
