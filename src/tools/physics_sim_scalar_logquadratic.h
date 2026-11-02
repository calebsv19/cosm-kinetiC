/* Diagnostic finite-volume positive tail fit. At each face fit
 * T(x)=A exp(b*x/h+c*(x/h)^2), c<=0, to adjacent cell means and the
 * symmetric outer/inner mean ratio. Both face value and derivative use this
 * profile. Exact zeros/extrema/failed fits retain the cold-cell repair.
 * Fits are face-local, not a globally continuous reconstructed field. */
#ifndef PHYSICS_SIM_SCALAR_LOGQUADRATIC_H
#define PHYSICS_SIM_SCALAR_LOGQUADRATIC_H
#include <float.h>
#include <stdbool.h>
#include "physics_sim_scalar_front_target.h"
static inline void physics_sim_log_moments(double b,double c,int lo,double out[3]) {
    static const double nodes[8]={-.9602898564975363,-.7966664774136267,
        -.5255324099163290,-.1834346424956498,.1834346424956498,
        .5255324099163290,.7966664774136267,.9602898564975363};
    static const double weights[8]={.1012285362903763,.2223810344533745,
        .3137066458778873,.3626837833783620,.3626837833783620,
        .3137066458778873,.2223810344533745,.1012285362903763};
    double x[8],z[8],top=-INFINITY;
    for(int j=0;j<8;j++){x[j]=lo+(nodes[j]+1)*.5;z[j]=b*x[j]+c*x[j]*x[j];top=fmax(top,z[j]);}
    double sum=0,first=0,second=0;
    for(int j=0;j<8;j++){double q=weights[j]*.5*exp(z[j]-top);sum+=q;first+=q*x[j];second+=q*x[j]*x[j];}
    out[0]=top+log(sum);out[1]=first/sum;out[2]=second/sum;
}
static inline bool physics_sim_logquadratic_fit(double ll,double l,double r,
    double rr,double h,double *value,double *gradient) {
    if(!(ll>0 && l>0 && r>0 && rr>0))return false;
    if(!((ll<=l && l<=r && r<=rr)||(ll>=l && l>=r && r>=rr)))return false;
    double logs[4]={log(ll),log(l),log(r),log(rr)};
    double d1=logs[2]-logs[1],d2=(logs[0]+logs[3]-logs[1]-logs[2])*.5;
    double roundoff=64*DBL_EPSILON*(1+fabs(d1)+fabs(d2));
    if(d2>roundoff)return false;
    double b=d1,c=fmin(0,d2*.5),m[4][3];
    for(int iteration=0;iteration<10;iteration++) {
        for(int j=0;j<4;j++)physics_sim_log_moments(b,c,j-2,m[j]);
        double f=m[2][0]-m[1][0]-d1;
        double g=(m[0][0]+m[3][0]-m[1][0]-m[2][0])*.5-d2;
        if(!isfinite(f)||!isfinite(g))return false;
        if(fmax(fabs(f),fabs(g))<1e-11) {
            double face=exp((logs[1]+logs[2]-m[1][0]-m[2][0])*.5);
            double derivative=b*face/h;
            if(b*d1<0 || !isfinite(face)||!isfinite(derivative)||face<=0)return false;
            if(d1!=0 && derivative==0)return false;
            *value=face;*gradient=derivative;return true;
        }
        double j00=m[2][1]-m[1][1],j01=m[2][2]-m[1][2];
        double j10=(m[0][1]+m[3][1]-m[1][1]-m[2][1])*.5;
        double j11=(m[0][2]+m[3][2]-m[1][2]-m[2][2])*.5;
        double determinant=j00*j11-j01*j10;
        if(!isfinite(determinant)||fabs(determinant)<DBL_EPSILON)return false;
        b-=(f*j11-g*j01)/determinant;c-=(j00*g-j10*f)/determinant;
        if(!isfinite(b)||!isfinite(c)||c>roundoff)return false;
        c=fmin(0,c);
    }
    return false;
}
static inline void physics_sim_scalar_logquadratic(double ll,double l,double r,
    double rr,double u,double h,double *value,double *gradient) {
    physics_sim_scalar_front_target(ll,l,r,rr,u,h,value,gradient);
#if PHYSICS_SIM_LOGQUADRATIC_TARGET == 2
    double fitted_value,fitted_gradient;
    if(physics_sim_logquadratic_fit(ll,l,r,rr,h,&fitted_value,&fitted_gradient) &&
       fabs(fitted_gradient*h/fitted_value)>1) {
        *value=fitted_value;*gradient=fitted_gradient;
    }
#else
    (void)physics_sim_logquadratic_fit(ll,l,r,rr,h,value,gradient);
#endif
}
#endif
