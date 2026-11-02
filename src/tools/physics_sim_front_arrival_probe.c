/* One-dimensional periodic top-hat transport using the retained native face
 * reconstruction. No source integration, wall exchange, or solid response. */
#include "../app/cfd_scalar_edge_candidate.h"
#include "physics_sim_scalar_fct.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int n,mode;static double u,alpha,h,dt;
static unsigned long long value_stalls,gradient_stalls;
static double minimum=1,maximum,balance;
static void euler(const double *v,double *out) {
    if(mode>=4){assert(physics_sim_scalar_fct(v,out,n,u,alpha,h,dt,mode==5));
        for(int i=0;i<n;i++)assert(out[i]>=0 && out[i]<=1+1e-12);return;}
    memcpy(out,v,(size_t)n*sizeof(double));
    for(int i=0;i<n;i++) {
        double ll=v[(i+n-2)%n],l=v[(i+n-1)%n],r=v[i],rr=v[(i+1)%n],donor,gradient;
        cfd_scalar_edge_candidate(ll,l,r,rr,u,h,&donor,&gradient);
        double upstream=u>=0?l:r,central=(r-l)/h;
        if(donor==0 && upstream>0 && u!=0){value_stalls++;if(mode&1)donor=upstream;}
        if(gradient==0 && central!=0 && alpha!=0){gradient_stalls++;if(mode&2)gradient=central;}
        double flux=dt*(u*donor-alpha*gradient)/h;
        out[i]+=flux;out[(i+n-1)%n]-=flux;
    }
    for(int i=0;i<n;i++)assert(isfinite(out[i])&&out[i]>=0);
}
static void observe(int step,const double *v) {
    printf("{\"step\":%d,\"time_s\":%.17g,\"profile\":[",step,step*dt);
    for(int i=0;i<n;i++)printf("%s%.17g",i?",":"",v[i]);puts("]}");
}
int main(int argc,char **argv) {
    if(argc!=6)return 2;n=atoi(argv[1]);int steps=atoi(argv[2]);u=atof(argv[3]);mode=atoi(argv[4]);alpha=atof(argv[5])/1206;
    if((n!=128&&n!=256)||(steps!=10240&&steps!=20480)||mode<0||mode>5||fabs(u)>.05||(alpha!=0&&alpha!=.026/1206))return 2;
    h=.25/n;dt=3.2/steps;assert(dt*(2*fabs(u)/h+4*alpha/(h*h))<1);
    double v[256],stage[256],next[256];double start=0;
    for(int i=0;i<n;i++){v[i]=fmax(0,fmin((i+1)*h,1./16)-fmax(i*h,1./32))/h;start+=v[i];}
    observe(0,v);
    for(int step=1;step<=steps;step++) {
        euler(v,stage);euler(stage,next);double sum=0;
        for(int i=0;i<n;i++){v[i]=.5*(v[i]+next[i]);sum+=v[i];minimum=fmin(minimum,v[i]);maximum=fmax(maximum,v[i]);}
        balance=fmax(balance,fabs(sum-start)*h);assert(balance<1e-12);
        if(step%(steps/128)==0)observe(step,v);
    }
    printf("{\"terminal\":true,\"n\":%d,\"steps\":%d,\"u\":%.17g,\"alpha\":%.17g,\"mode\":%d,\"minimum\":%.17g,\"maximum\":%.17g,\"balance_k_m\":%.17g,\"value_stalls\":%llu,\"gradient_stalls\":%llu}\n",n,steps,u,alpha,mode,minimum,maximum,balance,value_stalls,gradient_stalls);
    return 0;
}
