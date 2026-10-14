#include "../src/tools/physics_sim_prescribed_flow.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void init(CfdOpenAtmosphere3d *s,int side,double diff) {
    int n[3]={side,side,side};double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,diff,.005,z,t,z,true,9.81,1./300,.1));
    s->ground=true;s->muscl=true;s->cache_projection=true;
    assert(cfd_open_atmosphere3d_lateral(s));
}
static void equal(const CfdOpenAtmosphere3d *a,const CfdOpenAtmosphere3d *b) {
    assert(!memcmp(a->velocity,b->velocity,a->velocity_count*sizeof(double)));
    assert(!memcmp(a->pressure,b->pressure,3*a->grid.count*sizeof(double)));
    assert(!memcmp(a->flux,b->flux,4*a->boundary_faces*sizeof(double)));
    assert(a->time==b->time && a->steps==b->steps && a->input_j==b->input_j && a->input_kg==b->input_kg);
    assert(a->scalar_work_cells==b->scalar_work_cells && a->divergence==b->divergence && a->residual==b->residual);
}
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=20000000};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d a,b,f;init(&a,8,.0005);init(&b,8,.0005);init(&f,16,.0005);
    double heat[512],smoke[512],forcing[4096],zero[512]={0};
    for(int q=0;q<512;q++){heat[q]=(q%8>=2 && q%8<6 && q<64)?.001:0;smoke[q]=heat[q]*1e-6;}
    /* Nonzero evolving fields: the scoped hook with its own old thermal state
     * must reproduce all accepted fields/counters exactly, step by step. */
    for(int t=0;t<12;t++) {
        assert(cfd_open_atmosphere3d_step(&a,heat,smoke));
        assert(prescribed_flow_step(&b,b.energy,heat,smoke));equal(&a,&b);
    }
    assert(prescribed_flow_prolong(&a,&f,forcing));
    for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int i=0;i<8;i++) {
        double sum=0;
        double parent=a.energy[(k*8+j)*8+i];
        for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++) {
            double child=forcing[(((2*k+z)*16+2*j+y)*16+2*i+x)];
            assert(child==parent/8);sum+=child;
        }
        assert(fabs(sum-parent)<=2e-15*fmax(parent,1e-300));
    }
    /* Prescribed buoyancy is external forcing, not scalar energy injection. */
    cfd_open_atmosphere3d_destroy(&b);init(&b,8,.0005);
    for(int q=0;q<512;q++)forcing[q]=b.rho*b.cp*b.grid.volume*10;
    double *accepted=b.energy;
    assert(prescribed_flow_step(&b,forcing,zero,zero));assert(b.energy==accepted);
    double speed=0;
    for(int q=0;q<b.velocity_count;q++)speed=fmax(speed,fabs(b.velocity[q]));assert(speed>0);
    for(int q=0;q<1024;q++)assert(b.energy[q]==0);
    assert(b.input_j==0 && b.input_kg==0);
    /* Admission and post-momentum scalar refusal preserve accepted state and
     * restore the energy pointer; a later ordinary step still works. */
    forcing[0]=NAN;assert(!prescribed_flow_step(&b,forcing,zero,zero));assert(b.steps==1 && b.energy==accepted);
    forcing[0]=b.rho*b.cp*b.grid.volume*10;
    cfd_open_atmosphere3d_destroy(&b);init(&b,8,1);accepted=b.energy;
    assert(!prescribed_flow_step(&b,forcing,zero,zero));assert(b.energy==accepted && b.steps==0);
    for(int q=0;q<b.velocity_count;q++)assert(b.velocity[q]==0);
    for(int q=0;q<3*b.grid.count;q++)assert(b.pressure[q]==0);
    b.diffusivity=.0005;assert(cfd_open_atmosphere3d_step(&b,zero,zero));
    for(int q=0;q<b.velocity_count;q++)assert(b.velocity[q]==0);
    assert(prescribed_flow_step(&b,forcing,zero,zero));
    assert(!prescribed_flow_prolong(&a,&a,forcing));
    cfd_open_atmosphere3d_destroy(&a);cfd_open_atmosphere3d_destroy(&b);cfd_open_atmosphere3d_destroy(&f);
    cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("prescribed flow: exact evolving control, conservative forcing prolongation, no scalar injection, refusal restoration and allocation closure passed");
    return 0;
}
