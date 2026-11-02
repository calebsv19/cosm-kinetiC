/* Opt-in fixed-footprint 1-D on/off unit heater. The caller's scalar ticks
 * describe delivered source intervals; no reference state or private clock.
 * Each interval retains its youngest/oldest diffusion age after shutoff.
 * Positive interval sums reuse the existing FCT budgets and incoming-first
 * staging. Geometry/alpha/clock changes, moving sources and flow are excluded. */
#ifndef PHYSICS_SIM_SOURCE_AGE_INTERVALS_H
#define PHYSICS_SIM_SOURCE_AGE_INTERVALS_H
#include "physics_sim_source_age.h"
#include <stdint.h>
#define PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS 8
typedef struct {double young,old;} PhysicsSimSourceAgeInterval;
typedef struct {
 uint64_t begin[PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS],end[PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS],next_tick;
 int count;bool open;double dt;
} PhysicsSimSourceAgeHistory;
static inline bool physics_sim_source_age_history_init(PhysicsSimSourceAgeHistory *history,double dt){
 if(!history||!isfinite(dt)||dt<=0||dt>1)return false;
 PhysicsSimSourceAgeHistory next={0};next.dt=dt;*history=next;return true;
}
/* Called before homogeneous diffusion, once for each existing scalar tick.
 * Current source delivery remains external and occurs after that diffusion. */
static inline bool physics_sim_source_age_history_record(PhysicsSimSourceAgeHistory *history,uint64_t tick,bool on){
 if(!history||!isfinite(history->dt)||history->dt<=0||history->dt>1||history->count<0||history->count>PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS||(history->open&&history->count==0)||tick==UINT64_MAX||tick!=history->next_tick)return false;
 PhysicsSimSourceAgeHistory next=*history;
 if(on&&!next.open){
  if(next.count==PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS)return false;
  next.begin[next.count]=tick;next.end[next.count]=tick;next.count++;next.open=true;
 }
 if(next.open){next.end[next.count-1]=tick;if(!on)next.open=false;}
 next.next_tick=tick+1;*history=next;return true;
}
static inline bool physics_sim_source_age_history_intervals(const PhysicsSimSourceAgeHistory *history,uint64_t tick,PhysicsSimSourceAgeInterval out[PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS],int *count){
 if(!history||!out||!count||tick==UINT64_MAX||history->next_tick!=tick+1||history->count<0||history->count>PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS||!isfinite(history->dt)||history->dt<=0||history->dt>1)return false;
 PhysicsSimSourceAgeInterval next[PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS];int n=0;
 for(int k=0;k<history->count;k++){
  if(history->begin[k]>history->end[k]||history->end[k]>tick||(k&&history->begin[k]<=history->end[k-1]))return false;
  if(history->end[k]==history->begin[k])continue;
  next[n].young=(double)(tick-history->end[k])*history->dt;next[n].old=(double)(tick-history->begin[k])*history->dt;
  if(!isfinite(next[n].young)||!isfinite(next[n].old))return false;n++;
 }
 memcpy(out,next,(size_t)n*sizeof *out);*count=n;return true;
}
static inline bool physics_sim_source_age_intervals_step(PhysicsSimTailDiffusion *s,PhysicsSimTailTime *rk,PhysicsSimSourceAge *work,double *v,double alpha,double dt,const PhysicsSimSourceAgeInterval *intervals,int count){
 if(!s||!rk||!work||!v||!isfinite(alpha)||alpha<=0||!isfinite(dt)||dt<=0||count<0||count>PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS||(count&&!intervals))return false;
 int n=s->stencil.limiter.n,start=-1,end=-1;
 if(n<16||n>4096||s->periodic||!s->heater_power||!isfinite(s->widths[0])||s->widths[0]<=0)return false;
 double h=s->widths[0];if(2*alpha*dt/(h*h)>1)return false;
 for(int j=0;j<n;j++){
  if(s->widths[j]!=h||(s->heater_power[j]!=0&&s->heater_power[j]!=1)||!isfinite(v[j])||v[j]<0||v[j]>.4+1e-10)return false;
  if(s->heater_power[j]&&(j==0||!s->heater_power[j-1])){if(start>=0)return false;start=j;}
  if(j&&s->heater_power[j]==0&&s->heater_power[j-1])end=j;
 }
 if(start<2||end<=start||end>n-2||s->heater_power[n-1]!=0)return false;
 for(int k=0;k<count;k++)if(!isfinite(intervals[k].young)||!isfinite(intervals[k].old)||intervals[k].young<0||intervals[k].old<=intervals[k].young||4*alpha*intervals[k].old/(h*h)>4096||(k&&intervals[k].old>intervals[k-1].young))return false;
 if(!count)return physics_sim_tail_time_step(s,rk,v,alpha,dt,3);
 double *model=work->model,*flux=work->flux,height=n*h,a=start*h,b=end*h;
 for(int j=0;j<n;j++){
  model[j]=0;for(int k=0;k<count;k++)model[j]+=physics_sim_source_age_mean(intervals[k].young,intervals[k].old,j*h,(j+1)*h,a,b,height,alpha);
  if(!isfinite(model[j])||model[j]<0)return false;
 }
 flux[0]=0;work->order[0]=0;
 for(int i=1;i<n;i++){
  double delta=v[i]-v[i-1],mdelta=model[i]-model[i-1],mg=0,q=0;
  for(int k=0;k<count;k++)mg+=physics_sim_source_age_integrated_gradient(intervals[k].young,intervals[k].old,i*h,a,b,height,alpha,dt);
  if(delta!=0&&mdelta!=0&&((mg>0&&mdelta>0)||(mg<0&&mdelta<0))){double fit=exp(log(fabs(mg))+log(fabs(delta))-log(fabs(mdelta)));if(isfinite(fit))q=copysign(fit,delta);}
  else if(v[i]==0&&v[i-1]==0&&mg!=0){
   int direction=mg>0?1:-1;
   for(int k=0;k<8;k++){int j=i+(direction>0?k:-1-k);if(j<0||j>=n)break;if(v[j]>0&&model[j]>0){q=mg*(v[j]/model[j]);break;}}
  }else if(delta!=0)q=dt*physics_sim_tail_gradient(s,v,i,2);
  flux[i]=-alpha*q/s->stencil.limiter.unit_width;if(!isfinite(flux[i]))return false;work->order[i]=i;
 }
 /* Same admitted-flux calculation and actual bounds as continuous-age path. */
 for(int i=1;i<n;i++){int z=work->order[i],j=i;while(j>0&&fabs(flux[work->order[j-1]])<fabs(flux[z])){work->order[j]=work->order[j-1];j--;}work->order[j]=z;}
 for(int j=0;j<n;j++){int face=work->order[j];if(flux[face]==0)continue;if(!physics_sim_tail_euler_flux(s,v,alpha,dt,3,flux,face))return false;}
 return true;
}
#endif
