/* Bounded applicability probe: no numerical algorithm changes. History0 on,
 * 1 delayed240,2 pulse240,3 pulse240/restart480. Route0 safe: after any shutoff,
 * retain generic diffusion. Route1 forces the documented continuous-source
 * closure out of contract, exclusively as a labeled applicability diagnostic. Route2 uses explicit on/off age intervals. */
#include "physics_sim_source_age_intervals.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static PhysicsSimTailDiffusion s;static PhysicsSimTailTime rk;static PhysicsSimSourceAge age;
static PhysicsSimSourceAgeHistory history_clock;static PhysicsSimSourceAgeInterval intervals[PHYSICS_SIM_SOURCE_AGE_MAX_INTERVALS];
static PhysicsSimSourceInterval source;static double w[256],power[256],v[256],zero[256],saved[256];
static bool active(int history,int tick){return history==0||(history==1?tick>=240:history==2?tick<240:tick<240||tick>=480);}
int main(int argc,char **argv){if(argc!=3&&argc!=4)return 2;int history=atoi(argv[1]),route=atoi(argv[2]);if(history<0||history>3||route<0||route>2)return 2;
int finish=argc==4?atoi(argv[3]):1280;if(finish!=320&&finish!=560&&finish!=1280)return 2;
const double dt=1./51200,alpha=.026/1206;bool seen_on=false,interrupted=false;int heating=0;
for(int j=0;j<256;j++){w[j]=.0625/256;power[j]=j>=64&&j<128;}
assert(physics_sim_tail_diffusion_init(&s,256,w,false));assert(physics_sim_tail_heater_source(&s,power,alpha));assert(physics_sim_source_interval_prepare_complete(&source,256,w,power,alpha,dt,false));
if(route==2)assert(physics_sim_source_age_history_init(&history_clock,dt));
for(int tick=0;tick<finish;tick++){
 bool on=active(history,tick);if(seen_on&&!on)interrupted=true;if(on)seen_on=true;
 if(!route&&interrupted&&tick==240){
  memcpy(saved,v,sizeof v);assert(physics_sim_tail_heater_source(&s,zero,alpha));
  assert(!physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));assert(!memcmp(saved,v,sizeof v));
  assert(physics_sim_tail_heater_source(&s,power,alpha));
 }
 if(route==2){int count;assert(physics_sim_source_age_history_record(&history_clock,(uint64_t)tick,on));assert(physics_sim_source_age_history_intervals(&history_clock,(uint64_t)tick,intervals,&count));assert(physics_sim_source_age_intervals_step(&s,&rk,&age,v,alpha,dt,intervals,count));}
 else if(route||!interrupted)assert(physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));else assert(physics_sim_tail_time_step(&s,&rk,v,alpha,dt,3));
 if(on){assert(physics_sim_source_interval_add(&source,v,dt));heating++;}
 int t=tick+1;if(t==80||t==240||t==320||t==480||t==560||t==720||t==1280||t==finish){
  double mass=0;for(int j=0;j<256;j++){assert(isfinite(v[j])&&v[j]>=0&&v[j]<=.4+1e-10);mass+=w[j]*v[j];}
  printf("{\"history\":%d,\"route\":%d,\"tick\":%d,\"interrupted\":%d,\"mass\":%.17g,\"expected_mass\":%.17g,\"values\":[",history,route,t,interrupted,mass,heating*dt/64);for(int j=0;j<256;j++)printf("%s%.17g",j?",":"",v[j]);puts("]}");
 }
}return 0;}
