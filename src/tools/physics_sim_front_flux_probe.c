/* Observe the real paired FCT transfers on a supplied cell-average field. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double low[256],delta[256],theta[256];
#define PHYSICS_SIM_FCT_FACE_DIAGNOSTIC(i,l,a,f) do {low[i]=(l);delta[i]=(a);theta[i]=(f);} while(0)
#include "physics_sim_scalar_fct.h"
static void array(const char *name,const double *v,int n){printf(",\"%s\":[",name);for(int i=0;i<n;i++)printf("%s%.17g",i?",":"",v[i]);printf("]");}
int main(int argc,char **argv) {
    if(argc!=6)return 2;int n=atoi(argv[1]);double u=atof(argv[2]),k=atof(argv[3]),dt=atof(argv[4]);
    if((n!=128&&n!=256)||fabs(u)>.05||(k!=0&&k!=.026)||(dt!=1./3200&&dt!=1./6400))return 2;
    FILE *f=fopen(argv[5],"rb");assert(f);double v[256],out[256];
    assert(fread(v,sizeof(double),(size_t)n,f)==(size_t)n&&fgetc(f)==EOF);fclose(f);
    double h=.25/n,alpha=k/1206;
    assert(physics_sim_scalar_fct(v,out,n,u,alpha,h,dt,true));
    double raw_t[256],bounded_t[256],raw_g[256],bounded_g[256],adv[256],diff[256],target[256],corrected[256];
    double parity=0,norm=0,balance=0,min=v[0],max=v[0],bound_violation=0;
    for(int i=0;i<n;i++){min=fmin(min,v[i]);max=fmax(max,v[i]);}
    int limited=0;
    for(int i=0;i<n;i++) {
        double ll=v[(i+n-2)%n],l=v[(i+n-1)%n],r=v[i],rr=v[(i+1)%n];
        raw_t[i]=(-ll+7*l+7*r-rr)/12;raw_g[i]=(ll-15*l+15*r-rr)/(12*h);
        PHYSICS_SIM_RETAINED_TARGET(ll,l,r,rr,u,h,&bounded_t[i],&bounded_g[i]);
        adv[i]=u*bounded_t[i];diff[i]=-alpha*bounded_g[i];target[i]=adv[i]+diff[i];
        corrected[i]=(low[i]+theta[i]*delta[i])*h/dt;
        assert(fabs(target[i]-(low[i]+delta[i])*h/dt)<1e-15);
        if(theta[i]<1 && delta[i]!=0)limited++;
        balance+=out[i]-v[i];bound_violation=fmax(bound_violation,fmax(min-out[i],out[i]-max));
    }
    for(int i=0;i<n;i++) {
        double net=dt/h*(corrected[i]-corrected[(i+1)%n]);
        parity+=fabs((out[i]-v[i])-net);norm+=fabs(net);
    }
    assert(parity<1e-12 && fabs(balance*h)<1e-12 && bound_violation<1e-12);
    printf("{\"n\":%d,\"u\":%.17g,\"k\":%.17g,\"dt\":%.17g,\"limited_faces\":%d,\"update_parity_absolute\":%.17g,\"update_parity_relative\":%.17g,\"balance_k_m\":%.17g,\"bound_violation\":%.17g",n,u,k,dt,limited,parity,parity/fmax(norm,1e-300),balance*h,bound_violation);
    array("raw_temperature",raw_t,n);array("bounded_temperature",bounded_t,n);array("raw_gradient",raw_g,n);array("bounded_gradient",bounded_g,n);array("advection",adv,n);array("diffusion",diff,n);array("target",target,n);array("corrected",corrected,n);array("theta",theta,n);
    puts("}");
}
