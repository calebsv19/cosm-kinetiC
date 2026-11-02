/* Two-step source-edge isolation: no momentum solve, exactly zero velocity. */
#include "physics_sim_matched_heat.h"
#include "app/cfd_passive3d.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    CfdMemoryBudget b={.limit_bytes=268435456};CfdMemoryBudget *prior=cfd_memory_scope(&b);
    for(int diffusion=0;diffusion<2;diffusion++)for(int mc=0;mc<2;mc++)for(int side=32;side<=64;side*=2){
        int n[3]={side,side,side},lo[3]={3*side/8,3*side/8,3*side/8},hi[3]={5*side/8,5*side/8,5*side/8};
        double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};CfdOpenAtmosphere3d s;
        if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,diffusion?.026:0,.0005,.0025,z,t,z,true,9.81,1./300,.1))return 3;
        s.ground=true;s.muscl=true;s.scalar_mc=mc;
        if(!cfd_open_atmosphere3d_lateral(&s) || !cfd_open_atmosphere3d_box(&s,lo,hi))return 3;
        double *source=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));FILE *f=fopen(argv[side==32?1:2],"rb");
        if(!f || !source || fread(source,sizeof(double),(size_t)s.grid.count,f)!=(size_t)s.grid.count)return 4;
        fclose(f);for(int q=0;q<s.grid.count;q++)source[q]/=40;
        for(int step=0;step<2;step++){
            double bound;if(!matched_heat_advance(&s,source,2,&bound))return 5;
            double above=0;for(int q=s.plane*(side/8);q<s.plane*(3*side/8);q++)above+=s.energy[q];
            printf("{\"side\":%d,\"mc\":%d,\"conductivity\":%.17g,\"time_s\":%.17g,\"heat_above_source_j\":%.17g,\"input_j\":%.17g}\n",side,mc,s.conductivity,s.time,above,s.input_j);
            if(!diffusion && above!=0)return 5;
        }
        cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);
    }
    cfd_memory_scope(prior);return b.live_bytes || b.rejected_allocations?6:0;
}
