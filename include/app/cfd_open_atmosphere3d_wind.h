#ifndef PHYSICS_SIM_OPEN_ATMOSPHERE_WIND_H
#define PHYSICS_SIM_OPEN_ATMOSPHERE_WIND_H
#include <stdbool.h>
#include "app/cfd_cartesian3d.h"
struct CfdOpenAtmosphere3d;
/* Absolute native step indices; configuration, not mutable checkpoint state. */
typedef struct {
    bool enabled, linear, confined, balanced;
    /* Explicit wind momentum policy. False preserves legacy ghost donors. */
    bool quiescent_backflow;
    /* Opt-in full-aperture half-channel reference: no-slip ground, slip top.
     * speed is the top/peak speed, u(z)=speed*(2z/H-(z/H)^2). */
    bool half_channel;
    int aperture_lo[2], aperture_hi[2];
    int side, count, step[16];
    double speed[16], height;
} CfdOpenAtmosphereWind;
bool cfd_open_wind_configure(struct CfdOpenAtmosphere3d *s,int side,double height,
    bool linear,int count,const int *steps,const double *speeds);
double cfd_open_wind_command(const struct CfdOpenAtmosphere3d *s,int step);
double cfd_open_wind_profile(const struct CfdOpenAtmosphere3d *s,int k,int step);
bool cfd_open_wind_neumann(const struct CfdOpenAtmosphere3d *s,int axis,int side);
void cfd_open_wind_predictor(struct CfdOpenAtmosphere3d *s);
bool cfd_open_wind_volume(const struct CfdOpenAtmosphere3d *s,double totals[3]);
/* Channel walls constrain normal flow and retain slip tangential predictors. */
bool cfd_open_wind_patch(struct CfdOpenAtmosphere3d *s,const int lo[2],const int hi[2]);
bool cfd_open_wind_half_channel(struct CfdOpenAtmosphere3d *s);
bool cfd_open_wind_balanced(struct CfdOpenAtmosphere3d *s,double height,bool linear,
    int count,const int *steps,const double *speeds);
double cfd_open_wind_profile_face(const struct CfdOpenAtmosphere3d *s,int j,int k,int step);
void cfd_open_wind_closed_predictor(struct CfdOpenAtmosphere3d *s);
bool cfd_open_wind_state_valid(const struct CfdOpenAtmosphere3d *s);
bool cfd_open_wind_pressure_refine(struct CfdOpenAtmosphere3d *s,CfdCartesian3dLinear *solver,double *residual);
bool cfd_open_wind_roundoff_balance(struct CfdOpenAtmosphere3d *s);
#endif
