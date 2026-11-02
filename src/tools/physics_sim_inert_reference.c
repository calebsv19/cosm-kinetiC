/* Small reproducible inert-box thermal cases with separate SI ledgers. */
#include "physics_sim_inert_coupling.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=5)return 2;
    int side=atoi(argv[1]),mode=atoi(argv[3]);double dt=strtod(argv[2],NULL);
    if((side!=8 && side!=16 && side!=32 && side!=64) || mode<0 || mode>6 ||
       !isfinite(dt) || dt<=0 || dt>.01)return 2;
    int ticks=(int)llround(1.2/dt);if(ticks<1 || ticks>10000 || fabs(ticks*dt-1.2)>1e-12)return 2;
    CfdMemoryBudget memory={.limit_bytes=512*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    CfdOpenAtmosphere3d s={0};double *e=NULL,*zero=NULL;FILE *csv=NULL;int status=1;
    int n[3]={side,side,side},lo[3]={3*side/8,3*side/8,3*side/8},hi[3]={5*side/8,5*side/8,5*side/8};
    double l[3]={1,1,1},p[2]={0},t[2]={300,300};
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,dt,p,t,p,true,9.81,1./300,.1))goto done;
    s.ground=true;s.cache_projection=true;
    if(mode>=2) {
        if(!cfd_open_atmosphere3d_lateral(&s))goto done;
        int knots[3]={0,(int)llround(.2/dt),(int)llround(.6/dt)};double speeds[3]={0,.5,0};
        if(!cfd_open_wind_configure(&s,0,.25,false,3,knots,speeds))goto done;
        s.wind.quiescent_backflow=true;
    }
    if(!cfd_open_atmosphere3d_box(&s,lo,hi))goto done;
    int cooling=mode%2;
    double cg=s.rho*s.cp*s.grid.volume;
    if(!cooling)for(int q=0;q<s.grid.count;q++)if(!cfd_open_atmosphere3d_solid(&s,q))s.energy[q]=5*cg;
    s.initial_j=cfd_passive3d_total(s.energy,s.grid.count);
    PhysicsSimHeatedFace face={0,1,cooling?305:300,10,(mode==4 || mode==5)?0:2,0,0};
    double initial_solid=10*(face.temperature_k-300),initial_gas=s.initial_j;
    e=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));zero=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));if(!e||!zero)goto done;
    csv=fopen(argv[4],"wx");if(!csv)goto done;
    fprintf(csv,"time_s,surface_k,gas_j,gas_to_surface_j,outflow_j,input_j,balance_j\n");
    /* Prove after-flow rollback before producing any accepted history row. */
    size_t count=(size_t)2*s.velocity_count+9*(size_t)s.grid.count+8*(size_t)s.boundary_faces;
    double *copy=cfd_memory_calloc(count,sizeof(double));if(!copy)goto done;
    memcpy(copy,s.storage,count*sizeof(double));PhysicsSimHeatedFace before=face;
    if(physics_sim_inert_coupling_step(&s,&face,e,zero,.125,true) ||
       memcmp(copy,s.storage,count*sizeof(double)) || memcmp(&before,&face,sizeof face) || s.steps!=0) {cfd_memory_free(copy);goto done;}
    cfd_memory_free(copy);
    double max_balance=0,analytic_error=0;
    for(int step=0;step<ticks;step++) {
        if(mode<2) {
            if(!physics_sim_heated_face_exchange_depth(&s,&face,dt,.125))goto done;
        } else {
            /* Warm-source and no-source pairs share the same inert boundary.
             * Initial warm gas gives a resolved non-faint heating signal. */
            double source=(!cooling && mode!=6 && step*dt<.4)?dt*.5:0;
            e[1+side*(1+side)]=source;
            if(!physics_sim_inert_coupling_step(&s,&face,e,zero,.125,false)) {
                fprintf(stderr,"inert step%d failed\n",step);goto done;
            }
        }
        double gas=cfd_passive3d_total(s.energy,s.grid.count),out=0;
        for(int q=0;q<s.boundary_faces;q++)out+=s.flux[s.boundary_faces+q]-s.flux[q];
        double surface=(face.temperature_k-300)+face.temperature_correction_k;
        double balance=gas+10*surface+out-initial_gas-initial_solid-s.input_j;
        max_balance=fmax(max_balance,fabs(balance));
        if(fabs(10*surface-initial_solid-face.gas_to_surface_j)>1e-9)goto done;
        if(mode<2) {
            double capacity=1.2*1005*.0625*.125;
            double difference=5*exp(-.125*(1/capacity+1./10)*(step+1)*dt);
            double exact=(cooling?50:5*capacity)+(cooling?capacity:-capacity)*difference;
            exact/=capacity+10;
            analytic_error=fmax(analytic_error,fabs(surface-exact));
        }
        if(step==0 || (step+1)%((ticks/24)>0?ticks/24:1)==0 || step+1==ticks)
            fprintf(csv,"%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",(step+1)*dt,300+surface,gas,face.gas_to_surface_j,out,s.input_j,balance);
    }
    printf("{\"grid\":%d,\"mode\":%d,\"dt_s\":%.17g,\"surface_k\":%.17g,\"gas_to_surface_j\":%.17g,\"max_balance_j\":%.17g,\"analytic_max_error_k\":%.17g,\"rollback_passed\":true,\"peak_bytes\":%zu,\"support_depth_m\":0.125,\"h_w_m2_k\":%.17g,\"capacity_j_k\":10}\n",side,mode,dt,face.temperature_k+face.temperature_correction_k,face.gas_to_surface_j,max_balance,analytic_error,memory.peak_bytes,face.h_w_m2_k);
    if(max_balance>1e-7 || (mode<2 && analytic_error>1e-5))goto done;
    status=0;
done:
    if(csv)fclose(csv);
    cfd_memory_free(e);cfd_memory_free(zero);cfd_open_atmosphere3d_destroy(&s);
    if(memory.live_bytes)status=1;
    cfd_memory_scope(prior);return status;
}
