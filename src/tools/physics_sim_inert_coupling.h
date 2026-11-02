#ifndef PHYSICS_SIM_INERT_COUPLING_H
#define PHYSICS_SIM_INERT_COUPLING_H
#include "physics_sim_heated_face.h"
/* Development host only. One native flow step plus authored fixed-depth
 * gas/solid Robin exchange, committed together in memory. No wall-law or
 * combustion claim. Flow and exchange clocks must agree exactly. */
bool physics_sim_inert_coupling_step(CfdOpenAtmosphere3d *gas,
    PhysicsSimHeatedFace *face,const double *source_j,const double *source_kg,
    double depth_m,bool fail_after_flow);
#endif
