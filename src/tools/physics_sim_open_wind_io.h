#ifndef PHYSICS_SIM_OPEN_WIND_IO_H
#define PHYSICS_SIM_OPEN_WIND_IO_H
#include "app/cfd_open_atmosphere3d.h"
#include <json-c/json.h>
bool physics_open_wind_load(CfdOpenAtmosphere3d *s,struct json_object *request);
bool physics_open_wind_restore(CfdOpenAtmosphere3d *s,struct json_object *state);
void physics_open_wind_fields(const CfdOpenAtmosphere3d *s,struct json_object *fields,struct json_object *state);
#endif
