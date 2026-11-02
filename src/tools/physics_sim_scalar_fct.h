/* Experimental 1D periodic, constant-velocity, source-free scalar correction.
 * Limit paired high-minus-low face transfers against GLOBAL old-state bounds
 * at each Euler stage. SSPRK2 then preserves the same bounds by convex averaging.
 * Not a multidimensional or variable-velocity application operator. */
#ifndef PHYSICS_SIM_SCALAR_FCT_H
#define PHYSICS_SIM_SCALAR_FCT_H
#include <stdbool.h>
#include <float.h>
#if defined(PHYSICS_SIM_LOGQUADRATIC_TARGET)
#include "physics_sim_scalar_logquadratic.h"
#define PHYSICS_SIM_RETAINED_TARGET physics_sim_scalar_logquadratic
#elif defined(PHYSICS_SIM_FRONT_TARGET)
#include "physics_sim_scalar_front_target.h"
#define PHYSICS_SIM_RETAINED_TARGET physics_sim_scalar_front_target
#elif defined(PHYSICS_SIM_EXPONENTIAL_TARGET)
#include "physics_sim_scalar_exponential.h"
#define PHYSICS_SIM_RETAINED_TARGET physics_sim_scalar_exponential
#else
#define PHYSICS_SIM_RETAINED_TARGET cfd_scalar_edge_candidate
#endif
#include "../app/cfd_scalar_edge_candidate.h"
static inline bool physics_sim_scalar_fct(const double *v,double *out,int n,
    double u,double alpha,double h,double dt,bool retained_target) {
    if(!v||!out||v==out||n<4||n>256||!isfinite(u)||!isfinite(alpha)||alpha<0||
       !isfinite(h)||h<=0||!isfinite(dt)||dt<=0||dt*(fabs(u)/h+2*alpha/(h*h))>1)return false;
    double low_flux[256],correction[256],plus[256],minus[256];
    double lower=v[0],upper=v[0];
    for(int i=0;i<n;i++) {if(!isfinite(v[i]))return false;lower=fmin(lower,v[i]);upper=fmax(upper,v[i]);}
    for(int i=0;i<n;i++) {
        double ll=v[(i+n-2)%n],l=v[(i+n-1)%n],r=v[i],rr=v[(i+1)%n];
        double donor=(-ll+7*l+7*r-rr)/12,gradient=(ll-15*l+15*r-rr)/(12*h);
        if(retained_target)PHYSICS_SIM_RETAINED_TARGET(ll,l,r,rr,u,h,&donor,&gradient);
        low_flux[i]=dt*(u*(u>=0?l:r)-alpha*(r-l)/h)/h;
        correction[i]=dt*(u*donor-alpha*gradient)/h-low_flux[i];
    }
    for(int i=0;i<n;i++) {
        int right=(i+1)%n;
        /* Evaluate differences before addition to preserve constant states. */
        out[i]=v[i]+(low_flux[i]-low_flux[right]);
        double gain=fmax(0,correction[i])+fmax(0,-correction[right]);
        double loss=fmax(0,-correction[i])+fmax(0,correction[right]);
        plus[i]=gain>0?fmax(0,fmin(1,(upper-out[i])/gain)):1;
        minus[i]=loss>0?fmax(0,fmin(1,(out[i]-lower)/loss)):1;
        /* Leave rounding headroom when a correction exhausts a budget.
         * This changes the paired flux, never clips a final cell value. */
        if(plus[i]<1)plus[i]*=1-16*DBL_EPSILON;
        if(minus[i]<1)minus[i]*=1-16*DBL_EPSILON;
    }
    for(int i=0;i<n;i++) {
        int left=(i+n-1)%n;double a=correction[i];
        double factor=a>=0?fmin(plus[i],minus[left]):fmin(minus[i],plus[left]);
#ifdef PHYSICS_SIM_FCT_FACE_DIAGNOSTIC
        PHYSICS_SIM_FCT_FACE_DIAGNOSTIC(i,low_flux[i],a,factor);
#endif
        a*=factor;out[i]+=a;out[left]-=a;
    }
    for(int i=0;i<n;i++)if(!isfinite(out[i]))return false;
    return true;
}
#endif
