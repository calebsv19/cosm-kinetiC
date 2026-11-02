#include "../src/tools/physics_sim_local_matrix.h"
#include "../src/tools/physics_sim_uniform_advection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef PHYSICS_SIM_SCALAR_FCT_H
#define physics_sim_scalar_fct physics_sim_scalar_fct_original
#include "../src/tools/physics_sim_scalar_fct.h"
#undef physics_sim_scalar_fct
static double v[4096],before[4096],width[4096];
int main(void){
 const int n=4096;const double dx=.25/n,dt=1./25600;
 unsigned checks=0;double worst=0;
 double small[256],original[256],extended[256];
 for(int direction=-1;direction<=1;direction++){
  for(int j=0;j<256;j++)small[j]=j<128?.2:0;
  assert(physics_sim_scalar_fct_original(small,original,256,direction*.025,0,.25/256,dt,false));
  assert(physics_sim_scalar_fct(small,extended,256,direction*.025,0,.25/256,dt,false));
  assert(!memcmp(original,extended,sizeof(original)));
 }
 for(int pattern=0;pattern<4;pattern++)for(int sign=-1;sign<=1;sign+=2){
  double initial=0;
  for(int j=0;j<n;j++){
   v[j]=pattern==0?.25:pattern==1?(j>=512&&j<1024)?.25:0:
        pattern==2?.1+.05*cos(j*2*acos(-1)/n):(j==1024?1e-200:0);
   before[j]=v[j];initial+=v[j];
  }
  for(int step=0;step<16;step++){
   assert(uniform_advection_step(v,n,sign*.025,dx,dt,2));
   double mass=0;for(int j=0;j<n;j++){assert(v[j]>=0&&v[j]<=.25);mass+=v[j];}
   double error=fabs(mass-initial)/(initial?initial:1);worst=fmax(worst,error);assert(error<1e-12);checks++;
   if(pattern==0)for(int j=0;j<n;j++)assert(v[j]==before[j]);
   if(pattern==1&&step==0)assert(v[sign>0?1024:511]>0);
  }
 }
 CfdMemoryBudget budget={.limit_bytes=1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 for(int j=0;j<n;j++){width[j]=dx;v[j]=(j>=512&&j<1024)?1e-12:0;}
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,0,.026/1206,dt,true));
 assert(physics_sim_local_matrix_step(&matrix,v));
 assert(v[511]>0&&v[1024]>0); /* Both formerly cold neighbors receive heat. */
 assert(uniform_advection_step(v,n,.025,dx,dt,2));assert(v[511]>0&&v[1024]>0);
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"profile_steps\":%u,\"stage_bounds_checks\":%llu,\"worst_mass_relative\":%.17g,\"cold_advective_neighbors\":2,\"cold_diffusive_neighbors\":2,\"live_bytes\":%zu}\n",checks,uniform_adv_checks,worst,budget.live_bytes);
}
