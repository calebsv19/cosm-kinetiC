/* Optional immutable patch/channel and balanced-throughflow configuration.
 * Prescribed boundaries constrain only normal flow. Side/top walls are slip;
 * the existing ground retains its no-slip ghost. No wall drag is inferred. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <string.h>

bool cfd_open_wind_patch(CfdOpenAtmosphere3d *s,const int lo[2],const int hi[2]) {
    if(!s || !s->wind.enabled || s->steps || s->projection || s->solid_projection ||
       s->wind.balanced || s->wind.confined || !lo || !hi ||
       s->pressure_boundary[0]!=0 || s->pressure_boundary[1]!=0)return false;
    for(int a=0;a<2;a++)if(lo[a]<0 || hi[a]<=lo[a] || hi[a]>s->grid.n[a+1])return false;
    s->wind.confined=true;
    memcpy(s->wind.aperture_lo,lo,2*sizeof(int));
    memcpy(s->wind.aperture_hi,hi,2*sizeof(int));return true;
}

bool cfd_open_wind_balanced(CfdOpenAtmosphere3d *s,double height,bool linear,
    int count,const int *steps,const double *speeds) {
    if(!s || count<1 || count>16 || !speeds || s->pressure_boundary[0]!=0 || s->pressure_boundary[1]!=0)return false;
    double positive[16];
    for(int i=0;i<count;i++) {
        if(!isfinite(speeds[i]) || fabs(speeds[i])>10)return false;
        positive[i]=fabs(speeds[i]);
    }
    if(!cfd_open_wind_configure(s,0,height,linear,count,steps,positive))return false;
    s->wind.balanced=true;memcpy(s->wind.speed,speeds,(size_t)count*sizeof(double));return true;
}

bool cfd_open_wind_half_channel(CfdOpenAtmosphere3d *s) {
    if(!s || !s->wind.enabled || !s->wind.confined || s->wind.balanced ||
       s->steps || s->projection || s->solid_projection || s->wind.half_channel ||
       s->wind.height!=s->grid.length[2])return false;
    for(int a=0;a<2;a++)if(s->wind.aperture_lo[a]!=0 ||
        s->wind.aperture_hi[a]!=s->grid.n[a+1])return false;
    s->wind.half_channel=true;return true;
}

double cfd_open_wind_profile_face(const CfdOpenAtmosphere3d *s,int j,int k,int step) {
    if(s->wind.confined && (j<s->wind.aperture_lo[0] || j>=s->wind.aperture_hi[0] ||
        k<s->wind.aperture_lo[1] || k>=s->wind.aperture_hi[1]))return 0;
    return cfd_open_wind_profile(s,k,step);
}

void cfd_open_wind_closed_predictor(CfdOpenAtmosphere3d *s) {
    if(!s->wind.confined)return;
    int nx=s->grid.n[0],ny=s->grid.n[1],nz=s->grid.n[2];
    for(int k=0;k<nz;k++)for(int i=0;i<nx;i++)for(int side=0;side<2;side++)
        s->candidate_velocity[cfd_open_atmosphere3d_face(s,1,i,side?ny:0,k)]=0;
    for(int j=0;j<ny;j++)for(int i=0;i<nx;i++)
        s->candidate_velocity[cfd_open_atmosphere3d_face(s,2,i,j,nz)]=0;
}

bool cfd_open_wind_state_valid(const CfdOpenAtmosphere3d *s) {
    if(!s->wind.enabled)return true;
    if(s->wind.half_channel && (!s->wind.confined || s->wind.balanced ||
        s->wind.height!=s->grid.length[2] || s->wind.aperture_lo[0]!=0 ||
        s->wind.aperture_lo[1]!=0 || s->wind.aperture_hi[0]!=s->grid.n[1] ||
        s->wind.aperture_hi[1]!=s->grid.n[2]))return false;
#ifndef PHYSICS_SIM_FROZEN_TRANSPORT_DIAGNOSTIC
    if(s->steps==0 && (s->wind.confined || s->wind.balanced))
        for(int q=0;q<s->velocity_count;q++)if(s->velocity[q]!=0)return false;
    if(s->steps>0)for(int side=0;side<2;side++) {
#else
    /* Only the explicit fixed-MAC diagnostic may seed an accepted flow field.
     * Its native bridge requires the diagnostic schema and exact fixed array.
     * Enforce the prescribed face even at diagnostic time zero. */
    for(int side=0;side<2;side++) {
#endif
        if(!s->wind.balanced && side!=s->wind.side)continue;
        for(int k=0;k<s->grid.n[2];k++)for(int j=0;j<s->grid.n[1];j++) {
            int face=cfd_open_atmosphere3d_face(s,0,side?s->grid.n[0]:0,j,k);
            int tick=s->steps>0?s->steps-1:0;
            if(s->velocity[face]!=cfd_open_wind_profile_face(s,j,k,tick))return false;
        }
    }
    if(s->wind.confined) {
        for(int k=0;k<s->grid.n[2];k++)for(int i=0;i<s->grid.n[0];i++)for(int side=0;side<2;side++)
            if(s->velocity[cfd_open_atmosphere3d_face(s,1,i,side?s->grid.n[1]:0,k)]!=0)return false;
        for(int j=0;j<s->grid.n[1];j++)for(int i=0;i<s->grid.n[0];i++)
            if(s->velocity[cfd_open_atmosphere3d_face(s,2,i,j,s->grid.n[2])]!=0)return false;
        int offsets[3]={s->plane,2*s->plane+2*s->grid.n[1]*s->grid.n[2],
            2*s->plane+2*s->grid.n[1]*s->grid.n[2]+s->grid.n[0]*s->grid.n[2]};
        int counts[3]={s->plane,s->grid.n[0]*s->grid.n[2],s->grid.n[0]*s->grid.n[2]};
        for(int block=0;block<4;block++)for(int b=0;b<3;b++)for(int q=0;q<counts[b];q++)
            if(s->flux[block*s->boundary_faces+offsets[b]+q]!=0)return false;
    }
    return true;
}
