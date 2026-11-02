/* Unequal-volume adapter for the retained paired conservative FCT algorithm.
 * Face transfers/budgets use width*T; temperatures retain the same bounds.
 * Four-cell finite-volume polynomial weights account for actual cell widths. */
#ifndef PHYSICS_SIM_NONUNIFORM_ADVECTION_H
#define PHYSICS_SIM_NONUNIFORM_ADVECTION_H
#include <math.h>
#include <stdbool.h>
#include <float.h>
#include <stdio.h>
#include "physics_sim_tail_fit.h"
typedef struct {
 int n;const double *width;double unit_width,capacity[4096],weight[4096][4];bool regular[4096];
 double low[4096],correction[4096],plus[4096],minus[4096],stage[4096],out[4096];
 unsigned long long checks,roundoff_cells,representable_faces;double max_excursion,advected_interface;
 int interface;
} PhysicsSimNonuniformAdvection;
static inline bool physics_sim_nonuniform_advection_init(PhysicsSimNonuniformAdvection *s,int n,const double *w,int face){
 if(!s||!w||n<4||n>4096)return false;
 s->n=n;s->width=w;s->interface=face;
 s->unit_width=w[0];
 for(int i=0;i<n;i++){if(!isfinite(w[i])||w[i]<=0)return false;s->unit_width=fmin(s->unit_width,w[i]);}
 /* Common finest-cell heat unit: capacities are >=1 (here exactly1 or32).
  * This is a unit conversion, not a heat floor or a changed limiter budget. */
 for(int i=0;i<n;i++)s->capacity[i]=w[i]/s->unit_width;
 for(int i=0;i<n;i++){
  int ll=(i+n-2)%n,l=(i+n-1)%n,r=i,rr=(i+1)%n;
  s->regular[i]=w[ll]==w[l]&&w[l]==w[r]&&w[r]==w[rr];
  double h=(w[l]+w[r])*.5;
  double bounds[5]={-(w[ll]+w[l])/h,-w[l]/h,0,w[r]/h,(w[r]+w[rr])/h};
  double a[4][5];
  for(int p=0;p<4;p++){
   for(int j=0;j<4;j++)a[p][j]=(pow(bounds[j+1],p+1)-pow(bounds[j],p+1))/((p+1)*(bounds[j+1]-bounds[j]));
   a[p][4]=p==0;
  }
  for(int col=0;col<4;col++){
   int pivot=col;for(int j=col+1;j<4;j++)if(fabs(a[j][col])>fabs(a[pivot][col]))pivot=j;
   if(fabs(a[pivot][col])<1e-15)return false;
   for(int k=0;k<5;k++){double temp=a[col][k];a[col][k]=a[pivot][k];a[pivot][k]=temp;}
   double divisor=a[col][col];for(int k=col;k<5;k++)a[col][k]/=divisor;
   for(int j=0;j<4;j++)if(j!=col){double factor=a[j][col];for(int k=col;k<5;k++)a[j][k]-=factor*a[col][k];}
  }
  for(int j=0;j<4;j++)s->weight[i][j]=a[j][4];
 }
 return true;
}
static inline double physics_sim_nonuniform_face(const PhysicsSimNonuniformAdvection *s,const double *v,int i){
 int n=s->n,ll=(i+n-2)%n,l=(i+n-1)%n,r=i,rr=(i+1)%n;
 if(s->regular[i])return (-v[ll]+7*v[l]+7*v[r]-v[rr])/12;
 /* Difference form preserves a constant profile exactly. */
 return v[l]+s->weight[i][0]*(v[ll]-v[l])+s->weight[i][2]*(v[r]-v[l])+s->weight[i][3]*(v[rr]-v[l]);
}
/* Opt-in positive finite-volume tail face. Same paired FCT budgets;
 * exact-zero/unsupported fits retain transport through a nonnegative
 * polynomial or the physical upwind donor, never a heat floor. */
