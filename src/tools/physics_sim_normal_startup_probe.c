/* Fixed-clock normal startup qualification; modes0 legacy,1 net precision,
 * 2 complete source,3 both,4 source-age staging. Internal cycles are diagnostic, no source retiming. */
#include "physics_sim_source_interval.h"
#include "physics_sim_source_age.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static PhysicsSimSourceAge age;static PhysicsSimTailDiffusion s;static PhysicsSimTailTime rk;static PhysicsSimSourceInterval source;
static double w[256],v[256],power[256];
int main(int argc,char **argv){if(argc!=4)return 2;int end=atoi(argv[3]);if(end!=80&&end!=240&&end!=1280)return 2;int cycles=atoi(argv[2]);if(cycles!=1&&cycles!=2&&cycles!=4&&cycles!=8&&cycles!=16&&cycles!=32)return 2;int method=atoi(argv[1]);if(method<0||method>4)return 2;double dt=1./51200,alpha=.026/1206;
for(int j=0;j<256;j++){w[j]=.0625/256;power[j]=j>=64&&j<128;}
assert(physics_sim_tail_diffusion_init(&s,256,w,false));assert(physics_sim_tail_heater_source(&s,power,alpha));assert(physics_sim_source_interval_prepare_mode(&source,256,w,power,alpha,dt,false,method>=2));
for(int tick=1;tick<=end;tick++){
if(method==4){assert(cycles==1);assert(physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));}else for(int c=0;c<cycles;c++)assert(physics_sim_tail_time_step(&s,&rk,v,alpha,dt/cycles,method%2?3:2));assert(physics_sim_source_interval_add(&source,v,dt));
if((tick&(tick-1))==0||tick==80||tick==240||tick==1280||tick==end){double mass=0;for(int j=0;j<256;j++){assert(v[j]>=0&&v[j]<=.4+1e-10);mass+=w[j]*v[j];}
printf("{\"tick\":%d,\"method\":%d,\"mass\":%.17g,\"expected_mass\":%.17g,\"values\":[",tick,method,mass,tick*dt/64);for(int j=0;j<256;j++)printf("%s%.17g",j?",":"",v[j]);puts("]}");
}}
}
