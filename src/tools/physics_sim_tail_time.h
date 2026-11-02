/* Conservative SSPRK(10,4), same face reconstruction in every stage.
 * Ketcheson 2009 thesis, section 6.4.2, Pseudo-code 3. Convex form avoids
 * the cancellation in its low-storage 15*q2-5*q1 expression. Each external
 * call still advances dt; the ten internal evaluations are not new clocks.
 * No matrix predictor, source, wall law or reference field is used here. */
#ifndef PHYSICS_SIM_TAIL_TIME_H
#define PHYSICS_SIM_TAIL_TIME_H
#include "physics_sim_tail_diffusion.h"
typedef struct {double base[4096],work[4096],saved[4096];} PhysicsSimTailTime;

/* Retain existing paired FCT headroom factors and actual-cell bounds. */
static inline bool physics_sim_tail_euler_flux(PhysicsSimTailDiffusion *s,double *v,
    double alpha,double dt,int high,const double *requested,int selected_face){
 PhysicsSimNonuniformDiffusion *d=&s->stencil;PhysicsSimNonuniformAdvection *a=&d->limiter;int n=a->n;
 memcpy(d->old,v,(size_t)n*sizeof(double));
 double lo=v[0],hi=v[0];
 for(int j=0;j<n;j++){
  int l=(j+n-1)%n,r=(j+1)%n;
  double left=(s->periodic||j>0)?2*dt*alpha/(a->width[j]*(a->width[j]+a->width[l])):0;
  double right=(s->periodic||j<n-1)?2*dt*alpha/(a->width[j]*(a->width[j]+a->width[r])):0;
  if(left+right>1||!isfinite(v[j])||v[j]<0||v[j]>.4+1e-10)return false;
  lo=fmin(lo,v[j]);hi=fmax(hi,v[j]);a->out[j]=v[j];
 }
 for(int i=0;i<n;i++){
  int l=(i+n-1)%n;
  a->low[i]=((!s->periodic&&i==0)||(selected_face>=0&&i!=selected_face))?0:-dt*alpha/a->unit_width*physics_sim_tail_gradient(s,d->old,i,0);
  if(!physics_sim_tail_apply(a,a->out,l,i,lo,hi,&a->low[i]))return false;
 }
 for(int i=0;i<n;i++){
  double desired=requested?((selected_face>=0&&i!=selected_face)?0:requested[i]):((!s->periodic&&i==0)?0:-dt*alpha/a->unit_width*physics_sim_tail_gradient(s,d->old,i,high==3?2:high));
  if(high==3)a->stage[i]=desired;
  a->correction[i]=desired-a->low[i];
 }
 for(int j=0;j<n;j++){
  int r=(j+1)%n;double gain=fmax(0,a->correction[j])+fmax(0,-a->correction[r]),loss=fmax(0,-a->correction[j])+fmax(0,a->correction[r]);
  a->plus[j]=gain>0?fmax(0,fmin(1,a->capacity[j]*(hi-a->out[j])/gain)):1;
  a->minus[j]=loss>0?fmax(0,fmin(1,a->capacity[j]*(a->out[j]-lo)/loss)):1;
  if(a->plus[j]<1)a->plus[j]*=1-16*DBL_EPSILON;if(a->minus[j]<1)a->minus[j]*=1-16*DBL_EPSILON;
 }
 /* Mode3 retains mode2 reconstruction and FCT factors. Form the admitted
  * total as a convex flux blend, then apply it once to the original field.
  * Low + (high-low) can erase representable cold heat by cancellation. */
 if(high==3){
  for(int i=0;i<n;i++){
   int l=(i+n-1)%n;double q=a->correction[i],factor=q>=0?fmin(a->plus[i],a->minus[l]):fmin(a->minus[i],a->plus[l]);
   a->correction[i]=(1-factor)*a->low[i]+factor*a->stage[i];
  }
  memcpy(a->out,d->old,(size_t)n*sizeof(double));
  for(int i=0;i<n;i++){int l=(i+n-1)%n;double q=a->correction[i];if(!physics_sim_tail_apply(a,a->out,l,i,lo,hi,&q))return false;}
 }else{
 for(int i=0;i<n;i++){
  int l=(i+n-1)%n;double q=a->correction[i],factor=q>=0?fmin(a->plus[i],a->minus[l]):fmin(a->minus[i],a->plus[l]);q*=factor;
  if(!physics_sim_tail_apply(a,a->out,l,i,lo,hi,&q))return false;
 }
 }
 if(!physics_sim_nonuniform_bounds(a,d->old,a->out))return false;
 memcpy(v,a->out,(size_t)n*sizeof(double));return true;
}
/* Original all-face path retains its arithmetic. Optional one-face requests
 * reuse exactly these FCT factors and actual-cell guards in source-age staging. */
static inline bool physics_sim_tail_euler(PhysicsSimTailDiffusion *s,double *v,double alpha,double dt,int high){
 return physics_sim_tail_euler_flux(s,v,alpha,dt,high,NULL,-1);
}
static inline double physics_sim_tail_mix(double a,double b,double weight){return a+weight*(b-a);}
static inline bool physics_sim_tail_time_step(PhysicsSimTailDiffusion *s,PhysicsSimTailTime *rk,
    double *v,double alpha,double dt,int high){
 if(!s||!rk||!v||!isfinite(alpha)||alpha<0||!isfinite(dt)||dt<=0||high<0||high>3)return false;
 int n=s->stencil.limiter.n;if(n<4||n>4096)return false;
 memcpy(rk->base,v,(size_t)n*sizeof(double));memcpy(rk->work,v,(size_t)n*sizeof(double));
 for(int stage=0;stage<5;stage++)if(!physics_sim_tail_euler(s,rk->work,alpha,dt/6,high))return false;
 for(int j=0;j<n;j++){
  rk->saved[j]=physics_sim_tail_mix(rk->base[j],rk->work[j],.9);
  rk->work[j]=physics_sim_tail_mix(rk->base[j],rk->work[j],.4);
 }
 for(int stage=0;stage<5;stage++)if(!physics_sim_tail_euler(s,rk->work,alpha,dt/6,high))return false;
 for(int j=0;j<n;j++)rk->work[j]=physics_sim_tail_mix(rk->saved[j],rk->work[j],.6);
 if(!physics_sim_nonuniform_bounds(&s->stencil.limiter,rk->base,rk->work))return false;
 memcpy(v,rk->work,(size_t)n*sizeof(double));return true;
}
#endif
