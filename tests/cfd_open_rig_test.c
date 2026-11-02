/* Fixed-rig storage/indexing, impermeability and pressure solve sanitizer probe. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_open_energy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void run(int n,bool opening) {
    CfdOpenAtmosphere3d s={0};int grid[3]={n,n,n};
    double length[3]={1,1,1},zero[2]={0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,false,9.81,1./300,.1));
    s.ground=true;s.cache_projection=true;s.scalar_mc=true;s.scalar_rk2=true;assert(cfd_open_atmosphere3d_lateral(&s));
    int steps[1]={0};double target[1]={.5},strength[1]={1};
    assert(cfd_open_weather_configure(&s,&s.weather,.25,.2,false,1,steps,target,strength));
    s.wind.quiescent_backflow=true;assert(cfd_open_atmosphere3d_rig(&s,opening));
    double *source=calloc((size_t)2*s.grid.count,sizeof(double));assert(source);
    source[(n/2*n+n/2)*n+n/4]=.001;source[s.grid.count+(n/2*n+n/2)*n+n/4]=1e-7;
    assert(cfd_open_atmosphere3d_valid(&s));
    for(int step=0;step<4;step++)assert(cfd_open_atmosphere3d_step(&s,source,source+s.grid.count));
    assert(cfd_open_atmosphere3d_valid(&s));
    int port=0,window=0;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++) {
        port+=!cfd_open_atmosphere3d_blocked(&s,0,0,j,k);
        window+=!cfd_open_atmosphere3d_blocked(&s,0,n/2,j,k);
    }
    assert(port==n*n/4);assert(window==(opening?n*n/8:0));
    for(int q=0;q<s.grid.count;q++)if(cfd_open_atmosphere3d_solid(&s,q))
        assert(s.energy[q]==0 && s.smoke[q]==0 && s.pressure[q]==0);
    free(source);cfd_open_atmosphere3d_destroy(&s);
}

int main(void) {
    CfdMemoryBudget budget={.limit_bytes=512u*1024u*1024u};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    run(16,false);run(16,true);run(32,false);run(32,true);
    assert(budget.live_bytes==0);cfd_memory_scope(prior);
    puts("rig16/32 open/closed pressure, faces, exclusions, zero tracked live bytes: passed");
    return 0;
}
