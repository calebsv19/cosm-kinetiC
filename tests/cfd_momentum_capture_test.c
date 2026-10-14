#include "../src/tools/physics_sim_momentum_capture.h"
#include "app/cfd_open_momentum_diagnostics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==2);CfdMemoryBudget budget={.limit_bytes=100000000};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);CfdOpenAtmosphere3d s,t;
    int n[3]={8,8,16};double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1));
    s.ground=true;s.muscl=true;s.cache_projection=true;assert(cfd_open_atmosphere3d_lateral(&s));
    double source[2048]={0};source[36]=.00001;source[1024+36]=1e-11;
    for(int i=0;i<2;i++)assert(cfd_open_atmosphere3d_step(&s,source,source+1024));
    uint64_t before=physics_sim_accepted_fingerprint(&s);
    assert(physics_sim_momentum_capture(&s,source,argv[1],-1,-1,before,&budget));
    char path[4096];snprintf(path,sizeof path,"%s.accepted.bin",argv[1]);
    assert(physics_sim_accepted_load(path,&t));assert(physics_sim_accepted_fingerprint(&t)==before);
    assert(cfd_open_atmosphere3d_step(&s,source,source+1024));
    assert(cfd_open_atmosphere3d_step(&t,source,source+1024));
    assert(physics_sim_accepted_fingerprint(&s)==physics_sim_accepted_fingerprint(&t));
    before=physics_sim_accepted_fingerprint(&s);s.projection_ground=false;
    assert(!cfd_open_atmosphere3d_step(&s,source,source+1024));
    assert(!strcmp(cfd_open_momentum_diagnostics.check,"pressure_cache_boundary"));
    assert(physics_sim_accepted_fingerprint(&s)==before);s.projection_ground=true;
    memset(s.velocity,0,s.velocity_count*sizeof(double));
    for(int q=0;q<s.grid.count;q++)s.velocity[q]=100;
    for(int k=0;k<16;k++)for(int j=0;j<8;j++)s.velocity[cfd_open_atmosphere3d_face(&s,0,8,j,k)]=100;
    before=physics_sim_accepted_fingerprint(&s);
    assert(!cfd_open_atmosphere3d_step(&s,source,source+1024));
    assert(!strcmp(cfd_open_momentum_diagnostics.check,"momentum_cfl"));
    assert(cfd_open_momentum_diagnostics.courant>.4 && !cfd_open_momentum_diagnostics.pressure_attempted);
    assert(physics_sim_accepted_fingerprint(&s)==before);
    snprintf(path,sizeof path,"%s.failure",argv[1]);assert(physics_sim_momentum_capture(&s,source,path,30,17,before,&budget));
    cfd_open_atmosphere3d_destroy(&s);cfd_open_atmosphere3d_destroy(&t);
    /* Large imposed pressure makes the absolute divergence gate observable
     * despite a relatively converged pressure solve; attribution-only fixture. */
    double pressure[2]={0,1e12};memset(source,0,sizeof source);
    assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,pressure,ambient,zero,true,9.81,1./300,.1));
    s.ground=true;s.muscl=true;s.cache_projection=true;assert(cfd_open_atmosphere3d_lateral(&s));
    before=physics_sim_accepted_fingerprint(&s);
    assert(!cfd_open_atmosphere3d_step(&s,source,source+1024));
    assert(!strcmp(cfd_open_momentum_diagnostics.check,"projection_divergence"));
    assert(cfd_open_momentum_diagnostics.maximum_divergence>=1e-8);
    assert(physics_sim_accepted_fingerprint(&s)==before);cfd_open_atmosphere3d_destroy(&s);
    CfdCartesian3d g;int size[3]={4,4,4};bool periodic[3]={false,false,false};
    assert(cfd_cartesian3d_init(&g,size,l));CfdCartesian3dLinear *solver=cfd_cartesian3d_linear_create(&g,periodic,0,1,false);assert(solver);
    double rhs[64]={0},x[64]={0},residual=NAN;int iterations=-1;rhs[0]=NAN;
    cfd_open_momentum_diagnostics_begin();assert(!cfd_cartesian3d_linear_solve(solver,rhs,x,&iterations,&residual));
    assert(!strcmp(cfd_open_momentum_diagnostics.linear_failure,"preconditioned_dot_nonpositive_or_nonfinite"));
    cfd_cartesian3d_linear_destroy(solver);cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("capture: accepted-state exact roundtrip/continued-step parity, cache/CFL/CG/divergence attribution, refused-state preservation and allocation closure passed");return 0;
}
