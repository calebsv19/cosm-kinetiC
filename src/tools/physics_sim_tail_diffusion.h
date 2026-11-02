/* Opt-in early-tail candidate: positive CN base plus conservative bounded
 * finite-volume sixth-order regular-face/cubic unequal-face correction, with
 * positive local exponential/log-quadratic fits at steep faces. Retains existing paired FCT budgets.
 * Caller matrix is initialized at dt/2; no source, wall law or clock is hidden. */
#ifndef PHYSICS_SIM_TAIL_DIFFUSION_H
#define PHYSICS_SIM_TAIL_DIFFUSION_H
#include "physics_sim_nonuniform_diffusion.h"
#include "physics_sim_tail_fit.h"
#include "physics_sim_heater_edge.h"
typedef struct {PhysicsSimNonuniformDiffusion stencil;double widths[4096];bool periodic;const double *heater_power;double heater_alpha;} PhysicsSimTailDiffusion;
static inline bool physics_sim_tail_diffusion_init(PhysicsSimTailDiffusion *s,int n,const double *w,bool periodic){
 if(!s||!w||n<4||n>4096)return false;s->heater_power=NULL;s->heater_alpha=0;memcpy(s->widths,w,(size_t)n*sizeof(double));
 if(!physics_sim_nonuniform_diffusion_init(&s->stencil,n,s->widths,0))return false;s->periodic=periodic;return true;
}
/* Optional 1-D source-edge reconstruction metadata. Power storage must
 * remain valid and unchanged through each step; invalid setup is atomic. */
static inline bool physics_sim_tail_heater_source(PhysicsSimTailDiffusion *s,const double *power,double alpha){
 if(!s||!power||!isfinite(alpha)||alpha<=0||s->stencil.limiter.n<4||s->stencil.limiter.n>4096)return false;
 for(int j=0;j<s->stencil.limiter.n;j++)if(!isfinite(power[j])||power[j]<0)return false;
 s->heater_power=power;s->heater_alpha=alpha;return true;
}
static inline int physics_sim_tail_index(int i,int n,bool periodic){
 if(periodic)return (i+n)%n;if(i<0)return -i-1;if(i>=n)return 2*n-i-1;return i;
}
static inline double physics_sim_tail_gradient(PhysicsSimTailDiffusion *s,const double *v,int i,int high){
 PhysicsSimNonuniformDiffusion *d=&s->stencil;PhysicsSimNonuniformAdvection *a=&d->limiter;int n=a->n,l=(i+n-1)%n;
 double central=(v[i]-v[l])/(.5*(a->width[i]+a->width[l]));
 if(!high)return central;
 int index[6];bool regular=true;for(int j=0;j<6;j++){index[j]=physics_sim_tail_index(i+j-3,n,s->periodic);if(a->width[index[j]]!=a->width[l])regular=false;}
 if(high>1&&s->heater_power&&regular){
  double jump=s->heater_power[i]-s->heater_power[l];
  double values[4]={v[index[1]],v[l],v[i],v[index[4]]},widths[4]={a->width[index[1]],a->width[l],a->width[i],a->width[index[4]]},fit;
  if(physics_sim_heater_edge_fit(values,widths,jump,s->heater_alpha,&fit))return fit;
 }
 double target;
 if(high>1&&regular)target=(-2*(v[index[0]]-v[l])+25*(v[index[1]]-v[l])+245*(v[i]-v[l])-25*(v[index[4]]-v[l])+2*(v[index[5]]-v[l]))/(180*a->width[l]);
 else target=d->gradient[i][0]*(v[index[1]]-v[l])+d->gradient[i][2]*(v[i]-v[l])+d->gradient[i][3]*(v[index[4]]-v[l]);
 /* A polynomial in temperature poorly resolves more than a factor-two
  * change per cell. Use the existing positive cell-mean fit there as well
  * as at unequal faces. Exact zeros retain the cold-cell transport path.
  * This is reconstruction selection, not a physical or acceptance limit. */
 bool steep=v[l]>0&&v[i]>0&&(v[l]>2*v[i]||v[i]>2*v[l]);
 if(high>1&&(!regular||steep)){double values[4]={v[index[1]],v[l],v[i],v[index[4]]},widths[4]={a->width[index[1]],a->width[l],a->width[i],a->width[index[4]]},fit;if((physics_sim_tail_fit(values,widths,&fit)||physics_sim_tail_exp_fit(values[1],values[2],widths[1],widths[2],&fit))&&isfinite(fit)&&((fit>0&&central>0)||(fit<0&&central<0)))return fit;}
 if(!isfinite(target)||(central>0&&target<=0)||(central<0&&target>=0))return central;
 return fmax(fmin(0,2*central),fmin(fmax(0,2*central),target));
}
/* Preserve paired heat, admit representable actual two-cell temperatures.
 * Continuous headroom and existing FCT factors are retained; only over-budget
 * or rounding-up transfer is reduced. No cell clipping/heat floor. */
