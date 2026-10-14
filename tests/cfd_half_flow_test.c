#include "../src/tools/physics_sim_half_flow.h"
#include "../src/tools/physics_sim_momentum_capture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void init(CfdOpenAtmosphere3d *s) {
    int n[3]={8,8,8};double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1));
    s->ground=true;s->muscl=true;s->cache_projection=true;assert(cfd_open_atmosphere3d_lateral(s));
}
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=20000000};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;init(&s);double source[1024]={0},saved[1024];
    s.steps=2;s.time=.01;s.scalar_work_cells=8ULL*512;
    source[36]=.001;source[512+36]=1e-9;memcpy(saved,source,sizeof saved);
    uint64_t before=physics_sim_accepted_fingerprint(&s);source[1023]=NAN;
    assert(!physics_sim_half_flow(&s,source));assert(physics_sim_accepted_fingerprint(&s)==before);
    assert(source[36]==saved[36] && isnan(source[1023]));source[1023]=0;
    double time=s.time,input=s.input_j;unsigned long long work=s.scalar_work_cells;
    assert(physics_sim_half_flow(&s,source));assert(s.dt==.0025 && s.steps==4 && s.time==time && s.input_j==input && s.scalar_work_cells==work);
    for(int q=0;q<1024;q++)assert(source[q]+source[q]==saved[q]);
    assert(!physics_sim_half_flow(&s,source));
    for(int i=0;i<2;i++)assert(cfd_open_atmosphere3d_step(&s,source,source+512));
    assert(s.steps==6 && s.time==.015 && s.scalar_work_cells==work+4ULL*512);
    assert(fabs(s.input_j-.001)<1e-15 && fabs(s.input_kg-1e-9)<1e-20);
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("half flow: prevalidation/no partial mutation, physical-time/index rebase, exact source halves, fixed scalar cycles, integrated input and allocation closure passed");return 0;
}
