/* Opt-in normal X inlet. Pressure correction cannot alter prescribed faces.
 * Tangential predictors retain the ambient zero-gradient policy; the ground
 * retains its no-slip ghost. No interior velocity is reset by this module. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <string.h>

bool cfd_open_wind_configure(CfdOpenAtmosphere3d *s,int side,double height,
    bool linear,int count,const int *steps,const double *speeds) {
    if(!s || !s->storage || !s->ground || !s->lateral_open || s->steps ||
       s->wind.enabled || s->projection || s->solid_projection || side<0 || side>1 ||
       !isfinite(height) || height<s->grid.h[2] || height>s->grid.length[2] ||
       count<1 || count>16 || !steps || !speeds || steps[0]!=0)return false;
    for(int i=0;i<count;i++)if(steps[i]<0 || steps[i]>10000 ||
        (i && steps[i]<=steps[i-1]) || !isfinite(speeds[i]) || speeds[i]<0 || speeds[i]>10)return false;
    s->wind=(CfdOpenAtmosphereWind){.enabled=true,.linear=linear,.side=side,.height=height,.count=count};
    memcpy(s->wind.step,steps,(size_t)count*sizeof(int));
    memcpy(s->wind.speed,speeds,(size_t)count*sizeof(double));return true;
}
double cfd_open_wind_command(const CfdOpenAtmosphere3d *s,int step) {
    if(!s->wind.enabled)return 0;
    const CfdOpenAtmosphereWind *w=&s->wind;int i=0;
    while(i+1<w->count && step>=w->step[i+1])i++;
    double speed=w->speed[i];
    if(w->linear && i+1<w->count)speed+=(w->speed[i+1]-speed)*
        ((double)(step-w->step[i])/(w->step[i+1]-w->step[i]));
    return w->side?-speed:speed;
}
double cfd_open_wind_profile(const CfdOpenAtmosphere3d *s,int k,int step) {
    if(s->wind.half_channel) {
        double z=(k+.5)*s->grid.h[2]/s->grid.length[2];
        return cfd_open_wind_command(s,step)*z*(2-z);
    }
    return cfd_open_wind_command(s,step)*fmin(1,((k+.5)*s->grid.h[2])/s->wind.height);
}
bool cfd_open_wind_neumann(const CfdOpenAtmosphere3d *s,int axis,int side) {
    return s->wind.enabled && ((axis==0 && (s->wind.balanced || side==(s->wind.side?1:-1))) ||
        (s->wind.confined && (axis==1 || (axis==2 && side>0))));
}
void cfd_open_wind_predictor(CfdOpenAtmosphere3d *s) {
    if(!s->wind.enabled)return;
    for(int side=0;side<2;side++) {
        if(!s->wind.balanced && side!=s->wind.side)continue;
        int i=side?s->grid.n[0]:0;
        for(int k=0;k<s->grid.n[2];k++)for(int j=0;j<s->grid.n[1];j++)
            s->candidate_velocity[cfd_open_atmosphere3d_face(s,0,i,j,k)]=cfd_open_wind_profile_face(s,j,k,s->steps);
    }
    cfd_open_wind_closed_predictor(s);
}
bool cfd_open_wind_volume(const CfdOpenAtmosphere3d *s,double totals[3]) {
    totals[0]=s->wind_air_in;totals[1]=s->wind_air_out;totals[2]=s->wind_inlet_volume;
    if(!s->wind.enabled)return true;
    double incoming=0,outgoing=0,inlet=0;
    const int *n=s->grid.n;
    for(int a=0;a<3;a++)for(int side=0;side<2;side++) {
        if(a==2 && side==0)continue;
        int b=(a+1)%3,c=(a+2)%3;
        for(int u=0;u<n[b];u++)for(int v=0;v<n[c];v++) {
            int p[3]={0};p[a]=side?n[a]:0;p[b]=u;p[c]=v;
            int face=cfd_open_atmosphere3d_face(s,a,p[0],p[1],p[2]);
            double amount=(side?1:-1)*s->candidate_velocity[face]*s->dt*s->grid.h[b]*s->grid.h[c];
            if(!isfinite(amount))return false;
            incoming+=fmax(-amount,0);outgoing+=fmax(amount,0);
            if(a==0 && s->wind.balanced)inlet+=fmax(-amount,0);
            else if(a==0 && side==s->wind.side)inlet-=amount;
        }
    }
    if(fabs(incoming-outgoing)>1e-10*fmax(incoming+outgoing,1e-8) || inlet<0)return false;
    totals[0]+=incoming;totals[1]+=outgoing;totals[2]+=inlet;
    return isfinite(totals[0]) && isfinite(totals[1]) && isfinite(totals[2]);
}