static inline double physics_sim_nonuniform_tail_face(const PhysicsSimNonuniformAdvection *s,const double *v,int i,double u){
 int n=s->n,ll=(i+n-2)%n,l=(i+n-1)%n,r=i,rr=(i+1)%n;
 double value=physics_sim_nonuniform_face(s,v,i),fit,gradient;
 bool steep=v[l]>0&&v[r]>0&&(v[l]>2*v[r]||v[r]>2*v[l]);
 if(!s->regular[i]||steep){
  double means[4]={v[ll],v[l],v[r],v[rr]},widths[4]={s->width[ll],s->width[l],s->width[r],s->width[rr]};
  if(physics_sim_tail_profile_fit(means,widths,&fit,&gradient)&&isfinite(fit)&&fit>=0)return fit;
 }
 double maximum=fmax(fmax(v[ll],v[l]),fmax(v[r],v[rr]));
 return isfinite(value)&&value>=0&&value<=maximum?value:(u>=0?v[l]:v[r]);
}
static inline bool physics_sim_nonuniform_bounds(PhysicsSimNonuniformAdvection *s,const double *old,const double *next){
 double lo=old[0],hi=old[0];for(int j=0;j<s->n;j++){lo=fmin(lo,old[j]);hi=fmax(hi,old[j]);}
 double rounding=64*DBL_EPSILON*fmax(fabs(lo),fabs(hi));
 for(int j=0;j<s->n;j++){
  if(!isfinite(next[j])||next[j]<0||next[j]>.4+1e-10||next[j]<lo-rounding||next[j]>hi+rounding){
   fprintf(stderr,"unequal bounds refusal stage=%llu cell=%d next=%a lo=%a hi=%a\n",s->checks,j,next[j],lo,hi);return false;
  }
  if(next[j]<lo||next[j]>hi){s->roundoff_cells++;s->max_excursion=fmax(s->max_excursion,fmax(lo-next[j],next[j]-hi));}
 }
 s->checks++;return true;
}
/* At subnormal precision, a/C can round up after budget multiplication.
 * Retain the limiter factors; reduce the paired face transfer by representable
 * ulps until its actual two-cell update cannot overdraw either donor. */
static inline bool physics_sim_nonuniform_apply(PhysicsSimNonuniformAdvection *s,double *v,int left,int right,double *a){
 double l=v[left]-*a/s->capacity[left],r=v[right]+*a/s->capacity[right];
 if(l<0||r<0){
  unsigned tries=0;
  do{
   *a=nextafter(*a,0);l=v[left]-*a/s->capacity[left];r=v[right]+*a/s->capacity[right];
  }while((l<0||r<0)&&++tries<64);
  if(l<0||r<0)return false;
  s->representable_faces++;
 }
 v[right]=r;v[left]=l;return true;
}
static inline bool physics_sim_nonuniform_euler(PhysicsSimNonuniformAdvection *s,const double *v,double *out,double u,double dt,int method,double *interface_flux){
 int n=s->n;double lo=v[0],hi=v[0];
 for(int j=0;j<n;j++){
  if(!isfinite(v[j])||v[j]<0||fabs(u)*dt>s->width[j])return false;
  lo=fmin(lo,v[j]);hi=fmax(hi,v[j]);
 }
 for(int i=0;i<n;i++){
  int l=(i+n-1)%n;
  s->low[i]=(dt*u/s->unit_width)*(u>=0?v[l]:v[i]);
  s->correction[i]=method>=2?(dt*u/s->unit_width)*(method==3?physics_sim_nonuniform_tail_face(s,v,i,u):physics_sim_nonuniform_face(s,v,i))-s->low[i]:0;
 }
 for(int i=0;i<n;i++){
  int right=(i+1)%n;double capacity=s->capacity[i];
  out[i]=v[i]+(s->low[i]-s->low[right])/capacity;
  double gain=fmax(0,s->correction[i])+fmax(0,-s->correction[right]);
  double loss=fmax(0,-s->correction[i])+fmax(0,s->correction[right]);
  s->plus[i]=gain>0?fmax(0,fmin(1,capacity*(hi-out[i])/gain)):1;
  s->minus[i]=loss>0?fmax(0,fmin(1,capacity*(out[i]-lo)/loss)):1;
  if(s->plus[i]<1)s->plus[i]*=1-16*DBL_EPSILON;
  if(s->minus[i]<1)s->minus[i]*=1-16*DBL_EPSILON;
 }
 for(int i=0;i<n;i++){
  int left=(i+n-1)%n;double a=s->correction[i];
  double factor=a>=0?fmin(s->plus[i],s->minus[left]):fmin(s->minus[i],s->plus[left]);
  a*=factor;if(!physics_sim_nonuniform_apply(s,out,left,i,&a))return false;
  if(i==s->interface)*interface_flux=s->low[i]+a;
 }
 return physics_sim_nonuniform_bounds(s,v,out);
}
static inline bool physics_sim_nonuniform_advection_step(PhysicsSimNonuniformAdvection *s,double *v,double u,double dt,int method){
 if(!isfinite(u)||!isfinite(dt)||dt<=0||(method!=1&&method!=2&&method!=3))return false;
 if(u==0)return true;
 double first=0,second=0;
 if(!physics_sim_nonuniform_euler(s,v,s->stage,u,dt,method,&first)||
    !physics_sim_nonuniform_euler(s,s->stage,s->out,u,dt,method,&second))return false;
 for(int j=0;j<s->n;j++)s->out[j]=.5*(v[j]+s->out[j]);
 if(!physics_sim_nonuniform_bounds(s,v,s->out))return false;
 for(int j=0;j<s->n;j++)v[j]=s->out[j];
 s->advected_interface+=.5*(first+second)*s->unit_width;return true;
}
#endif
