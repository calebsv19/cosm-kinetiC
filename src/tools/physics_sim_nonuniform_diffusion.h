/* Bounded deferred gradient correction around the retained positive BE solve.
 * Same extensive paired FCT budgets/headroom; no advection or matrix edits. */
#ifndef PHYSICS_SIM_NONUNIFORM_DIFFUSION_H
#define PHYSICS_SIM_NONUNIFORM_DIFFUSION_H
#include "physics_sim_nonuniform_advection.h"
#include "physics_sim_local_matrix.h"
#include <string.h>
typedef struct {
 PhysicsSimNonuniformAdvection limiter;
 double gradient[4096][4],old[4096],accepted_interface;
 bool enabled[4096];int enabled_faces;unsigned long long corrections,fallbacks;
} PhysicsSimNonuniformDiffusion;
static inline bool physics_sim_nonuniform_diffusion_init(PhysicsSimNonuniformDiffusion *d,int n,const double *w,int face){
 if(!physics_sim_nonuniform_advection_init(&d->limiter,n,w,face))return false;
 for(int i=0;i<n;i++){
  int ll=(i+n-2)%n,l=(i+n-1)%n,r=i,rr=(i+1)%n;
  d->enabled[i]=!d->limiter.regular[i];if(d->enabled[i])d->enabled_faces++;
  double h=(w[l]+w[r])*.5;
  double bounds[5]={-(w[ll]+w[l])/h,-w[l]/h,0,w[r]/h,(w[r]+w[rr])/h},a[4][5];
  for(int p=0;p<4;p++){
   for(int j=0;j<4;j++)a[p][j]=(pow(bounds[j+1],p+1)-pow(bounds[j],p+1))/((p+1)*(bounds[j+1]-bounds[j]));
   a[p][4]=p==1?1/h:0;
  }
  for(int col=0;col<4;col++){
   int pivot=col;for(int j=col+1;j<4;j++)if(fabs(a[j][col])>fabs(a[pivot][col]))pivot=j;
   if(fabs(a[pivot][col])<1e-15)return false;
   for(int k=0;k<5;k++){double temp=a[col][k];a[col][k]=a[pivot][k];a[pivot][k]=temp;}
   double divisor=a[col][col];for(int k=col;k<5;k++)a[col][k]/=divisor;
   for(int j=0;j<4;j++)if(j!=col){double factor=a[j][col];for(int k=col;k<5;k++)a[j][k]-=factor*a[col][k];}
  }
  for(int j=0;j<4;j++)d->gradient[i][j]=a[j][4];
 }
 return true;
}
static inline double physics_sim_nonuniform_gradient(PhysicsSimNonuniformDiffusion *d,const double *v,int i){
 PhysicsSimNonuniformAdvection *s=&d->limiter;int n=s->n,l=(i+n-1)%n;
 double central=(v[i]-v[l])/(.5*(s->width[i]+s->width[l]));
 if(!d->enabled[i])return central;
 double high=d->gradient[i][0]*(v[(i+n-2)%n]-v[l])+d->gradient[i][2]*(v[i]-v[l])+d->gradient[i][3]*(v[(i+1)%n]-v[l]);
 /* Same reconstruction guard as the retained cold-front target: retain
  * physical two-cell transfer at exact zeros or a wrong-sign high gradient. */
 if(!isfinite(high)||(central>0&&high<=0)||(central<0&&high>=0)){d->fallbacks++;return central;}
 return fmax(fmin(0,2*central),fmin(fmax(0,2*central),high));
}
static inline bool physics_sim_nonuniform_diffusion_step(PhysicsSimNonuniformDiffusion *d,PhysicsSimLocalMatrix *matrix,double *v,double alpha,double dt){
 if(!isfinite(alpha)||alpha<0||!isfinite(dt)||dt<=0)return false;
 PhysicsSimNonuniformAdvection *s=&d->limiter;int n=s->n;
 memcpy(d->old,v,(size_t)n*sizeof(double));
 if(!physics_sim_local_matrix_step(matrix,v))return false;
 int face=s->interface,l=(face+n-1)%n;
 double low_interface=-dt*alpha*(v[face]-v[l])/(.5*(s->width[face]+s->width[l]));
 if(!d->enabled_faces){d->accepted_interface+=low_interface;return true;}
 double lo=d->old[0],hi=d->old[0];
 for(int j=0;j<n;j++){lo=fmin(lo,d->old[j]);hi=fmax(hi,d->old[j]);}
 for(int i=0;i<n;i++){
  int left=(i+n-1)%n;
  double central=(v[i]-v[left])/(.5*(s->width[i]+s->width[left]));
  s->correction[i]=d->enabled[i]?(-dt*alpha/s->unit_width)*(physics_sim_nonuniform_gradient(d,v,i)-central):0;
 }
 for(int i=0;i<n;i++){
  int right=(i+1)%n;double capacity=s->capacity[i];
  double gain=fmax(0,s->correction[i])+fmax(0,-s->correction[right]);
  double loss=fmax(0,-s->correction[i])+fmax(0,s->correction[right]);
  s->plus[i]=gain>0?fmax(0,fmin(1,capacity*(hi-v[i])/gain)):1;
  s->minus[i]=loss>0?fmax(0,fmin(1,capacity*(v[i]-lo)/loss)):1;
  if(s->plus[i]<1)s->plus[i]*=1-16*DBL_EPSILON;
  if(s->minus[i]<1)s->minus[i]*=1-16*DBL_EPSILON;
 }
 double accepted=0;
 for(int i=0;i<n;i++){
  int left=(i+n-1)%n;double a=s->correction[i];
  double factor=a>=0?fmin(s->plus[i],s->minus[left]):fmin(s->minus[i],s->plus[left]);
  a*=factor;v[i]+=a/s->capacity[i];v[left]-=a/s->capacity[left];
  if(i==face)accepted=a*s->unit_width;
 }
 if(!physics_sim_nonuniform_bounds(s,d->old,v))return false;
 d->accepted_interface+=low_interface+accepted;d->corrections++;return true;
}
#endif
