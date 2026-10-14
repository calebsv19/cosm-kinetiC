#include "../src/tools/physics_sim_matched_heat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void init(CfdOpenAtmosphere3d *s,int side) {
    int n[3]={side,side,side};double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1));
    s->ground=true;s->muscl=true;s->cache_projection=true;
    assert(cfd_open_atmosphere3d_lateral(s));
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--fine-capacity")) {
        CfdMemoryBudget budget={.limit_bytes=2147483648ULL};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
        CfdOpenAtmosphere3d coarse,fine;init(&coarse,64);init(&fine,128);
        double *zero=cfd_memory_calloc((size_t)fine.grid.count,sizeof(double));assert(zero);
        assert(cfd_open_atmosphere3d_step(&coarse,zero,zero));
        double div,bound;assert(matched_heat_velocity(&coarse,&fine,&div));
        assert(matched_heat_advance(&fine,zero,2,&bound));
        assert(fine.scalar_work_cells==4194304ULL);
        cfd_memory_free(zero);cfd_open_atmosphere3d_destroy(&coarse);cfd_open_atmosphere3d_destroy(&fine);
        cfd_memory_scope(prior);assert(budget.live_bytes==0 && budget.rejected_allocations==0);
        printf("{\"fine_capacity_passed\":true,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",budget.peak_bytes,budget.live_bytes);
        return 0;
    }
    assert(argc==1);
    CfdMemoryBudget b={.limit_bytes=100000000};CfdMemoryBudget *prior=cfd_memory_scope(&b);
    CfdOpenAtmosphere3d coarse,fine,heat;init(&coarse,8);init(&fine,16);init(&heat,8);
    /* Analytic affine incompressible flow u=-x/2, v=-y/2, w=z,
     * including top/lateral reservoir faces and impermeable ground. */
    for(int a=0;a<3;a++)for(int k=0;k<8+(a==2);k++)for(int j=0;j<8+(a==1);j++)for(int i=0;i<8+(a==0);i++)
        coarse.velocity[cfd_open_atmosphere3d_face(&coarse,a,i,j,k)]=(a==2?1.:-.5)*(int[]){i,j,k}[a]/8;
    double div;assert(matched_heat_velocity(&coarse,&fine,&div));assert(div<1e-14);
    for(int a=0;a<3;a++)for(int k=0;k<16+(a==2);k++)for(int j=0;j<16+(a==1);j++)for(int i=0;i<16+(a==0);i++) {
        double expected=(a==2?1.:-.5)*(int[]){i,j,k}[a]/16;
        assert(fabs(fine.candidate_velocity[cfd_open_atmosphere3d_face(&fine,a,i,j,k)]-expected)<1e-15);
    }
    /* Nonlinear solenoidal cross-flow verifies tangential transfer and all
     * four subfaces' area-averaged normal flux at every coarse face. */
    for(int a=0;a<3;a++)for(int k=0;k<8+(a==2);k++)for(int j=0;j<8+(a==1);j++)for(int i=0;i<8+(a==0);i++)
        coarse.velocity[cfd_open_atmosphere3d_face(&coarse,a,i,j,k)]=a==2?0:sin((a==0?j:i)*.37)*.05;
    assert(matched_heat_velocity(&coarse,&fine,&div));assert(div<1e-14);
    for(int a=0;a<3;a++)for(int k=0;k<8+(a==2);k++)for(int j=0;j<8+(a==1);j++)for(int i=0;i<8+(a==0);i++) {
        double sum=0;
        for(int v=0;v<2;v++)for(int w=0;w<2;w++) {
            int p[3]={2*i,2*j,2*k};p[(a+1)%3]+=v;p[(a+2)%3]+=w;
            sum+=fine.candidate_velocity[cfd_open_atmosphere3d_face(&fine,a,p[0],p[1],p[2])]/4;
        }
        assert(fabs(sum-coarse.velocity[cfd_open_atmosphere3d_face(&coarse,a,i,j,k)])<1e-15);
    }
    memset(coarse.velocity,0,(size_t)coarse.velocity_count*sizeof(double));
    double source[512]={0},smoke[512]={0};source[200]=.001;
    unsigned control_cycles=1;
#ifdef PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT
    control_cycles=(unsigned)round(coarse.dt/PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT);
#endif
    for(int step=0;step<4;step++) {
        assert(cfd_open_atmosphere3d_step(&coarse,source,smoke));
        memcpy(heat.candidate_velocity,coarse.velocity,(size_t)coarse.velocity_count*sizeof(double));
        double bound;assert(matched_heat_advance(&heat,source,control_cycles,&bound));
        assert(memcmp(heat.energy,coarse.energy,sizeof source)==0);
    }
    double accepted[512];memcpy(accepted,heat.energy,sizeof accepted);
    double bound;assert(!matched_heat_advance(&heat,source,0,&bound));
    assert(memcmp(accepted,heat.energy,sizeof accepted)==0);
    source[0]=-1;assert(!matched_heat_advance(&heat,source,1,&bound));source[0]=0;
    heat.candidate_velocity[0]=-100;assert(!matched_heat_advance(&heat,source,1,&bound));
    assert(memcmp(accepted,heat.energy,sizeof accepted)==0);
    /* Explicit substeps must divide source amounts as well as time. Compare
     * one two-substep call with two independently clocked half-dt calls. */
    memset(heat.candidate_velocity,0,(size_t)heat.velocity_count*sizeof(double));
    memcpy(coarse.energy,heat.energy,sizeof source);
    memset(coarse.candidate_velocity,0,(size_t)coarse.velocity_count*sizeof(double));
    assert(matched_heat_advance(&heat,source,2,&bound));
    coarse.dt=.0025;for(int q=0;q<512;q++)source[q]/=2;
    assert(matched_heat_advance(&coarse,source,1,&bound));
    assert(matched_heat_advance(&coarse,source,1,&bound));
    assert(memcmp(coarse.energy,heat.energy,sizeof source)==0);
    cfd_open_atmosphere3d_destroy(&coarse);cfd_open_atmosphere3d_destroy(&fine);cfd_open_atmosphere3d_destroy(&heat);
    cfd_memory_scope(prior);assert(b.live_bytes==0 && b.rejected_allocations==0);
    puts("matched heat: affine divergence, nonlinear face flux, evolving control parity, refusal atomicity and allocation closure passed");
    return 0;
}
