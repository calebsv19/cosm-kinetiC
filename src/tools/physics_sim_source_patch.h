/* Within-one-tick diffusion of a separable binary rectangular source.
 * Integrate both axes at the SAME heat age; independent source increments
 * would double count heat or use independent ages. No evolving state replaced. */
#ifndef PHYSICS_SIM_SOURCE_PATCH_H
#define PHYSICS_SIM_SOURCE_PATCH_H
#include "app/cfd_memory.h"
#include <math.h>
#include <stdbool.h>
#include <float.h>
typedef struct {int nx,nz;double dt,*delta;} PhysicsSimSourcePatch;
static inline double physics_sim_source_gaussian_primitive(double x){
 double pi=acos(-1);
 if(x>=8){double y=1/(x*x),term=-.5*y,sum=term;for(int m=1;m<16;m++){term*=-(2.*m+1)*y/2;sum+=term;}return exp(-x*x)/sqrt(pi)*sum;}
 return x*erfc(x)-exp(-x*x)/sqrt(pi);
}
static inline bool physics_sim_source_instant(int n,const double *w,const double *power,double alpha,double age,bool periodic,double *out){
 if(n<16||n>4096||!isfinite(age)||age<=0)return false;
 for(int j=0;j<n;j++){if(!isfinite(w[j])||w[j]<=0||(power[j]!=0&&power[j]!=1))return false;out[j]=power[j];}
 double scale=sqrt(4*alpha*age);if(!isfinite(scale)||scale<=0)return false;
 for(int i=periodic?0:1;i<n;i++){
  int l=(i+n-1)%n;double jump=power[l]-power[i];if(!jump)continue;
  if(!periodic&&(i<4||i+4>n))return false;
  double h=w[l],omitted=fabs(physics_sim_source_gaussian_primitive(4*h/scale)/physics_sim_source_gaussian_primitive(0));
  if(!isfinite(omitted)||omitted>1e-12)return false;
  for(int k=0;k<4;k++){
   int left=(i+n-1-k)%n,right=(i+k)%n;if(w[left]!=h||w[right]!=h)return false;
   double value=.5*scale/h*(physics_sim_source_gaussian_primitive((k+1)*h/scale)-physics_sim_source_gaussian_primitive(k*h/scale));
   if(!isfinite(value)||value<0)return false;out[left]-=jump*value;out[right]+=jump*value;
  }
 }
 for(int j=0;j<n;j++)if(!isfinite(out[j])||out[j]<0||out[j]>1)return false;
 return true;
}
static inline bool physics_sim_source_patch_prepare(PhysicsSimSourcePatch *out,int nx,int nz,const double *wx,const double *wz,const double *px,const double *pz,double alpha,double dt,int quadrature){
 if(!out||!wx||!wz||!px||!pz||out->delta||nx<16||nx>4096||nz<16||nz>256||!isfinite(alpha)||alpha<=0||!isfinite(dt)||dt<=0||(quadrature!=64&&quadrature!=96))return false;
 double *delta=cfd_memory_calloc((size_t)nx*nz,sizeof(double));if(!delta)return false;
 PhysicsSimSourcePatch next={nx,nz,dt,delta};double x[96],weight[96],sum=0,hx[4096],hz[256];
 for(int j=0;j<nx;j++)for(int k=0;k<nz;k++)delta[k*nx+j]=dt*px[j]*pz[k];
 for(int j=0;j<quadrature;j++){
  double z=cos(acos(-1)*(j+.75)/(quadrature+.5)),derivative=0;
  for(int it=0;it<16;it++){double a=1,b=z;for(int m=2;m<=quadrature;m++){double c=((2.*m-1)*z*b-(m-1)*a)/m;a=b;b=c;}derivative=quadrature*(z*b-a)/(z*z-1);double change=b/derivative;z-=change;if(fabs(change)<4*DBL_EPSILON)break;}
  x[j]=(z+1)/2;weight[j]=2*x[j]/((1-z*z)*derivative*derivative);sum+=weight[j];
 }
 double used=0;for(int j=0;j<quadrature;j++){weight[j]=j==quadrature-1?1-used:weight[j]/sum;used+=weight[j];if(weight[j]<=0)goto fail;}
 for(int q=0;q<quadrature;q++){
  double age=dt*x[q]*x[q];
  if(!physics_sim_source_instant(nx,wx,px,alpha,age,true,hx)||!physics_sim_source_instant(nz,wz,pz,alpha,age,false,hz))goto fail;
  for(int k=0;k<nz;k++)for(int j=0;j<nx;j++)delta[k*nx+j]+=dt*weight[q]*(hx[j]*hz[k]-px[j]*pz[k]);
 }
 for(int k=0;k<nz;k++)for(int j=0;j<nx;j++)if(!isfinite(delta[k*nx+j])||delta[k*nx+j]<0||delta[k*nx+j]>dt)goto fail;
 *out=next;return true;
 fail:cfd_memory_free(delta);return false;
}
static inline bool physics_sim_source_patch_add(const PhysicsSimSourcePatch *s,double *v,double dt){
 if(!s||!s->delta||!v||dt!=s->dt)return false;
 size_t count=(size_t)s->nx*s->nz;
 for(size_t j=0;j<count;j++)if(!isfinite(v[j])||v[j]<0||!isfinite(v[j]+s->delta[j])||v[j]+s->delta[j]>.4+1e-10)return false;
 for(size_t j=0;j<count;j++)v[j]+=s->delta[j];return true;
}
static inline void physics_sim_source_patch_destroy(PhysicsSimSourcePatch *s){if(s){cfd_memory_free(s->delta);s->delta=NULL;}}
#endif
