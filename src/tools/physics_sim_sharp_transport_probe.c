/* Steady sharp interior source with prescribed uniform native velocity.
 * Thin column; no buoyancy or obstacle; exact physical sampling support. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double slab(const CfdOpenAtmosphere3d *s,double lo,double hi){
    double sum=0,h=s->grid.h[2];
    for(int k=0;k<s->grid.n[2];k++){
        double overlap=fmax(0,fmin(hi,(k+1)*h)-fmax(lo,k*h));
        sum+=s->energy[k*s->plane]/(1206*s->grid.volume)*overlap;
    }
    return sum/(hi-lo);
}
static void observe(const CfdOpenAtmosphere3d *s){
    printf("{\"time_s\":%.17g,\"near_k\":%.17g,\"tail_k\":%.17g}\n",s->time,slab(s,11./32,12./32),slab(s,14./32,15./32));
}
int main(int argc,char **argv){
    /* Preserve completed observations when a bounded diagnostic times out. */
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc!=6)return 2;
    int nz=atoi(argv[1]),steps=atoi(argv[3]);double length=atof(argv[2]),conductivity=atof(argv[4]);
    if((nz!=32 && nz!=64 && nz!=128 && nz!=256 && nz!=512 && nz!=1024) || (length!=1 && length!=.5) || (steps!=2560 && steps!=5120) ||
       (conductivity!=0 && conductivity!=.026) || (strcmp(argv[5],"mc") && strcmp(argv[5],"minmod")))return 2;
    int n[3]={4,4,nz};double l[3]={1,1,length},zero[2]={0,0},ambient[2]={300,300},dt=3.2/steps;
    CfdMemoryBudget b={.limit_bytes=16777216};CfdMemoryBudget *prior=cfd_memory_scope(&b);CfdOpenAtmosphere3d s;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,conductivity,0,dt,zero,ambient,zero,false,0,0,.1))return 3;
    s.max_scalar_work_cells=200000000;s.scalar_rk2=true;s.muscl=true;s.scalar_mc=!strcmp(argv[5],"mc");s.cache_projection=true;
    double *source=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));if(!source)return 3;
    for(int k=0;k<nz;k++)for(int q=0;q<s.plane;q++){
        double overlap=fmax(0,fmin(.25,(k+1)*s.grid.h[2])-fmax(.125,k*s.grid.h[2]));
        source[k*s.plane+q]=1206*s.grid.volume*dt*overlap/s.grid.h[2];
    }
    for(int q=0;q<s.grid.count+s.plane;q++)s.velocity[2*s.grid.count+q]=.05;
    observe(&s);
    for(int step=0;step<steps;step++){
        if(!cfd_open_atmosphere3d_step(&s,source,source+s.grid.count)){fprintf(stderr,"%d: %s\n",step,s.step_rejection);return 4;}
        if((step+1)%(steps/128)==0)observe(&s);
    }
    if(s.scalar_work_cells!=(unsigned long long)steps*s.grid.count*2)return 6;
    double velocity_error=0;
    for(int a=0;a<3;a++)for(int q=0;q<s.grid.count+(a==2?s.plane:0);q++)velocity_error=fmax(velocity_error,fabs(s.velocity[a*s.grid.count+q]-(a==2?.05:0)));
    double balance=cfd_passive3d_total(s.energy,s.grid.count)-s.input_j-cfd_passive3d_total(s.flux,s.boundary_faces)+cfd_passive3d_total(s.flux+s.boundary_faces,s.boundary_faces);
    printf("{\"terminal\":true,\"n\":%d,\"length_m\":%.17g,\"time_s\":%.17g,\"steps\":%d,\"dt_s\":%.17g,\"conductivity\":%.17g,\"reconstruction\":\"%s\",\"scalar_rk2\":true,\"heat_balance_j\":%.17g,\"velocity_error\":%.17g,\"profile_k\":[",nz,length,s.time,steps,dt,conductivity,argv[5],balance,velocity_error);
    for(int k=0;k<nz;k++)printf("%s%.17g",k?",":"",s.energy[k*s.plane]/(1206*s.grid.volume));
    printf("],\"memory_peak_bytes\":%zu",b.peak_bytes);
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf(",\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",b.live_bytes,b.rejected_allocations);
    return b.live_bytes || b.rejected_allocations || fabs(balance)>1e-8 || velocity_error>1e-12?5:0;
}
