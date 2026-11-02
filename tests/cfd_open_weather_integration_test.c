/* Apply only with the guarded weather integration after timing acceptance. */
#include "app/cfd_open_atmosphere3d_weather.h"
#include "app/cfd_open_energy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/tools/physics_sim_inert_coupling.h"
#include <stdlib.h>
static void setup(CfdOpenAtmosphere3d *s,bool box) {
    int n[3]={8,8,8};double l[3]={1,1,1},p[2]={0},t[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1e-5,1000,300,0,0,.005,p,t,p,false,0,1./300,.1));
    s->ground=true;assert(cfd_open_atmosphere3d_lateral(s));
    if(box){int lo[3]={3,3,3},hi[3]={5,5,5};assert(cfd_open_atmosphere3d_box(s,lo,hi));}
}
static void activate(CfdOpenAtmosphere3d *s,bool reversal) {
    int steps[3]={0,20,60};double targets[3]={.5,-.5,-.5},strengths[3]={1,1,0};
    assert(cfd_open_weather_configure(s,&s->weather,.125,.1,false,
        reversal?3:1,steps,targets,strengths));
    s->wind.quiescent_backflow=true;s->cache_projection=true;
    s->kinetic_ledger[KE_INITIAL]=cfd_open_kinetic(s,s->velocity);
    assert(cfd_open_atmosphere3d_valid(s));
}
static void first_step(void) {
    CfdOpenAtmosphere3d s={0};setup(&s,false);activate(&s,false);
    double zero[512]={0},alpha=-expm1(-s.dt/.1);
    assert(cfd_open_atmosphere3d_step(&s,zero,zero));
    for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int i=0;i<=8;i++) {
        double expected=.5*fmin(1,k+.5)*alpha;
        assert(fabs(s.velocity[cfd_open_atmosphere3d_face(&s,0,i,j,k)]-expected)<2e-15);
    }
    assert(s.weather_state.external_work_j==cfd_open_kinetic(&s,s.velocity));
    assert(s.kinetic_ledger[KE_INLET]==0 && s.weather_state.air_in_m3>0);
    assert(fabs(s.weather_state.air_in_m3-s.weather_state.air_out_m3)<1e-14);
    cfd_open_atmosphere3d_destroy(&s);
}
static void reversal_and_release(bool box) {
    CfdOpenAtmosphere3d s={0};setup(&s,box);activate(&s,true);double zero[512]={0};
    bool negative=false;double released=0;
    for(int step=0;step<100;step++) {
        double old=s.weather_state.external_work_j;
        assert(cfd_open_atmosphere3d_step(&s,zero,zero));
        assert(cfd_open_atmosphere3d_valid(&s));
        if(step>=20 && s.weather_state.external_work_j<old)negative=true;
        if(step==59)released=s.weather_state.external_work_j;
        if(step>=60)assert(s.weather_state.external_work_j==released);
        assert(s.kinetic_ledger[KE_INLET]==0);
    }
    assert(negative && cfd_open_kinetic(&s,s.velocity)>0);
    cfd_open_atmosphere3d_destroy(&s);
}
static void rollback(void) {
    CfdOpenAtmosphere3d s={0};setup(&s,true);s.buoyancy=true;s.gravity=0;activate(&s,false);
    PhysicsSimHeatedFace object={.axis=0,.side=-1,.temperature_k=300,
        .capacity_j_k=10,.h_w_m2_k=2};
    double zero[512]={0},source[512]={0};CfdOpenWeatherState accepted=s.weather_state;
    size_t bytes=(size_t)s.velocity_count*sizeof(double);double *v=malloc(bytes);assert(v);memcpy(v,s.velocity,bytes);
    assert(!physics_sim_inert_coupling_step(&s,&object,zero,zero,.125,true));
    assert(s.steps==0 && !memcmp(v,s.velocity,bytes));
    assert(!memcmp(&accepted,&s.weather_state,sizeof(accepted)) && s.candidate_weather_work_j==0);
    source[0]=1e9;
    assert(!cfd_open_atmosphere3d_step(&s,source,zero));
    assert(s.step_rejection && !strcmp(s.step_rejection,"temperature contrast applicability"));
    assert(s.steps==0 && !memcmp(v,s.velocity,bytes));
    assert(!memcmp(&accepted,&s.weather_state,sizeof(accepted)));
    assert(physics_sim_inert_coupling_step(&s,&object,zero,zero,.125,false));
    assert(s.steps==1 && s.weather_state.external_work_j>0 && cfd_open_atmosphere3d_valid(&s));
    free(v);cfd_open_atmosphere3d_destroy(&s);
}
int main(void) {
    CfdMemoryBudget memory={.limit_bytes=512*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    first_step();reversal_and_release(false);reversal_and_release(true);rollback();
    assert(memory.live_bytes==0);cfd_memory_scope(prior);
    puts("integrated weather first-step reference, signed work, release, obstacle, scalar/inert rollback: passed");return 0;
}
