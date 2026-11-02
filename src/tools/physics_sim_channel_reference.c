/* Native3D half-Poiseuille reference, evolved from rest. Analytical values
 * are observations only; no interior velocity/pressure is prescribed. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv) {
    if(argc!=4)return 2;
    int nz=atoi(argv[1]),steps=atoi(argv[3]);double dt=strtod(argv[2],NULL);
    if((nz!=8 && nz!=16 && nz!=32 && nz!=64) || steps<1 || steps>10000 ||
       !isfinite(dt) || dt<=0 || dt>.01)return 2;
    int n[3]={4,4,nz},knots[1]={0},lo[2]={0,0},hi[2]={4,nz};
    double l[3]={.25,.25,1},p[2]={0},ambient[2]={300,300},speed[1]={.1};
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    CfdOpenAtmosphere3d s={0};int status=1;double *zero=NULL;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,.12,1005,300,0,0,dt,p,ambient,p,false,0,0,.1))goto done;
    s.ground=true;s.cache_projection=true;
    if(!cfd_open_atmosphere3d_lateral(&s) ||
       !cfd_open_wind_configure(&s,0,1,false,1,knots,speed) ||
       !cfd_open_wind_patch(&s,lo,hi) || !cfd_open_wind_half_channel(&s))goto done;
    zero=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));if(!zero)goto done;
    double last_change=0,max_div=0;
    for(int step=0;step<steps;step++) {
        double before=s.velocity[cfd_open_atmosphere3d_face(&s,0,2,2,nz/2)];
        if(!cfd_open_atmosphere3d_step(&s,zero,zero)) {
            fprintf(stderr,"channel rejected step%d: %s\n",step,s.step_rejection);goto done;
        }
        last_change=fabs(s.velocity[cfd_open_atmosphere3d_face(&s,0,2,2,nz/2)]-before);
        max_div=fmax(max_div,s.divergence);
    }
    double error=0,scale=0,p_error=0,p_scale=0,q=0,dp=0;
    /* For the declared mixed boundaries, G=2mu U/H² and p=G(L-x). */
    double gradient=2*s.mu*speed[0]/(l[2]*l[2]);
    for(int k=0;k<nz;k++) {
        double z=(k+.5)/nz,exact=speed[0]*z*(2-z);
        double u=s.velocity[cfd_open_atmosphere3d_face(&s,0,2,2,k)];
        error+=(u-exact)*(u-exact);scale+=exact*exact;q+=u*l[1]*s.grid.h[2];
        for(int i=0;i<4;i++) {
            double target=gradient*(l[0]-(i+.5)*s.grid.h[0]);
            double pressure=s.pressure[(k*4+2)*4+i];p_error+=(pressure-target)*(pressure-target);p_scale+=target*target;
        }
        dp+=s.pressure[(k*4+2)*4]-s.pressure[(k*4+2)*4+3];
    }
    dp/=nz;dp*=l[0]/(l[0]-s.grid.h[0]);
    printf("{\"grid\":[4,4,%d],\"dt_s\":%.17g,\"steps\":%d,\"time_s\":%.17g,\"velocity_relative_l2\":%.17g,\"pressure_relative_l2\":%.17g,\"flow_m3_s\":%.17g,\"exact_flow_m3_s\":%.17g,\"pressure_drop_pa\":%.17g,\"exact_pressure_drop_pa\":%.17g,\"last_midpoint_velocity_change_m_s\":%.17g,\"max_divergence\":%.17g,\"numerical_peak_bytes\":%zu,\"profile\":[",nz,dt,steps,s.time,sqrt(error/scale),sqrt(p_error/p_scale),q,2*speed[0]*l[1]*l[2]/3,dp,gradient*l[0],last_change,max_div,memory.peak_bytes);
    for(int k=0;k<nz;k++)printf("%s%.17g",k?",":"",s.velocity[cfd_open_atmosphere3d_face(&s,0,2,2,k)]);
    puts("]}");status=0;
done:
    cfd_memory_free(zero);cfd_open_atmosphere3d_destroy(&s);
    if(memory.live_bytes)status=1;
    cfd_memory_scope(prior);return status;
}
