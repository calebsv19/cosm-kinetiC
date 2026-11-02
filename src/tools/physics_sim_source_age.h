/* Opt-in 1-D continuously powered unit rectangular-source reconstruction.
 * Uniform reflecting grid, one source block, constant alpha. Infer heating age
 * from evolved edge means; use actual adjacent differences for flux amplitude.
 * Bounded cold extrapolation uses at most eight warm-side means. Source addition
 * remains external. No reference field, state replacement or private clock.
 * Process largest transfers first so incoming heat precedes smaller outgoing
 * transfers, refreshing the unchanged one-face FCT budgets at each stage.
 * Not qualified for advection, source shutoff, arbitrary source histories or2-D. */
#ifndef PHYSICS_SIM_SOURCE_AGE_H
#define PHYSICS_SIM_SOURCE_AGE_H
#include "physics_sim_tail_time.h"
typedef struct {double model[4096],flux[4096];int order[4096];} PhysicsSimSourceAge;
static inline double physics_sim_source_age_kernel(double t,double distance,double alpha){
 if(t<=0)return 0;double x=fabs(distance)/sqrt(4*alpha*t),bracket;
 if(x>=8){double y=1/(x*x),term=.5*y,sum=term;for(int k=1;k<16;k++){term*=-(2*k+1)*.5*y;sum+=term;}bracket=exp(-x*x)*sum;}
 else bracket=exp(-x*x)-sqrt(acos(-1))*x*erfc(x);
 return sqrt(t/(acos(-1)*alpha))*bracket;
}
static inline double physics_sim_source_age_cold(double t,double lo,double hi,double alpha){
 if(t<=0)return 0;double scale=sqrt(4*alpha*t);
 return .5*t*scale/(hi-lo)*(physics_sim_source_primitive(hi/scale)-physics_sim_source_primitive(lo/scale));
}
static inline double physics_sim_source_age_slab_mean(double t,double lo,double hi,double a,double b,double alpha){
 if(hi<=a)return physics_sim_source_age_cold(t,a-hi,a-lo,alpha)-physics_sim_source_age_cold(t,b-hi,b-lo,alpha);
 if(lo>=b)return physics_sim_source_age_cold(t,lo-b,hi-b,alpha)-physics_sim_source_age_cold(t,lo-a,hi-a,alpha);
 if(lo>=a&&hi<=b)return t-physics_sim_source_age_cold(t,lo-a,hi-a,alpha)-physics_sim_source_age_cold(t,b-hi,b-lo,alpha);
 return NAN;
}
static inline double physics_sim_source_age_mean(double young,double old,double lo,double hi,double a,double b,double height,double alpha){
 double sum=0;for(int m=-1;m<=1;m++){
  double aa[2]={a+2*m*height,-b+2*m*height},bb[2]={b+2*m*height,-a+2*m*height};
  for(int k=0;k<2;k++)sum+=physics_sim_source_age_slab_mean(old,lo,hi,aa[k],bb[k],alpha)-physics_sim_source_age_slab_mean(young,lo,hi,aa[k],bb[k],alpha);
 }return sum;
}
static inline double physics_sim_source_age_gradient(double young,double old,double x,double a,double b,double height,double alpha){
 double sum=0;for(int m=-1;m<=1;m++){
  double aa[2]={a+2*m*height,-b+2*m*height},bb[2]={b+2*m*height,-a+2*m*height};
  for(int k=0;k<2;k++)sum+=physics_sim_source_age_kernel(old,x-aa[k],alpha)-physics_sim_source_age_kernel(young,x-aa[k],alpha)-physics_sim_source_age_kernel(old,x-bb[k],alpha)+physics_sim_source_age_kernel(young,x-bb[k],alpha);
 }return sum;
}
static inline bool physics_sim_source_age_infer(PhysicsSimTailDiffusion *s,const double *v,double alpha,double ages[2],int *start,int *end){
 int n=s->stencil.limiter.n;if(s->periodic||!s->heater_power)return false;*start=-1;*end=-1;
 for(int i=0;i<n;i++){
  if(s->widths[i]!=s->widths[0]||(s->heater_power[i]!=0&&s->heater_power[i]!=1))return false;
  if(s->heater_power[i]&&(i==0||!s->heater_power[i-1])){if(*start>=0)return false;*start=i;}
  if(!s->heater_power[i]&&i>0&&s->heater_power[i-1])*end=i;
 }
 if(*start<2||*end<=*start||*end>n-2)return false;
 double values[4];
 for(int j=0;j<4;j++){values[j]=v[*start+j-2];}
 double h=s->widths[0],unit=h*h/(4*alpha),inner=(values[2]-values[1])/unit;
 if(!(inner>0)||!isfinite(inner))return false;
 double lo=0,hi=4096;if(physics_sim_heater_pair(hi,0)<=inner)return false;
 for(int k=0;k<60;k++){double mid=(lo+hi)/2;if(physics_sim_heater_pair(mid,0)<inner)lo=mid;else hi=mid;}
 ages[0]=0;ages[1]=.5*(lo+hi)*unit;return true;
}
static inline double physics_sim_source_age_integrated_gradient(double young,double old,double x,double a,double b,double height,double alpha,double dt){
 static const double nodes[8]={-.9602898564975363,-.7966664774136267,-.5255324099163290,-.1834346424956498,.1834346424956498,.5255324099163290,.7966664774136267,.9602898564975363};
 static const double weights[8]={.1012285362903763,.2223810344533745,.3137066458778873,.3626837833783620,.3626837833783620,.3137066458778873,.2223810344533745,.1012285362903763};
 int panels=(int)ceil(180*dt/(old+dt));if(panels<1)panels=1;double sum=0;
 for(int k=0;k<panels;k++)for(int j=0;j<8;j++){double t=dt*(k+.5+.5*nodes[j])/panels;sum+=weights[j]*physics_sim_source_age_gradient(young+t,old+t,x,a,b,height,alpha)/(2*panels);}
 return dt*sum;
}
static inline bool physics_sim_source_age_step(PhysicsSimTailDiffusion *s,PhysicsSimTailTime *rk,PhysicsSimSourceAge *work,double *v,double alpha,double dt){
 if(!s||!rk||!work||!v||!isfinite(alpha)||alpha<=0||!isfinite(dt)||dt<=0||s->stencil.limiter.n<16||s->stencil.limiter.n>4096)return false;
 double *model=work->model,*flux=work->flux;int *order=work->order;double ages[2];int start,end,n=s->stencil.limiter.n;
 /* Reject unsupported geometry/source before touching the field. */
 if(s->periodic||!s->heater_power||!isfinite(s->widths[0])||s->widths[0]<=0)return false;
 int begins=0,finishes=0;
 for(int j=0;j<n;j++){
  if(s->widths[j]!=s->widths[0]||(s->heater_power[j]!=0&&s->heater_power[j]!=1)||!isfinite(v[j])||v[j]<0||v[j]>.4+1e-10)return false;
  if(s->heater_power[j]&&(j==0||s->heater_power[j-1]==0)){if(j<2)return false;begins++;}
  if(j>0&&s->heater_power[j]==0&&s->heater_power[j-1]){if(j>n-2)return false;finishes++;}
 }
 if(begins!=1||finishes!=1||s->heater_power[n-1]!=0||2*alpha*dt/(s->widths[0]*s->widths[0])>1)return false;


 if(!physics_sim_source_age_infer(s,v,alpha,ages,&start,&end))return physics_sim_tail_time_step(s,rk,v,alpha,dt,3);
 double h=s->widths[0],height=n*h,a=start*h,b=end*h;
 for(int j=0;j<n;j++)model[j]=physics_sim_source_age_mean(ages[0],ages[1],j*h,(j+1)*h,a,b,height,alpha);
 flux[0]=0;order[0]=0;
 for(int i=1;i<n;i++){
  double delta=v[i]-v[i-1],mdelta=model[i]-model[i-1];
  double mg=physics_sim_source_age_integrated_gradient(ages[0],ages[1],i*h,a,b,height,alpha,dt),q=0;
  if(delta!=0&&mdelta!=0&&((mg>0&&mdelta>0)||(mg<0&&mdelta<0))){double fit=exp(log(fabs(mg))+log(fabs(delta))-log(fabs(mdelta)));if(isfinite(fit))q=copysign(fit,delta);}
  else if(v[i]==0&&v[i-1]==0&&mg!=0){
   int direction=mg>0?1:-1;
   for(int k=0;k<8;k++){int j=i+(direction>0?k:-1-k);if(j<0||j>=n)break;if(v[j]>0&&model[j]>0){q=mg*(v[j]/model[j]);break;}}
  }else if(delta!=0)q=dt*physics_sim_tail_gradient(s,v,i,2);
  flux[i]=-alpha*q/s->stencil.limiter.unit_width;order[i]=i;
 }
 /* All requests use the original field and same interval. Each one-face
  * stage uses the unchanged FCT calculation and actual-cell bound guard. */
 for(int i=1;i<n;i++){int z=order[i],j=i;while(j>0&&fabs(flux[order[j-1]])<fabs(flux[z])){order[j]=order[j-1];j--;}order[j]=z;}

 for(int j=0;j<n;j++){int face=order[j];if(flux[face]==0)continue;bool ok=physics_sim_tail_euler_flux(s,v,alpha,dt,3,flux,face);if(!ok)return false;}
 return true;
}

#endif