static inline bool physics_sim_tail_apply(PhysicsSimNonuniformAdvection *a,double *v,int l,int r,double lo,double hi,double *q){
 double bound=*q>=0?fmin(a->capacity[l]*(v[l]-lo),a->capacity[r]*(hi-v[r])):fmin(a->capacity[r]*(v[r]-lo),a->capacity[l]*(hi-v[l]));
 if(!isfinite(*q)||bound<0)return false;
 if(fabs(*q)>bound)*q=copysign(bound,*q);
 for(int attempt=0;attempt<65;attempt++){
  double left=v[l]-*q/a->capacity[l],right=v[r]+*q/a->capacity[r];
  if(left>=lo&&left<=hi&&right>=lo&&right<=hi){v[l]=left;v[r]=right;return true;}
  *q=nextafter(*q,0);a->representable_faces++;
 }return false;
}
static inline bool physics_sim_tail_diffusion_step(PhysicsSimTailDiffusion *s,PhysicsSimLocalMatrix *matrix,double *v,double alpha,double dt,int high){
 if(high<0||high>2)return false;
 if(!s||!matrix||!v||!isfinite(alpha)||alpha<0||!isfinite(dt)||dt<=0)return false;
 PhysicsSimNonuniformDiffusion *d=&s->stencil;PhysicsSimNonuniformAdvection *a=&d->limiter;int n=a->n;
 if(matrix->n!=n)return false;memcpy(d->old,v,(size_t)n*sizeof(double));
 for(int j=0;j<n;j++){
  int l=(j+n-1)%n,r=(j+1)%n;double left=(s->periodic||j>0)?dt*alpha/(a->width[j]*(a->width[j]+a->width[l])):0;
  double right=(s->periodic||j<n-1)?dt*alpha/(a->width[j]*(a->width[j]+a->width[r])):0;
  if(left+right>1||!isfinite(v[j])||v[j]<0)return false;
  a->stage[j]=(1-left-right)*v[j]+left*v[l]+right*v[r];
 }
 if(!physics_sim_local_matrix_step(matrix,a->stage))return false;
 double lo=d->old[0],hi=d->old[0];for(int j=0;j<n;j++){lo=fmin(lo,d->old[j]);hi=fmax(hi,d->old[j]);a->out[j]=d->old[j];}
 for(int i=0;i<n;i++){
  int l=(i+n-1)%n;double h=(a->width[i]+a->width[l])*.5;
  a->low[i]=(!s->periodic&&i==0)?0:-.5*dt*alpha/a->unit_width*(d->old[i]-d->old[l]+a->stage[i]-a->stage[l])/h;
 }
 for(int i=0;i<n;i++){int l=(i+n-1)%n;if(!physics_sim_tail_apply(a,a->out,l,i,lo,hi,&a->low[i]))return false;}
 for(int i=0;i<n;i++){
  double target=physics_sim_tail_gradient(s,d->old,i,high)+physics_sim_tail_gradient(s,a->stage,i,high);
  a->correction[i]=(!s->periodic&&i==0)?0:-.5*dt*alpha/a->unit_width*target-a->low[i];
 }
 for(int j=0;j<n;j++){
  int r=(j+1)%n;double gain=fmax(0,a->correction[j])+fmax(0,-a->correction[r]),loss=fmax(0,-a->correction[j])+fmax(0,a->correction[r]);
  a->plus[j]=gain>0?fmax(0,fmin(1,a->capacity[j]*(hi-a->out[j])/gain)):1;
  a->minus[j]=loss>0?fmax(0,fmin(1,a->capacity[j]*(a->out[j]-lo)/loss)):1;
  if(a->plus[j]<1)a->plus[j]*=1-16*DBL_EPSILON;if(a->minus[j]<1)a->minus[j]*=1-16*DBL_EPSILON;
 }
 for(int i=0;i<n;i++){
  int l=(i+n-1)%n;double transfer=a->correction[i],factor=transfer>=0?fmin(a->plus[i],a->minus[l]):fmin(a->minus[i],a->plus[l]);
  transfer*=factor;if(!physics_sim_tail_apply(a,a->out,l,i,lo,hi,&transfer))return false;
 }
 if(!physics_sim_nonuniform_bounds(a,d->old,a->out))return false;
 memcpy(v,a->out,(size_t)n*sizeof(double));return true;
}
#endif
