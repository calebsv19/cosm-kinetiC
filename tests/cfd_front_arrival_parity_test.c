/* The small 1D harness must reproduce the real candidate Euler operator. */
#define main front_probe_main
#include "../src/tools/physics_sim_front_arrival_probe.c"
#undef main
#include "../src/app/cfd_open_atmosphere3d.c"
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=64*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;int shape[3]={128,4,4};double length[3]={.25,.25,.0625},zero[2]={0},ambient[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&s,shape,length,1.2,1.8e-5,1005,300,.026,0,1./3200,zero,ambient,zero,false,0,0,.1));
    s.ground=s.muscl=s.scalar_mc=true;n=128;mode=0;h=.25/n;dt=1./3200;alpha=.026/1206;
    double v[256],next[256],worst=0;double *source=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));assert(source);
    double pure_peak=0;
    for(int diffusion=0;diffusion<=1;diffusion++) {
    alpha=diffusion*.026/1206;
    for(int direction=-1;direction<=1;direction++) {
        u=direction*.01875;
        for(int i=0;i<n;i++)v[i]=(i>=16&&i<32)?1:0;
        for(int q=0;q<s.grid.count;q++){s.energy[q]=v[q%n];s.candidate_velocity[q]=u;}
        for(int step=0;step<64;step++) {
            euler(v,next);assert(scalar_euler(&s,s.energy,s.scalar_candidate,source,dt,0,alpha,0,1));
            for(int q=0;q<s.grid.count;q++)worst=fmax(worst,fabs(next[q%n]-s.scalar_candidate[q]));
            if(!diffusion)for(int q=0;q<s.grid.count;q++)pure_peak=fmax(pure_peak,s.scalar_candidate[q]);
            assert(worst<1e-14);memcpy(v,next,(size_t)n*sizeof(double));memcpy(s.energy,s.scalar_candidate,(size_t)s.grid.count*sizeof(double));
        }
    }
    }
    assert(pure_peak>1); /* Explicitly reproduce the candidate limitation. */
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!budget.live_bytes);
    printf("{\"native_euler_stages\":384,\"pure_advection_peak\":%.17g,\"max_parity_error\":%.17g,\"live_bytes\":%zu}\n",pure_peak,worst,budget.live_bytes);
}
