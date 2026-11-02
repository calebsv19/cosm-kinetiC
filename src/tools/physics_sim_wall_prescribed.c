/* Qualification-only prescribed tangential shear at an impermeable ground.
 * Reuse native transport/admission/commit, replacing only momentum evolution.
 * No claim to a pressure-driven/no-slip momentum solution or force balance. */
#include "app/cfd_open_atmosphere3d.h"
#include <string.h>
static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *,double *,double *);
#define PHYSICS_SIM_OPEN_PRESCRIBED_THERMAL_QUALIFIER 1
#include "../app/cfd_open_atmosphere3d.c"
static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *s,double *div,double *res){
    if(!s->ground)return momentum(s,div,res);
    if(s->solid_box||s->buoyancy||s->lateral_open)return false;
    for(int q=0;q<s->grid.count;q++){
        double expected=.05*(q/s->plane+.5)/s->grid.n[2];
        if(s->velocity[q]!=expected||s->velocity[s->grid.count+q]!=0||s->velocity[2*s->grid.count+q]!=0)return false;
    }
    for(int q=0;q<s->plane;q++)if(s->velocity[3*s->grid.count+q]!=0)return false;
    memcpy(s->candidate_velocity,s->velocity,(size_t)s->velocity_count*sizeof(double));
    memset(s->candidate_pressure,0,(size_t)s->grid.count*sizeof(double));
    *div=*res=0;return true;
}
