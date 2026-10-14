#include "../src/tools/physics_sim_vertical_source.h"
#include "../src/tools/physics_sim_flow_source.h"
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    enum { N = 16, P = N*N, SUPPORT = N*N*N/8, COUNT = 2*N*N*N };
    double input[2*SUPPORT], full[2*COUNT], control[N*N*N*2];
    for (int q=0;q<2*SUPPORT;q++) input[q]=(q%13)*.00001;
    assert(physics_sim_vertical_source(input,N,N,full));
    assert(physics_sim_flow_support(input,N,control));
    assert(!memcmp(full,control,sizeof control));
    assert(physics_sim_vertical_source(input,N,2*N,full));
    for(int a=0;a<2;a++)for(int k=0;k<N;k++)for(int p=0;p<P;p++) {
        double expected=k<N/8?input[a*SUPPORT+k*P+p]:0;
        assert(full[a*COUNT+2*k*P+p]+full[a*COUNT+(2*k+1)*P+p]==expected);
        assert(full[a*COUNT+2*k*P+p]==full[a*COUNT+(2*k+1)*P+p]);
    }
    double saved[2*COUNT];memcpy(saved,full,sizeof saved);
    input[2*SUPPORT-1]=NAN;
    assert(!physics_sim_vertical_source(input,N,2*N,full));
    assert(!memcmp(full,saved,sizeof saved));
    assert(!physics_sim_vertical_source(input,N,3*N,full));
    assert(!physics_sim_vertical_source(input,N,2*N,input));

    CfdMemoryBudget budget={.limit_bytes=100000000};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;int grid[3]={N,N,2*N};
    double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1));
    s.ground=true;s.muscl=true;s.cache_projection=true;
    assert(cfd_open_atmosphere3d_lateral(&s));
    for(int t=0;t<4;t++)assert(cfd_open_atmosphere3d_step(&s,saved,saved+COUNT));
    assert(s.steps==4 && s.scalar_work_cells==16ULL*COUNT && s.time==.02);
    double total=0,in=0,out=0;
    for(int q=0;q<COUNT;q++) {total+=s.energy[q];assert(s.energy[q]>=0 && s.smoke[q]>=0);}
    for(int q=0;q<s.boundary_faces;q++){in+=s.flux[q];out+=s.flux[s.boundary_faces+q];}
    assert(fabs(total-s.input_j-in+out)<1e-10 && s.divergence<1e-8 && s.residual<1e-11);
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("vertical source: isotropic parity, exact child closure, unchanged XY, invalid input refusal, anisotropic flow accounting and allocation closure passed");
    return 0;
}
