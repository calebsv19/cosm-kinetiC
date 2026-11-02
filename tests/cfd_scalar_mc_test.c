#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=64*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    double worst=0;
    for(int momentum=0;momentum<2;momentum++)for(int rk=0;rk<2;rk++)for(int mode=0;mode<5;mode++) {
        int n[3]={16,16,16};double l[3]={1,1,1},p[2]={0,0},t[2]={300,300},ambient_smoke[2]={0,0};
        if(mode==4){t[0]=t[1]=301;ambient_smoke[0]=ambient_smoke[1]=.001206;}CfdOpenAtmosphere3d s;
        assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,.026/1206,.01,p,t,ambient_smoke,false,9.81,1./300,.1));
        s.scalar_rk2=rk;s.muscl=momentum;s.scalar_mc=true;s.cache_projection=true;
        if(mode==3){int lo[3]={6,6,6},hi[3]={10,10,10};s.ground=true;assert(cfd_open_atmosphere3d_box(&s,lo,hi));}
        int count=s.grid.count;double *src=calloc(2*(size_t)count,sizeof(double));assert(src);
        for(int q=0;q<count;q++)if(!cfd_open_atmosphere3d_solid(&s,q)) {
            int x=q%16,y=q/16%16,z=q/256;
            double value=mode==4?0:mode==0?1:((x>=4 && x<12 && y>=4 && y<12 && z>=4 && z<12)?1:0);
            s.energy[q]=1206*s.grid.volume*value;s.smoke[q]=s.energy[q]*1e-6;
            s.initial_j+=s.energy[q];s.initial_kg+=s.smoke[q];
        }
        if(mode!=3)for(int a=0;a<3;a++)for(int q=0;q<count+(a==2?s.plane:0);q++)s.velocity[a*count+q]=(mode==2?-.5:.5);
        for(int tick=0;tick<30;tick++) {
            assert(cfd_open_atmosphere3d_step(&s,src,src+count));
            for(int q=0;q<count;q++) {
                double value=s.energy[q]/(1206*s.grid.volume);
                assert(value>=0 && value<=1+1e-12);
                assert(fabs(s.smoke[q]-s.energy[q]*1e-6)<1e-12);
                if(cfd_open_atmosphere3d_solid(&s,q))assert(s.energy[q]==0);
            }
        }
        double total=cfd_passive3d_total(s.energy,count),in=0,out=0;
        for(int q=0;q<s.boundary_faces;q++){in+=s.flux[q];out+=s.flux[s.boundary_faces+q];}
        worst=fmax(worst,fabs(total-s.initial_j-in+out));assert(worst<1e-9);
        double *saved=malloc((size_t)2*count*sizeof(double));assert(saved);memcpy(saved,s.energy,(size_t)2*count*sizeof(double));
        double time=s.time;
        if(rk){
            s.max_scalar_work_cells=s.scalar_work_cells+(unsigned long long)count;
            assert(!cfd_open_atmosphere3d_step(&s,src,src+count));
            assert(time==s.time && !memcmp(saved,s.energy,(size_t)2*count*sizeof(double)));
        }
        src[0]=-1;
        assert(!cfd_open_atmosphere3d_step(&s,src,src+count));assert(time==s.time && !memcmp(saved,s.energy,(size_t)2*count*sizeof(double)));
        free(saved);free(src);cfd_open_atmosphere3d_destroy(&s);
    }
    cfd_memory_scope(prior);assert(budget.live_bytes==0);
    printf("{\"mc_cases\":20,\"worst_balance_j\":%.17g,\"live_bytes\":0}\n",worst);
}
