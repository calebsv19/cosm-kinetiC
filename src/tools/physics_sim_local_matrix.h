/* Positive finite-volume periodic/open tridiagonal solve on unequal volumes.
 * Each row: (1+lower+upper)*T - lower*Tleft - upper*Tright = rhs.
 * Lower/upper are face conductance divided by local cell capacity. */
#ifndef PHYSICS_SIM_LOCAL_MATRIX_H
#define PHYSICS_SIM_LOCAL_MATRIX_H
#include "app/cfd_memory.h"
#include <math.h>
#include <stdbool.h>
typedef struct {int n;double *memory,*lower,*upper,*inverse,*p,*q,*y;} PhysicsSimLocalMatrix;
static inline void physics_sim_local_open(PhysicsSimLocalMatrix *s,double *v){
 for(int i=0;i<s->n;i++)v[i]=(v[i]+(i?s->lower[i]*v[i-1]:0))*s->inverse[i];
 for(int i=s->n-2;i>=0;i--)v[i]+=s->upper[i]*s->inverse[i]*v[i+1];
}
static inline bool physics_sim_local_matrix_init(PhysicsSimLocalMatrix *s,int n,
 const double *width,double u,double alpha,double dt,bool periodic){
 if(!s||n<4||n>4096)return false;
 s->n=n;s->memory=cfd_memory_calloc((size_t)n*6,sizeof(double));if(!s->memory)return false;
 s->lower=s->memory;s->upper=s->lower+n;s->inverse=s->upper+n;s->p=s->inverse+n;s->q=s->p+n;s->y=s->q+n;
 for(int i=0;i<n;i++){
  double left=(width[i]+width[(i+n-1)%n])*.5,right=(width[i]+width[(i+1)%n])*.5;
  double a=dt*(alpha/left+fmax(u,0))/width[i],b=dt*(alpha/right+fmax(-u,0))/width[i];
  if(!periodic&&i==0)a=0;if(!periodic&&i==n-1)b=0;
  s->lower[i]=a;s->upper[i]=b;
  s->inverse[i]=1/(1+a+b-(i?a*s->upper[i-1]*s->inverse[i-1]:0));
 }
 if(periodic){s->p[0]=s->lower[0];s->q[n-1]=s->upper[n-1];}
 physics_sim_local_open(s,s->p);physics_sim_local_open(s,s->q);return true;
}
static inline bool physics_sim_local_matrix_step(PhysicsSimLocalMatrix *s,double *v){
 int n=s->n;for(int i=0;i<n;i++){if(!isfinite(v[i])||v[i]<0)return false;s->y[i]=v[i];}
 physics_sim_local_open(s,s->y);
 double a=1-s->q[0],d=1-s->p[n-1],det=a*d-s->p[0]*s->q[n-1];if(!(det>0))return false;
 double first=(d*s->y[0]+s->p[0]*s->y[n-1])/det,last=(a*s->y[n-1]+s->q[n-1]*s->y[0])/det;
 for(int i=0;i<n;i++){v[i]=s->y[i]+s->p[i]*last+s->q[i]*first;if(!isfinite(v[i])||v[i]<0)return false;}
 return true;
}
static inline void physics_sim_local_matrix_destroy(PhysicsSimLocalMatrix *s){cfd_memory_free(s->memory);s->memory=0;}
#endif
