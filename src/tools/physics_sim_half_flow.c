#include "physics_sim_half_flow.h"
#include <math.h>
#include <limits.h>
bool physics_sim_half_flow(CfdOpenAtmosphere3d *s,double *source) {
    if(!s || !source || s->dt!=.005 || s->steps<0 || s->steps>5000 ||
       !cfd_open_atmosphere3d_valid(s) || s->time!=(2*s->steps)*.0025)return false;
    for(int q=0;q<2*s->grid.count;q++)
        if(!isfinite(source[q]) || source[q]<0 || (source[q]>0 && source[q]*.5==0))return false;
    for(int q=0;q<2*s->grid.count;q++)source[q]*=.5;
    s->dt=.0025;s->steps*=2;
    return true;
}
