#ifndef PHYSICS_SIM_OPEN_ATMOSPHERE_WEATHER_H
#define PHYSICS_SIM_OPEN_ATMOSPHERE_WEATHER_H
#include <stdbool.h>
struct CfdOpenAtmosphere3d;
/* App-owned open-domain momentum driver. Strength zero releases the flow;
 * target zero with positive strength instead applies physical braking. */
typedef struct {
    bool enabled, linear;
    int count, step[16];
    double target_m_s[16], strength[16], ramp_height_m, response_s;
} CfdOpenAtmosphereWeather;
typedef struct {
    double external_work_j, air_in_m3, air_out_m3;
} CfdOpenWeatherState;
bool cfd_open_weather_configure(const struct CfdOpenAtmosphere3d *s,
    CfdOpenAtmosphereWeather *out,double height,double response,bool linear,
    int count,const int *steps,const double *targets,const double *strengths);
bool cfd_open_weather_valid(const struct CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w);
bool cfd_open_weather_command(const CfdOpenAtmosphereWeather *w,int step,
    double *target,double *strength);
/* Changes candidate_velocity only, after full validation. Work is the signed
 * discrete MAC kinetic difference of this force substep, excluding projection,
 * advection, viscosity, thermal energy and boundary flux. Caller owns commit. */
bool cfd_open_weather_apply(struct CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,double *work_j);
bool cfd_open_weather_state_valid(const struct CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,const CfdOpenWeatherState *state);
/* Integrate actual projected boundary flow, never a prescribed inlet target.
 * Returns new cumulative volumes without modifying accepted state. */
bool cfd_open_weather_volume(const struct CfdOpenAtmosphere3d *s,
    const CfdOpenWeatherState *accepted,double totals[2]);
#endif
