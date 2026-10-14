/* Independent localized heated scalar under prescribed evolving translation.
 * This isolates scalar transport, not nonlinear plume momentum. The compact
 * cos^4 packet has analytic cell averages and stays away from open Z planes.
 * u(t)=0.1+0.1t; X(t)=0.1t+0.05t^2; theta=(1+t)phi(x-X(t)).
 * Hence the exact volumetric heat source is rho*cp*phi K/s. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double primitive(double x) {
    double k=4*acos(-1.);x=fmax(-.125,fmin(.125,x));
    return .375*x+sin(2*k*x)/(4*k)+sin(4*k*x)/(32*k);
}
static double profile(const int n[3],int q,int axis,double t) {
    int coord[3]={q%n[0],q/n[0]%n[1],q/(n[0]*n[1])};double value=1;
    for(int a=0;a<3;a++){
        double center=.5+(a==axis?.1*t+.05*t*t:0);
        value*=fmax(0.,n[a]*(primitive((coord[a]+1.)/n[a]-center)-primitive((double)coord[a]/n[a]-center)));
    }
    return value;
}
static double run(int axis,int side,double dt,double *final) {
    int n[3]={8,8,8};n[axis]=side;
    double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,0,0,dt,zero,ambient,zero,false,9.81,1./300,.1));
    s.cache_projection=true;s.muscl=true;
    int count=s.grid.count;double *src=calloc((size_t)2*count,sizeof(double));assert(src);
    for(int q=0;q<count;q++){
        s.energy[q]=1206*s.grid.volume*profile(n,q,axis,0);s.smoke[q]=s.energy[q]*1e-6;
        s.initial_j+=s.energy[q];s.initial_kg+=s.smoke[q];
    }
    int ticks=(int)round(.5/dt);
    for(int tick=0;tick<ticks;tick++){
        double t=tick*dt,u=.1+.1*(t+.5*dt);
        for(int q=0;q<count+(axis==2?s.plane:0);q++)s.velocity[axis*count+q]=u;
        for(int q=0;q<count;q++){src[q]=1206*s.grid.volume*dt*profile(n,q,axis,t);src[count+q]=src[q]*1e-6;}
        if(!cfd_open_atmosphere3d_step(&s,src,src+count)){fprintf(stderr,"heated transport axis=%d side=%d tick=%d: %s\n",axis,side,tick,s.step_rejection);abort();}
    }
    double error=0,scale=0,total=0,peak=0;
    for(int q=0;q<count;q++){
        double exact=1206*s.grid.volume*1.5*profile(n,q,axis,.5);
        error+=fabs(s.energy[q]-exact);scale+=exact;total+=s.energy[q];peak=fmax(peak,s.energy[q]/(1206*s.grid.volume));
        assert(s.energy[q]>=0 && fabs(s.smoke[q]-s.energy[q]*1e-6)<1e-14);
    }
    double incoming=0,outgoing=0;
    for(int q=0;q<s.boundary_faces;q++){incoming+=s.flux[q];outgoing+=s.flux[s.boundary_faces+q];}
    double balance=total-s.initial_j-s.input_j-incoming+outgoing;
    assert(fabs(balance)<1e-10 && peak<=1.5+1e-12);
    printf("{\"check\":\"localized_heated_evolving_translation\",\"axis\":%d,\"side\":%d,\"dt_s\":%.17g,\"relative_l1\":%.17g,\"heat_balance_j\":%.17g}\n",axis,side,dt,error/scale,balance);
    if(final)for(int q=0;q<count;q++)final[q]=s.energy[q];
    free(src);cfd_open_atmosphere3d_destroy(&s);return error/scale;
}
void cfd_open_heated_transport_checks(void) {
    for(int axis=0;axis<3;axis++){
        double previous=1;
        for(int n=16;n<=64;n*=2){double e=run(axis,n,.0005,NULL);assert(e<previous);previous=e;}
        assert(previous<.1);
        double *f=calloc(3*32*8*8,sizeof(double));assert(f);
        for(int t=0;t<3;t++)run(axis,32,.005/(1<<t),f+t*32*8*8);
        double delta[2]={0,0},scale=0;
        for(int q=0;q<32*8*8;q++){
            scale+=f[2*32*8*8+q];
            for(int t=0;t<2;t++)delta[t]+=fabs(f[t*32*8*8+q]-f[(t+1)*32*8*8+q]);
        }
        assert(delta[0]/scale<.01 && delta[1]<.6*delta[0]);
        printf("{\"check\":\"heated_translation_time_refinement\",\"axis\":%d,\"coarse_difference_l1\":%.17g,\"fine_difference_l1\":%.17g,\"ratio\":%.17g}\n",axis,delta[0]/scale,delta[1]/scale,delta[1]/delta[0]);
        free(f);
    }
}
