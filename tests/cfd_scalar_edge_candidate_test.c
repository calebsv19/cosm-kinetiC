/* Qualifier candidate: actual native Euler on zero/sharp/alternating/random
 * nonnegative fields, both velocity signs and diffusion alone. */
#include <assert.h>
#include <stdio.h>
#include "../src/app/cfd_open_atmosphere3d.c"
int main(void) {
    CfdMemoryBudget b={.limit_bytes=64*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&b);
    int shape[3]={128,4,64};double lengths[3]={.25,.25,.0625},z[2]={0},t[2]={300,300};
    CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,shape,lengths,1.2,1.8e-5,1005,300,.026,0,1./1600,z,t,z,false,0,0,.1));
    s.ground=s.muscl=s.scalar_mc=true;
    double *source=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));assert(source);
    double worst=0,minimum=1;unsigned seed=1234567;int checks=0;
    for(int pattern=0;pattern<6;pattern++)for(int sign=-1;sign<=1;sign++) {
        double start=0;
        for(int q=0;q<s.grid.count;q++) {
            int x=q%128,k=q/s.plane;seed=1664525u*seed+1013904223u;
            double v=pattern==0?0:pattern==1?1:pattern==2?(x==32&&k==16):
                pattern==3?(x>=16&&x<32&&k>=16&&k<32):pattern==4?((x+k)%2):(double)(seed&65535)/65535;
            s.energy[q]=v;start+=v;s.candidate_velocity[q]=sign*.05*(k+.5)/64;
        }
        /* The bound includes both x diffusion faces, vertical second-order
         * diffusion and the maximum reconstructed outgoing advective rate. */
        double bound=(2*.05/s.grid.h[0]+4*(.026/1206)/(s.grid.h[0]*s.grid.h[0])+2*(.026/1206)/(s.grid.h[2]*s.grid.h[2]));
        assert(bound/3200<1);
        for(int step=0;step<32;step++) {
            assert(scalar_euler(&s,s.energy,s.scalar_candidate,source,1./3200,0,.026/1206,0,1));
            double sum=0;
            for(int q=0;q<s.grid.count;q++){double v=s.scalar_candidate[q];assert(isfinite(v)&&v>=0);minimum=fmin(minimum,v);sum+=v;}
            double err=fabs(sum-start)/fmax(1,start);worst=fmax(worst,err);assert(err<1e-12);
            memcpy(s.energy,s.scalar_candidate,(size_t)s.grid.count*sizeof(double));checks++;
        }
    }
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!b.live_bytes);
    printf("{\"native_steps\":%d,\"minimum\":%.17g,\"conservation_relative\":%.17g,\"live_bytes\":%zu}\n",checks,minimum,worst,b.live_bytes);
}
