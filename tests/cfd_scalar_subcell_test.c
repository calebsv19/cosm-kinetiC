/* Independent Fourier amplification, positivity, bounds and periodic mass. */
#include "../src/tools/physics_sim_scalar_subcell.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
static struct physics_sim_subcell solver;
static double v[4096];
int main(void) {
    int sizes[6]={8,32,128,512,2048,4096},checks=0;double worst=0,mass_error=0;
    for(int size=0;size<6;size++)for(int direction=-1;direction<=1;direction++)
    for(int diffusion=0;diffusion<2;diffusion++)for(int scaling=0;scaling<3;scaling++)
    for(int pattern=0;pattern<2;pattern++) {
        int n=sizes[size];double dx=.25/n,u=direction*.003125,alpha=diffusion*.026/1206;
        double rate=fabs(u)/dx+2*alpha/(dx*dx),dt=rate?.1/rate:.001;
        double scale=scaling==0?1:scaling==1?1e-100:1e100;
        assert(physics_sim_subcell_init(&solver,n,u,alpha,dx,dt));
        double theta=6.283185307179586/n,total=0;
        for(int i=0;i<n;i++){v[i]=scale*(pattern?(i==n/2):(1+.25*cos(theta*i)));total+=v[i]/scale;}
        double ar=1+dt*(2*alpha/(dx*dx)+fabs(u)/dx)*(1-cos(theta));
        double ai=dt*u/dx*sin(theta),den=ar*ar+ai*ai,re=.25,im=0;
        for(int step=0;step<16;step++) {
            double lo=v[0],hi=v[0];for(int i=0;i<n;i++){lo=fmin(lo,v[i]);hi=fmax(hi,v[i]);}
            assert(physics_sim_subcell_step(&solver,v));
            double nr=(re*ar+im*ai)/den;im=(im*ar-re*ai)/den;re=nr;
            double mass=0;
            for(int i=0;i<n;i++){
                assert(v[i]>=0 && v[i]>=lo-64*DBL_EPSILON*scale && v[i]<=hi+64*DBL_EPSILON*scale);
                mass+=v[i]/scale;
                if(!pattern){double expected=1+re*cos(theta*i)-im*sin(theta*i);
                    worst=fmax(worst,fabs(v[i]/scale-expected));assert(fabs(v[i]/scale-expected)<1e-11);}
            }
            mass_error=fmax(mass_error,fabs(mass-total)/total);assert(fabs(mass-total)/total<1e-11);checks++;
        }
    }
    printf("{\"subcell_stage_checks\":%d,\"max_fourier_absolute_scaled\":%.17g,\"max_mass_relative\":%.17g}\n",checks,worst,mass_error);
}
