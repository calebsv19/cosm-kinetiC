/* Roundoff-only global closure at a free pressure outlet, before divergence
 * validation. The prescribed inlet is untouched. Larger imbalance refuses.
 * Both the existing pressure and air-volume tolerances remain unchanged. */
#include "app/cfd_open_atmosphere3d.h"
#include <float.h>
#include <math.h>
static void totals(const CfdOpenAtmosphere3d *s,double *in,double *out,long double *net) {
    *in=0;*out=0;*net=0;
    const int *n=s->grid.n;
    for(int axis=0;axis<3;axis++)for(int side=0;side<2;side++) {
        if(axis==2 && side==0)continue;
        int b=(axis+1)%3,c=(axis+2)%3;
        for(int u=0;u<n[b];u++)for(int v=0;v<n[c];v++) {
            int x[3]={0};x[axis]=side?n[axis]:0;x[b]=u;x[c]=v;
            double amount=(side?1:-1)*s->candidate_velocity[cfd_open_atmosphere3d_face(s,axis,x[0],x[1],x[2])]*s->dt*s->grid.h[b]*s->grid.h[c];
            *in+=fmax(-amount,0);*out+=fmax(amount,0);*net+=(long double)amount;
        }
    }
}
bool cfd_open_wind_roundoff_balance(CfdOpenAtmosphere3d *s) {
    if(!s->wind.confined && !s->wind.balanced)return true;
    double incoming,outgoing;long double net;totals(s,&incoming,&outgoing,&net);
    if(fabs(incoming-outgoing)<=1e-10*fmax(incoming+outgoing,1e-8))return true;
    double speed=0;
    for(int q=0;q<s->velocity_count;q++)speed=fmax(speed,fmax(fabs(s->velocity[q]),fabs(s->candidate_velocity[q])));
    double lx=s->grid.length[0],ly=s->grid.length[1],lz=s->grid.length[2];
    double area=2*ly*lz+2*lx*lz+lx*ly;
    double allowance=64*DBL_EPSILON*speed*area*s->dt;
    if(!isfinite(incoming) || !isfinite(outgoing) || !isfinite((double)net) || fabsl(net)>allowance)return false;
    int axis=s->wind.confined?0:2,side=s->wind.confined?!s->wind.side:1;
    int b=(axis+1)%3,c=(axis+2)%3;
    double outlet_area=s->grid.length[b]*s->grid.length[c];
    double correction=(double)net/(s->dt*outlet_area)*(side?1:-1);
    for(int u=0;u<s->grid.n[b];u++)for(int v=0;v<s->grid.n[c];v++) {
        int x[3]={0};x[axis]=side?s->grid.n[axis]:0;x[b]=u;x[c]=v;
        s->candidate_velocity[cfd_open_atmosphere3d_face(s,axis,x[0],x[1],x[2])]-=correction;
    }
    totals(s,&incoming,&outgoing,&net);
    return isfinite(incoming) && isfinite(outgoing) && fabs(incoming-outgoing)<=1e-10*fmax(incoming+outgoing,1e-8);
}
