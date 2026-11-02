#ifndef PHYSICS_SIM_OPEN_WEATHER_IO_H
#define PHYSICS_SIM_OPEN_WEATHER_IO_H
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_open_atmosphere3d_weather.h"
#include <json-c/json.h>
bool physics_open_weather_load(CfdOpenAtmosphere3d *s,
    CfdOpenAtmosphereWeather *weather,struct json_object *request);
bool physics_open_weather_restore(CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *weather,CfdOpenWeatherState *accepted,
    struct json_object *state);
void physics_open_weather_fields(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *weather,const CfdOpenWeatherState *accepted,
    struct json_object *fields,struct json_object *state);
#endif
