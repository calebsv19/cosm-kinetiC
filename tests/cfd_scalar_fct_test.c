/* Invariant checks at every stage, across arbitrary profiles and near CFL limit. */
#include "../src/tools/physics_sim_scalar_fct.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    double v[256],out[256],worst=0;unsigned seed=19;int checks=0;
    for(int size=8;size<=256;size*=2)for(int target=0;target<2;target++)
    for(int pattern=0;pattern<5;pattern++)for(int direction=-1;direction<=1;direction++)
    for(int diff=0;diff<2;diff++)for(int scale_case=0;scale_case<3;scale_case++) {
        double scale=scale_case==0?1:scale_case==1?1e-12:1e6;
        double u=direction*.01875,a=diff*.026/1206,h=.25/size;
        double rate=fabs(u)/h+2*a/(h*h),dt=rate? .95/rate : .001;
        double initial=0;
        for(int i=0;i<size;i++) {
            seed=1664525u*seed+1013904223u;
            double x=pattern==0?1:pattern==1?(i==size/2):pattern==2?(i%2):
                pattern==3?(double)(seed&65535)/65535:2*sin(6.283185307179586*i/size)-1;
            v[i]=scale*x;initial+=v[i];
        }
        for(int step=0;step<16;step++) {
            double lo=v[0],hi=v[0],sum=0;
            for(int i=0;i<size;i++){lo=fmin(lo,v[i]);hi=fmax(hi,v[i]);}
            assert(physics_sim_scalar_fct(v,out,size,u,a,h,dt,target));
            for(int i=0;i<size;i++) {
                assert(isfinite(out[i]));
                assert(out[i]>=lo-64*DBL_EPSILON*scale && out[i]<=hi+64*DBL_EPSILON*scale);
                if(pattern<4)assert(out[i]>=0);
                if(pattern==0)assert(fabs(out[i]-scale)<=4*DBL_EPSILON*scale);
                sum+=out[i];
            }
            double residual=fabs(sum-initial)/(size*scale);worst=fmax(worst,residual);assert(residual<1e-13);
            memcpy(v,out,(size_t)size*sizeof(double));checks++;
        }
    }
    double old[8]={0},sentinel[8];for(int i=0;i<8;i++)sentinel[i]=42;
    assert(!physics_sim_scalar_fct(old,sentinel,8,.05,.026/1206,.001,1,false));
    assert(!physics_sim_scalar_fct(old,sentinel,8,NAN,0,.001,.001,false));
    assert(!physics_sim_scalar_fct(old,sentinel,8,0,-1,.001,.001,false));
    assert(!physics_sim_scalar_fct(old,old,8,0,0,.001,.001,false));
    for(int i=0;i<8;i++)assert(sentinel[i]==42);
    printf("{\"stage_checks\":%d,\"max_conservation_scaled\":%.17g,\"invalid_input_refusals\":4}\n",checks,worst);
}
