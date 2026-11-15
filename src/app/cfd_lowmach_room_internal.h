#ifndef PHYSICS_SIM_LOW_MACH_ROOM_INTERNAL_H
#define PHYSICS_SIM_LOW_MACH_ROOM_INTERNAL_H
#include "app/cfd_lowmach_room.h"
int cfd_lm_lookup(const CfdLmRoom *s, int axis, const int xyz[3]);
bool cfd_lm_project(const CfdLmRoom *s, double dt, const double *target,
                    double *velocity, double *pressure, CfdLmReceipt *receipt);
void cfd_lm_predict(const CfdLmRoom *s, double dt, double *velocity);
#endif
