/* Experimental finite-duration heater-edge gradient from four cell means.
 * Infer a positive diffusion-age interval using the declared source jump.
 * Pair differences remove a constant/quadratic smooth background. Only the
 * requested face gradient changes; no state or source energy is replaced. */
#ifndef PHYSICS_SIM_HEATER_EDGE_H
#define PHYSICS_SIM_HEATER_EDGE_H
#include "physics_sim_source_interval.h"
#include <float.h>
static inline double physics_sim_heater_cold_mean(double sigma,int k) {
    return .5*sigma*(physics_sim_source_primitive((k+1)/sigma)-physics_sim_source_primitive(k/sigma));
}
static inline double physics_sim_heater_pair(double age,int k) {
    return age>0?age*(1-2*physics_sim_heater_cold_mean(sqrt(age),k)):0;
}
static inline double physics_sim_heater_pair_derivative(double age,int k) {
    if(age<=0)return 1;
    double s=sqrt(age),a=k/s,b=(k+1)/s;
    double fa=a*erfc(a)-exp(-a*a)/sqrt(acos(-1));
    double fb=b*erfc(b)-exp(-b*b)/sqrt(acos(-1));
    return 1-s*(fb-fa);
}
static inline bool physics_sim_heater_edge_fit(const double value[4],const double width[4],double jump,double alpha,double *gradient) {
    if(!gradient||!isfinite(jump)||jump==0||!isfinite(alpha)||alpha<=0)return false;
    double h=width[0],v[4];if(!isfinite(h)||h<=0)return false;
    for(int k=0;k<4;k++) {
        if(width[k]!=h||!isfinite(value[k])||value[k]<0)return false;
        v[k]=jump>0?value[k]:value[3-k];
    }
    if(!(v[0]<=v[1]&&v[1]<v[2]&&v[2]<=v[3]))return false;
    double unit=fabs(jump)*h*h/(4*alpha),inner=(v[2]-v[1])/unit,outer=(v[3]-v[0])/unit;
    if(!isfinite(inner)||!isfinite(outer)||inner<=0||outer<=inner||outer>=3*inner)return false;
    double lo=0,hi=4096;
    if(physics_sim_heater_pair(hi,0)<=inner)return false;
    for(int k=0;k<48;k++){double mid=(lo+hi)/2;if(physics_sim_heater_pair(mid,0)<inner)lo=mid;else hi=mid;}
    double young=0,old=(lo+hi)/2;
    double tolerance=1e-10*outer;
    for(int iteration=0;iteration<24;iteration++) {
        double f=physics_sim_heater_pair(old,0)-physics_sim_heater_pair(young,0)-inner;
        double g=physics_sim_heater_pair(old,1)-physics_sim_heater_pair(young,1)-outer;
        double norm=fmax(fabs(f),fabs(g));
        if(norm<=tolerance) {
            double result=fabs(jump)*h/(2*alpha*sqrt(acos(-1)))*(sqrt(old)-sqrt(young));
            if(!isfinite(result)||result<=0)return false;
            *gradient=jump>0?result:-result;return true;
        }
        double a=physics_sim_heater_pair_derivative(old,0),b=-physics_sim_heater_pair_derivative(young,0);
        double c=physics_sim_heater_pair_derivative(old,1),d=-physics_sim_heater_pair_derivative(young,1),det=a*d-b*c;
        if(!isfinite(det)||fabs(det)<DBL_EPSILON)return false;
        double dx=(f*d-g*b)/det,dy=(a*g-c*f)/det,step=1;bool accepted=false;
        for(int backtrack=0;backtrack<24;backtrack++,step*=.5) {
            double no=old-step*dx,ny=young-step*dy;
            if(ny<0||no<=ny||no>4096||!isfinite(no)||!isfinite(ny))continue;
            double nf=physics_sim_heater_pair(no,0)-physics_sim_heater_pair(ny,0)-inner;
            double ng=physics_sim_heater_pair(no,1)-physics_sim_heater_pair(ny,1)-outer;
            if(fmax(fabs(nf),fabs(ng))<norm){old=no;young=ny;accepted=true;break;}
        }
        if(!accepted)return false;
    }
    return false;
}
#endif
