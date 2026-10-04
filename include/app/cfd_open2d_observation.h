#ifndef PHYSICS_SIM_CFD_OPEN2D_OBSERVATION_H
#define PHYSICS_SIM_CFD_OPEN2D_OBSERVATION_H
#include "app/cfd_open2d_force_check.h"
#include <json-c/json.h>
bool cfd_open2d_body_check(const CfdOpen2D *c,CfdMac2DForceCheck *out);
void cfd_open2d_snapshot(const CfdOpen2D *c,struct json_object *out,bool full,bool rate_valid,double momentum_rate);
struct json_object *cfd_open2d_sample(const CfdOpen2D *c,struct json_object *request);
#endif
