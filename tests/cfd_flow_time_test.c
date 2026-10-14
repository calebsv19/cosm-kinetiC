#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void init(CfdOpenAtmosphere3d *s,double dt,double diffusivity) {
    int n[3]={8,8,8};double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,diffusivity,dt,
           zero,ambient,zero,false,9.81,1./300,.1));
    s->ground=true;s->muscl=true;s->cache_projection=true;
    assert(cfd_open_atmosphere3d_lateral(s));
}
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=10000000};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s[3];double dts[3]={.005,.0025,.00125};
    double heat[512],smoke[512];
    for(int a=0;a<3;a++) {
        init(s+a,dts[a],.0005);
        for(int q=0;q<512;q++){heat[q]=dts[a]*.25;smoke[q]=dts[a]*.000001;}
        int steps=(int)round(.02/dts[a]);
        for(int t=0;t<steps;t++)assert(cfd_open_atmosphere3d_step(s+a,heat,smoke));
        assert(s[a].scalar_work_cells==16ULL*512);
        assert(s[a].time==.02);
    }
    /* Same fixed scalar updates/source fractions under zero flow must be
     * bit-identical despite different momentum update counts. */
    for(int a=1;a<3;a++) {
        assert(!memcmp(s[0].energy,s[a].energy,1024*sizeof(double)));
        assert(fabs(s[0].input_j-s[a].input_j)<1e-13*s[0].input_j);
        assert(fabs(s[0].input_kg-s[a].input_kg)<1e-13*s[0].input_kg);
    }
    CfdOpenAtmosphere3d rejected;init(&rejected,.0013,.0005);
    memset(heat,0,sizeof heat);memset(smoke,0,sizeof smoke);
    assert(!cfd_open_atmosphere3d_step(&rejected,heat,smoke));
    assert(rejected.steps==0 && rejected.scalar_work_cells==0 && rejected.input_j==0);
    cfd_open_atmosphere3d_destroy(&rejected);
    /* Fixed timing must refuse a diffusion rate requiring more cycles. */
    init(&rejected,.005,1.);assert(!cfd_open_atmosphere3d_step(&rejected,heat,smoke));
    assert(rejected.steps==0 && rejected.scalar_work_cells==0);
    for(int q=0;q<512;q++)assert(rejected.energy[q]==0 && rejected.smoke[q]==0);
    cfd_open_atmosphere3d_destroy(&rejected);
    for(int a=0;a<3;a++)cfd_open_atmosphere3d_destroy(s+a);
    cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("fixed scalar: exact source/time partition parity, nonintegral timing and unsafe CFL refusal, allocation closure passed");
    return 0;
}
