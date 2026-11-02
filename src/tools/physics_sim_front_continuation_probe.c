/* Attribution only: resume supplied 128-cell means at their physical time.
 * Observe actual FCT transfers through SSPRK2, without changing its operator. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double captured_low[256],captured_correction[256],captured_factor[256];
static void capture_face(int i,double low,double correction,double factor) {
    captured_low[i]=low;captured_correction[i]=correction;captured_factor[i]=factor;
}
#define PHYSICS_SIM_FCT_FACE_DIAGNOSTIC(i,l,a,f) capture_face(i,l,a,f)
#include "physics_sim_scalar_fct.h"
static const double h=.25/128,u=.003125,alpha=.026/1206;
static void stage(const double *v,double *out,double dt,double transfer[128],
    double advection[128],double diffusion[128],double limiter[128],int *limited) {
    assert(physics_sim_scalar_fct(v,out,128,u,alpha,h,dt,true));
    for(int i=0;i<128;i++) {
        double ll=v[(i+126)%128],l=v[(i+127)%128],r=v[i],rr=v[(i+1)%128],face,gradient;
        PHYSICS_SIM_RETAINED_TARGET(ll,l,r,rr,u,h,&face,&gradient);
        double low_adv=dt*u*l/h,high_adv=dt*u*face/h;
        double low_diff=-dt*alpha*(r-l)/(h*h),high_diff=-dt*alpha*gradient/h;
        double theta=captured_factor[i];
        transfer[i]=captured_low[i]+theta*captured_correction[i];
        limiter[i]=(theta-1)*captured_correction[i];
        advection[i]=low_adv+theta*(high_adv-low_adv);
        diffusion[i]=low_diff+theta*(high_diff-low_diff);
        assert(fabs(transfer[i]-advection[i]-diffusion[i])<1e-14);
        if(theta<1 && captured_correction[i]!=0)(*limited)++;
    }
}
static void array(const char *name,const double *v) {
    printf(",\"%s\":[",name);for(int i=0;i<128;i++)printf("%s%.17g",i?",":"",v[i]);printf("]");
}
static void observe(int step,double dt,const double *v,const double *total,
    const double *advection,const double *diffusion,const double *limiter,int limited) {
    printf("{\"step\":%d,\"time_s\":%.17g,\"limited_stage_faces\":%d",step,step*dt,limited);
    array("profile",v);array("integrated_flux_k_m",total);
    array("integrated_advection_k_m",advection);array("integrated_diffusion_k_m",diffusion);
    array("integrated_limiter_adjustment_k_m",limiter);puts("}");
}
int main(int argc,char **argv) {
    if(argc!=5)return 2;
    int denominator=atoi(argv[1]),first=atoi(argv[2]),last=atoi(argv[3]);
    if((denominator!=3200&&denominator!=6400)||first<0||last<=first||
       last>denominator*3/2||first%(denominator/40)||last%(denominator/40))return 2;
    double dt=1./denominator,v[128],initial[128],a[128],b[128];
    FILE *f=fopen(argv[4],"rb");if(!f)return 2;
    if(fread(v,sizeof(double),128,f)!=128||fgetc(f)!=EOF){fclose(f);return 2;}fclose(f);
    double mass=0,lower=v[0],upper=v[0];
    for(int i=0;i<128;i++) {
        if(!isfinite(v[i])||v[i]<0||v[i]>1)return 2;
        mass+=v[i]*h;lower=fmin(lower,v[i]);upper=fmax(upper,v[i]);
    }
    if(fabs(mass-1./32)>1e-12)return 2;
    memcpy(initial,v,sizeof(v));
    double total[128]={0},advection[128]={0},diffusion[128]={0},limiter[128]={0};
    double f1[128],f2[128],a1[128],a2[128],d1[128],d2[128],l1[128],l2[128];
    double max_balance=0,max_closure=0,max_component_closure=0,minimum=lower,maximum=upper;
    int limited=0;observe(first,dt,v,total,advection,diffusion,limiter,limited);
    for(int step=first+1;step<=last;step++) {
        stage(v,a,dt,f1,a1,d1,l1,&limited);stage(a,b,dt,f2,a2,d2,l2,&limited);
        double current_mass=0;
        for(int i=0;i<128;i++) {
            v[i]=.5*(v[i]+b[i]);minimum=fmin(minimum,v[i]);maximum=fmax(maximum,v[i]);
            assert(v[i]>=lower-1e-12 && v[i]<=upper+1e-12);
            current_mass+=v[i]*h;
            total[i]+=.5*h*(f1[i]+f2[i]);
            advection[i]+=.5*h*(a1[i]+a2[i]);diffusion[i]+=.5*h*(d1[i]+d2[i]);
            limiter[i]+=.5*h*(l1[i]+l2[i]);
            max_component_closure=fmax(max_component_closure,fabs(total[i]-advection[i]-diffusion[i]));
        }
        max_balance=fmax(max_balance,fabs(current_mass-mass));assert(max_balance<1e-12);
        if(step%(denominator/40)==0) {
            for(int patch=0;patch<8;patch++) {
                double change=0;int west=patch*16,east=((patch+1)*16)%128;
                for(int i=west;i<west+16;i++)change+=(v[i]-initial[i])*h;
                max_closure=fmax(max_closure,fabs(change-(total[west]-total[east])));
            }
            observe(step,dt,v,total,advection,diffusion,limiter,limited);
        }
    }
    assert(max_closure<1e-12 && max_component_closure<1e-12);
    printf("{\"terminal\":true,\"n\":128,\"first_step\":%d,\"last_step\":%d,\"dt\":%.17g,\"minimum\":%.17g,\"maximum\":%.17g,\"balance_k_m\":%.17g,\"budget_closure_k_m\":%.17g,\"component_closure_k_m\":%.17g,\"limited_stage_faces\":%d}\n",first,last,dt,minimum,maximum,max_balance,max_closure,max_component_closure,limited);
}
