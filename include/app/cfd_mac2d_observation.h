#ifndef PHYSICS_SIM_CFD_MAC2D_OBSERVATION_H
#define PHYSICS_SIM_CFD_MAC2D_OBSERVATION_H
#include "app/cfd_mac2d.h"
#include <json-c/json.h>
void cfd_mac2d_snapshot(const CfdMac2D *c, struct json_object *out, bool full);
struct json_object *cfd_mac2d_sample(const CfdMac2D *c, struct json_object *request);
#endif
