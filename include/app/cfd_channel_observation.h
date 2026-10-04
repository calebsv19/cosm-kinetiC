#ifndef PHYSICS_SIM_CFD_CHANNEL_OBSERVATION_H
#define PHYSICS_SIM_CFD_CHANNEL_OBSERVATION_H
#include "app/cfd_channel.h"
#include <json-c/json.h>
void cfd_channel_snapshot(const CfdChannel *c, struct json_object *out);
struct json_object *cfd_channel_sample(const CfdChannel *c, struct json_object *request);
#endif
