#include "../src/tools/physics_sim_horizontal_source.h"
#include "../src/tools/physics_sim_vertical_source.h"
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    CfdCartesian3d admitted;int large[3]={256,128,256},too_large[3]={256,256,256};double unit[3]={1,1,1};
    assert(cfd_cartesian3d_init(&admitted,large,unit) && admitted.count==8388608);
    CfdCartesian3d kept=admitted;assert(!cfd_cartesian3d_init(&admitted,too_large,unit));
    assert(!memcmp(&admitted,&kept,sizeof kept));
    enum { N=8, H=16, S=N*N*N/8, V=N*N*H, C=H*N*H };
    double input[2*S],fine[2*C],vertical[2*V],saved[2*C];
    for(int q=0;q<2*S;q++)input[q]=(q%13)*.00001;
    assert(physics_sim_horizontal_source(input,N,fine));
    assert(physics_sim_vertical_source(input,N,H,vertical));
    for(int a=0;a<2;a++)for(int k=0;k<H;k++)for(int j=0;j<N;j++)for(int i=0;i<N;i++) {
        double sum=0;
        for(int x=0;x<2;x++)sum+=fine[a*C+(k*N+j)*H+2*i+x];
        assert(sum==vertical[a*V+(k*N+j)*N+i]);
    }
    memcpy(saved,fine,sizeof fine);input[2*S-1]=NAN;
    assert(!physics_sim_horizontal_source(input,N,fine));assert(!memcmp(saved,fine,sizeof fine));
    assert(!physics_sim_horizontal_source(input,256,fine));assert(!physics_sim_horizontal_source(input,N,input));
    CfdMemoryBudget budget={.limit_bytes=100000000};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;int grid[3]={H,N,H};double lengths[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&s,grid,lengths,1.2,1.8e-5,1005,300,.026,.0005,.0025,zero,ambient,zero,true,9.81,1./300,.1));
    s.ground=true;s.muscl=true;s.cache_projection=true;assert(cfd_open_atmosphere3d_lateral(&s));
    for(int t=0;t<4;t++)assert(cfd_open_atmosphere3d_step(&s,saved,saved+C));
    double heat=0;for(int q=0;q<C;q++)heat+=s.energy[q];
    assert(fabs(heat-s.input_j)<1e-10 && s.scalar_work_cells==8ULL*C && s.steps==4);
    assert(s.divergence<1e-8 && s.residual<1e-11);
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("horizontal source exact restriction, refusal preservation, fixed scalar timing, conservative evolution and allocation closure passed");
}
