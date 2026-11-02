/* Experimental 1-D constant-power interval plus its within-tick diffusion.
 * Power stays on its original cells. Paired source-energy redistribution
 * accounts for conduction during dt; it never replaces an evolving field.
 * Four uniform cells per side of a power jump are required. Reject wider
 * kernels/boundary-adjacent or unequal-width jumps instead of changing clocks.
 * prepare_complete is opt-in for powers <=1 K/s: retain every representable paired increment,
 * refusing boundary/unequal support before natural underflow. No heat floor.
 * Source K/s, widths m, alpha m^2/s, dt s, delta K. */
#ifndef PHYSICS_SIM_SOURCE_INTERVAL_H
#define PHYSICS_SIM_SOURCE_INTERVAL_H
#include <math.h>
#include <stdbool.h>
#include <string.h>
typedef struct {int n;double dt,delta[4096],omitted_fraction;} PhysicsSimSourceInterval;

/* Primitive of the time-integrated half-line heat kernel. Its large-x
 * expansion avoids cancellation; floating underflow remains natural. */
static inline double physics_sim_source_primitive(double x){
 double pi=acos(-1);
 if(x>=8){double y=1/(x*x),term=-.5*y*y,sum=term;
  for(int m=2;m<12;m++){term*=-m*(2.*m+1)/(2*(m-1))*y;sum+=term;}
  return exp(-x*x)/sqrt(pi)*sum;
 }
 return (x+2*x*x*x/3)*erfc(x)-2*(x*x+1)/(3*sqrt(pi))*exp(-x*x);
}
static inline bool physics_sim_source_interval_prepare_mode(PhysicsSimSourceInterval *out,
    int n,const double *width,const double *power,double alpha,double dt,bool periodic,bool complete){
 if(!out||!width||!power||n<16||n>4096||!isfinite(alpha)||alpha<0||!isfinite(dt)||dt<=0||dt>1)return false;
 PhysicsSimSourceInterval next={.n=n,.dt=dt};double maximum=0;
 for(int j=0;j<n;j++){
  if(!isfinite(width[j])||width[j]<=0||!isfinite(power[j])||power[j]<0||!isfinite(dt*power[j]))return false;
  if(complete&&power[j]>1)return false;
  maximum=fmax(maximum,dt*power[j]);next.delta[j]=dt*power[j];
 }
 if(alpha>0)for(int i=periodic?0:1;i<n;i++){
  int l=(i+n-1)%n;double jump=power[l]-power[i];if(!jump)continue;
  if(!periodic&&(i<4||i+4>n))return false;
  double h=width[l],scale=sqrt(4*alpha*dt);
  if(!isfinite(scale)||scale<=0)return false;
  double omitted=fabs(physics_sim_source_primitive(4*h/scale)/physics_sim_source_primitive(0));
  if(!isfinite(omitted)||omitted>1e-12)return false;
  next.omitted_fraction=fmax(next.omitted_fraction,omitted);
  bool underflowed=false;
  for(int k=0;k<(complete?n:4);k++){
   double integral=.5*dt*(scale/h)*(physics_sim_source_primitive((k+1)*h/scale)-physics_sim_source_primitive(k*h/scale));
   double transfer=jump*integral;if(!isfinite(transfer)||integral<0)return false;
   if(complete&&transfer==0){underflowed=true;break;}
   if(!periodic&&(i-1-k<0||i+k>=n))return false;
   int left=(i+n-1-k)%n,right=(i+k)%n;
   if(width[left]!=h||width[right]!=h)return false;
   next.delta[left]-=transfer;next.delta[right]+=transfer;
  }
  if(complete&&!underflowed)return false;
 }
 for(int j=0;j<n;j++)if(!isfinite(next.delta[j])||next.delta[j]<0||next.delta[j]>maximum)return false;
 *out=next;return true;
}
static inline bool physics_sim_source_interval_prepare(PhysicsSimSourceInterval *out,int n,const double *width,const double *power,double alpha,double dt,bool periodic){return physics_sim_source_interval_prepare_mode(out,n,width,power,alpha,dt,periodic,false);}
static inline bool physics_sim_source_interval_prepare_complete(PhysicsSimSourceInterval *out,int n,const double *width,const double *power,double alpha,double dt,bool periodic){return physics_sim_source_interval_prepare_mode(out,n,width,power,alpha,dt,periodic,true);}
static inline bool physics_sim_source_interval_add(const PhysicsSimSourceInterval *s,double *v,double dt){
 if(!s||!v||s->n<16||s->n>4096||dt!=s->dt)return false;
 for(int j=0;j<s->n;j++)if(!isfinite(v[j])||v[j]<0||!isfinite(s->delta[j])||s->delta[j]<0||!isfinite(v[j]+s->delta[j])||v[j]+s->delta[j]>.4+1e-10)return false;
 for(int j=0;j<s->n;j++)v[j]+=s->delta[j];return true;
}
#endif
