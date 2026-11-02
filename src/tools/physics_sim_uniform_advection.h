/* Diagnostic adapter: exact retained FCT limiter with a generated workspace
 * capacity specialization; physical diffusion remains the existing BE solve. */
#ifndef PHYSICS_SIM_UNIFORM_ADVECTION_H
#define PHYSICS_SIM_UNIFORM_ADVECTION_H
#include "physics_sim_scalar_fct_4096.h"
#include <stdio.h>
static double uniform_adv_stage[4096],uniform_adv_out[4096];
static unsigned long long uniform_adv_checks,uniform_adv_roundoff_cells;
static double uniform_adv_max_excursion;
static inline bool uniform_advection_bounds(const double *old,const double *next,int n){
 double lo=old[0],hi=old[0];
 for(int j=0;j<n;j++){lo=fmin(lo,old[j]);hi=fmax(hi,old[j]);}
 /* Match the retained limiter tests' floating-point comparison contract.
  * Physical nonnegativity and the existing source-temperature cap stay strict. */
 double rounding=64*DBL_EPSILON*fmax(fabs(lo),fabs(hi));
 for(int j=0;j<n;j++){
  if(!isfinite(next[j])||next[j]<0||next[j]>.4+1e-10||next[j]<lo-rounding||next[j]>hi+rounding){
   fprintf(stderr,"bounds refusal checks=%llu cell=%d next=%a lo=%a hi=%a\n",uniform_adv_checks,j,next[j],lo,hi);return false;
  }
  if(next[j]<lo||next[j]>hi){
   uniform_adv_roundoff_cells++;
   uniform_adv_max_excursion=fmax(uniform_adv_max_excursion,fmax(lo-next[j],next[j]-hi));
  }
 }
 uniform_adv_checks++;return true;
}
static inline bool uniform_advection_euler(const double *v,double *out,int n,double u,double dx,double dt,int method){
 if(method==2){
  if(!physics_sim_scalar_fct(v,out,n,u,0,dx,dt,false))return false;
 }else{
  double c=fabs(u)*dt/dx;if(c>1)return false;
  for(int j=0;j<n;j++){
   int donor=(j+(u>=0?n-1:1))%n;
   out[j]=v[j]+c*(v[donor]-v[j]);
  }
 }
 return uniform_advection_bounds(v,out,n);
}
static inline bool uniform_advection_step(double *v,int n,double u,double dx,double dt,int method){
 if(n<4||n>4096||(method!=1&&method!=2))return false;
 if(u==0)return true;
 if(!uniform_advection_euler(v,uniform_adv_stage,n,u,dx,dt,method)||
    !uniform_advection_euler(uniform_adv_stage,uniform_adv_out,n,u,dx,dt,method))return false;
 for(int j=0;j<n;j++)uniform_adv_out[j]=.5*(v[j]+uniform_adv_out[j]);
 if(!uniform_advection_bounds(v,uniform_adv_out,n))return false;
 for(int j=0;j<n;j++)v[j]=uniform_adv_out[j];return true;
}
#endif
