/* Thin-column reference: the ordinary open-atmosphere operator under constant
 * prescribed translation, with no buoyancy or obstacle. No solver copy. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double primitive(double x,double half) {
    double k=acos(-1.)/(2*half);x=fmax(-half,fmin(half,x));
    return .375*x+sin(2*k*x)/(4*k)+sin(4*k*x)/(32*k);
}
static void observe(const CfdOpenAtmosphere3d *s,int axis) {
    double e=0;int stride=(int[]){1,s->grid.n[0],s->plane}[axis];
    for(int q=0;q<s->grid.count;q++) {
        int c=q/stride%s->grid.n[axis];
        if(c>=11*s->grid.n[axis]/32 && c<12*s->grid.n[axis]/32)e+=s->energy[q];
    }
    printf("{\"time_s\":%.17g,\"slab_rise_k\":%.17g}\n",s->time,e/(1206./32));
}
int main(int argc,char **argv) {
    if(argc!=6 && argc!=7)return 2;
    if(argc==7 && strcmp(argv[6],"mc") && strcmp(argv[6],"mc_rk2") && strcmp(argv[6],"upwind") && strcmp(argv[6],"mc_rk2_upwind"))return 2;
    int side=atoi(argv[1]),axis=atoi(argv[4]);double dt=atof(argv[2]),conductivity=atof(argv[3]),half=atof(argv[5]);
    if((side!=32 && side!=64 && side!=128 && side!=256) || axis<0 || axis>2 ||
       (dt!=.00125 && dt!=.000625 && dt!=.0025) || (conductivity!=0 && conductivity!=.026) ||
       (half!=.0625 && half!=.125))return 2;
    CfdMemoryBudget budget={.limit_bytes=64*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    int n[3]={4,4,4};n[axis]=side;double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,conductivity,0,dt,zero,ambient,zero,false,9.81,1./300,.1));
    s.muscl=true;s.cache_projection=true;
    s.muscl=!(argc==7 && (!strcmp(argv[6],"upwind") || !strcmp(argv[6],"mc_rk2_upwind")));
    s.scalar_mc=argc==7 && strcmp(argv[6],"upwind");
    s.scalar_rk2=argc==7 && (!strcmp(argv[6],"mc_rk2") || !strcmp(argv[6],"mc_rk2_upwind"));
    int count=s.grid.count,stride=(int[]){1,n[0],s.plane}[axis];
    double *source=calloc((size_t)count,sizeof(double));assert(source);
    for(int q=0;q<count;q++) {
        int c=q/stride%side;
        double avg=side*(primitive((c+1.)/side-.1875,half)-primitive((double)c/side-.1875,half));
        s.energy[q]=1206*s.grid.volume*fmax(0,avg);s.initial_j+=s.energy[q];
    }
    for(int q=0;q<count+(axis==2?s.plane:0);q++)s.velocity[axis*count+q]=.05;
    observe(&s,axis);
    int ticks=(int)round(3.2/dt),observe_ticks=(int)round(.025/dt);
    for(int tick=0;tick<ticks;tick++) {
        if(!cfd_open_atmosphere3d_step(&s,source,source)) {fprintf(stderr,"tick %d: %s\n",tick,s.step_rejection);return 3;}
        if((tick+1)%observe_ticks==0)observe(&s,axis);
    }
    double velocity_error=0,total=cfd_passive3d_total(s.energy,count),in=0,out=0;
    for(int a=0;a<3;a++)for(int q=0;q<count+(a==2?s.plane:0);q++)velocity_error=fmax(velocity_error,fabs(s.velocity[a*count+q]-(a==axis?.05:0)));
    for(int q=0;q<s.boundary_faces;q++){in+=s.flux[q];out+=s.flux[s.boundary_faces+q];}
    assert(velocity_error<1e-12 && fabs(total-s.initial_j-in+out)<1e-9);
    printf("{\"terminal\":true,\"side\":%d,\"axis\":%d,\"dt_s\":%.17g,\"conductivity\":%.17g,\"half_width_m\":%.17g,\"velocity_error\":%.17g,\"heat_balance_j\":%.17g,\"profile_k\":[",side,axis,dt,conductivity,half,velocity_error,total-s.initial_j-in+out);
    for(int c=0;c<side;c++)printf("%s%.17g",c?",":"",s.energy[c*stride]/(1206*s.grid.volume));
    free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf("],\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",budget.peak_bytes,budget.live_bytes);
    return budget.live_bytes?4:0;
}
