/* Bounded gas-only prescribed translation on the actual unequal mesh.
 * Same source, fine clock, scalar pair and observations; no wall or feedback. */
#include "physics_sim_tail_time.h"
#include "physics_sim_source_interval.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static PhysicsSimTailDiffusion diffusion;
static PhysicsSimTailTime rk;
static PhysicsSimSourceInterval source;
static PhysicsSimNonuniformAdvection advection;
static double width[4096],power[4096],v[4096];
static int offset[129];
static void observe(int tick,int method,double u) {
    double transport=0,tail=0,mass=0;
    for(int j=0;j<2112;j++)mass+=v[j]*width[j];
    for(int j=offset[32];j<offset[48];j++)transport+=v[j]*width[j]*32;
    for(int j=offset[48];j<offset[64];j++)tail+=v[j]*width[j]*32;
    printf("{\"method\":%d,\"u_m_s\":%.17g,\"tick\":%d,\"time_s\":%.17g,\"transport_k\":%.17g,\"tail_k\":%.17g,\"edge_cell_k\":%.17g,\"mass_k_m\":%.17g,\"expected_mass_k_m\":%.17g}\n",method,u,tick,tick/51200.,transport,tail,v[offset[32]],mass,tick/(51200.*32));
}
int main(int argc,char **argv) {
    if(argc!=3)return 2;int method=atoi(argv[1]);double u=strtod(argv[2],NULL);
    if((method!=8&&method!=9&&method!=10)||(u!=0&&u!=.025))return 2;
    const double dt=1./51200,alpha=.026/1206;int n=0;
    for(int x=0;x<128;x++) {
        offset[x]=n;int count=x<48||x>=112?32:1;
        for(int k=0;k<count;k++){width[n]=.25/128/count;power[n]=(x>=16&&x<32);n++;}
    }
    offset[128]=n;assert(n==2112);
    assert(physics_sim_tail_diffusion_init(&diffusion,n,width,true));
    assert(physics_sim_nonuniform_advection_init(&advection,n,width,offset[48]));
    assert(physics_sim_source_interval_prepare(&source,n,width,power,alpha,dt,true));
    if(method>=9)assert(physics_sim_tail_heater_source(&diffusion,power,alpha));
    for(int tick=1;tick<=5120;tick++) {
        assert(physics_sim_nonuniform_advection_step(&advection,v,u,dt,method==10?3:2));
        assert(physics_sim_tail_time_step(&diffusion,&rk,v,alpha,dt,2));
        assert(physics_sim_source_interval_add(&source,v,dt));
        for(int j=0;j<n;j++)assert(isfinite(v[j])&&v[j]>=0&&v[j]<=tick*dt+1e-10);
        if(tick==1280||tick==5120)observe(tick,method,u);
    }
}
