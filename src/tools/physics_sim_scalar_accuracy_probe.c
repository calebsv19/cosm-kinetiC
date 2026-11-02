/* Independent smooth cell-average reference probe: ordinary native scalar and
 * momentum operators, periodic XY, zero vertical flow. No solver duplicate. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=5 && argc!=6)return 2;
    if(argc==6 && strcmp(argv[5],"mc") && strcmp(argv[5],"mc_rk2"))return 2;
    int side=atoi(argv[1]),mode=atoi(argv[3]);double dt=strtod(argv[2],NULL),end=strtod(argv[4],NULL);
    if((side!=16&&side!=32&&side!=64&&side!=128)||mode<0||mode>2||
       (dt!=.005&&dt!=.0025)||(end!=.8&&end!=20))return 2;
    int ticks=(int)llround(end/dt),n[3]={side,4,4};double l[3]={1,1,1},zero[2]={0},ambient[2]={300,300};
    CfdMemoryBudget mem={.limit_bytes=64*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&mem);
    CfdOpenAtmosphere3d s={0};int status=1;double *source=NULL,u=mode==1?0:.05,k=mode==0?0:.026;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,k,k/1206,dt,zero,ambient,zero,false,9.81,1./300,.1))goto done;
    s.muscl=true;s.cache_projection=true;
    s.scalar_mc=argc==6;s.scalar_rk2=argc==6 && !strcmp(argv[5],"mc_rk2");
    source=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));if(!source)goto done;
    double wave=2*acos(-1.),h=1./side,average=sin(wave*h/2)/(wave*h/2);
    for(int q=0;q<s.grid.count;q++) {
        double rise=1+.25*average*cos(wave*((q%side+.5)*h-.25));
        s.energy[q]=1206*s.grid.volume*rise;s.smoke[q]=1e-6*s.grid.volume*rise;
        s.velocity[q]=u;
    }
    s.initial_j=cfd_passive3d_total(s.energy,s.grid.count);s.initial_kg=cfd_passive3d_total(s.smoke,s.grid.count);
    for(int tick=0;tick<ticks;tick++)if(!cfd_open_atmosphere3d_step(&s,source,source)){
        fprintf(stderr,"scalar accuracy tick%d: %s\n",tick,s.step_rejection);goto done;
    }
    double velocity_error=0,in=0,out=0;
    for(int a=0;a<3;a++)for(int q=0;q<s.grid.count+(a==2?s.plane:0);q++)velocity_error=fmax(velocity_error,fabs(s.velocity[a*s.grid.count+q]-(a==0?u:0)));
    for(int q=0;q<s.boundary_faces;q++){in+=s.flux[q];out+=s.flux[s.boundary_faces+q];}
    printf("{\"side\":%d,\"mode\":%d,\"dt_s\":%.17g,\"time_s\":%.17g,\"velocity_m_s\":%.17g,\"alpha_m2_s\":%.17g,\"velocity_error_m_s\":%.17g,\"heat_defect_j\":%.17g,\"smoke_total_kg\":%.17g,\"initial_smoke_kg\":%.17g,\"profile_k\":[",side,mode,dt,s.time,u,k/1206,velocity_error,cfd_passive3d_total(s.energy,s.grid.count)+out-in-s.initial_j,cfd_passive3d_total(s.smoke,s.grid.count),s.initial_kg);
    for(int i=0;i<side;i++)printf("%s%.17g",i?",":"",s.energy[i]/(1206*s.grid.volume));
    printf("],\"profile_smoke_kg_m3\":[");for(int i=0;i<side;i++)printf("%s%.17g",i?",":"",s.smoke[i]/s.grid.volume);printf("]");
    status=0;
done:
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    if(!status)printf(",\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",mem.peak_bytes,mem.live_bytes);
    return mem.live_bytes?4:status;
}
