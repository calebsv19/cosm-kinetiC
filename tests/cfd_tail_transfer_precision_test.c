#include "../src/tools/physics_sim_tail_time.h"
#include "../src/tools/physics_sim_source_interval.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static PhysicsSimTailDiffusion legacy,precise;
static PhysicsSimSourceInterval source,complete,saved;
static double w[256],power[256],old[256],a[256],b[256],flux[256];
int main(void){
 double dt=1./51200,alpha=.026/1206;
 for(int j=0;j<256;j++){w[j]=.0625/256;power[j]=j>=64&&j<128;}
 assert(physics_sim_source_interval_prepare(&source,256,w,power,alpha,dt,false));
 assert(physics_sim_source_interval_prepare_complete(&complete,256,w,power,alpha,dt,false));
 /* Independent positive heat-kernel Simpson4096/8192, not this primitive:
  * omitted fifth cold cell is representable and must not be discarded. */
 double reference=0x1.b5cd3c1d9d02bp-857;
 assert(source.delta[59]==0&&complete.delta[59]>0);
 assert(fabs(complete.delta[59]/reference-1)<.005);
 assert(complete.delta[58]==0); /* Natural binary64 underflow, no heat floor. */
 double mass=0;for(int j=0;j<256;j++){assert(complete.delta[j]>=0&&complete.delta[j]<=dt);mass+=complete.delta[j]*w[j];}
 assert(fabs(mass/(dt/64)-1)<1e-12);
 for(int j=60;j<64;j++)assert(complete.delta[j]==source.delta[j]);
 assert(physics_sim_source_interval_add(&source,old,dt));
 memcpy(a,old,sizeof a);memcpy(b,old,sizeof b);
 assert(physics_sim_tail_diffusion_init(&legacy,256,w,false));
 assert(physics_sim_tail_diffusion_init(&precise,256,w,false));
 assert(physics_sim_tail_heater_source(&legacy,power,alpha));
 assert(physics_sim_tail_heater_source(&precise,power,alpha));
 for(int i=1;i<256;i++)flux[i]=-dt/6*alpha/w[0]*physics_sim_tail_gradient(&precise,old,i,2);
 assert(physics_sim_tail_euler(&legacy,a,alpha,dt/6,2));
 assert(physics_sim_tail_euler(&precise,b,alpha,dt/6,3));
 assert(old[60]>0&&old[61]>0&&a[60]==0&&a[61]==0);
 for(int j=60;j<=61;j++){
  double expected=old[j]+flux[j]-flux[j+1];
  assert(b[j]>old[j]&&fabs(b[j]/expected-1)<1e-13);
 }
 for(int j=0;j<256;j++)assert(b[j]>=0&&b[j]<=dt);
 /* Complete-support refusal is atomic when a representable tail would hit
  * unequal geometry before underflow. Legacy support remains unchanged. */
 saved=complete;w[59]*=2;
 assert(!physics_sim_source_interval_prepare_complete(&complete,256,w,power,alpha,dt,false));
 assert(!memcmp(&saved,&complete,sizeof saved));w[59]/=2;
 power[64]=2;assert(!physics_sim_source_interval_prepare_complete(&complete,256,w,power,alpha,dt,false));assert(!memcmp(&saved,&complete,sizeof saved));
 printf("representable fifth source cell %.17g; legacy erases two positive cold cells; net transfer matches independent face ledger; bounds, source mass and atomic refusal pass\n",reference);
 return 0;
}
