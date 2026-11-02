/* Bounded uniform-fine advection candidate and same-split donor control.
 * No z solve, wall exchange, solid, mesh restriction, feedback or state reset. */
#include "physics_sim_local_matrix.h"
#include "physics_sim_uniform_advection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double value[4096],width[4096];
int main(int argc,char **argv){
 if(argc!=3)return 2;
 int method=atoi(argv[2]);if(method<0||method>2)return 2;
 double u=strtod(argv[1],NULL);if(u!=0&&u!=.025)return 2;
 const int n=4096,steps=12800,pulse=10240;
 const double dx=.25/n,dt=1./25600,alpha=.026/1206;
 CfdMemoryBudget budget={.limit_bytes=1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 for(int j=0;j<n;j++)width[j]=dx;
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,method==0?u:0,alpha,dt,true));
 double worst=0,minimum=0,maximum=0;
 for(int step=0;step<=steps;step++){
  double mass=0;for(int j=0;j<n;j++){
   mass+=dx*value[j];minimum=fmin(minimum,value[j]);maximum=fmax(maximum,value[j]);
   assert(isfinite(value[j])&&value[j]>=0&&value[j]<=.4+1e-10);
  }
  double exact=(step<pulse?step:pulse)*dt/32;
  worst=fmax(worst,fabs(mass-exact));assert(fabs(mass-exact)<1e-11);
  if(step%640==0){
   double main=0,tail=0;
   for(int j=1024;j<1536;j++)main+=value[j]/512;
   for(int j=1536;j<2048;j++)tail+=value[j]/512;
   printf("{\"step\":%d,\"time_s\":%.17g,\"transport_k\":%.17g,\"tail_k\":%.17g,\"inventory_k_m\":%.17g,\"source_k_m\":%.17g}\n",step,step*dt,main,tail,mass,exact);
  }
  if(step==steps)break;
  /* Same add-source-then-BE order as physics_sim_localized_columns.c. */
  if(step<pulse)for(int j=512;j<1024;j++)value[j]+=dt;
  if(method)assert(uniform_advection_step(value,n,u,dx,dt,method));
  assert(physics_sim_local_matrix_step(&matrix,value));
 }
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"method\":%d,\"advection_stage_checks\":%llu,\"stage_roundoff_cells\":%llu,\"max_stage_excursion_k\":%.17g,\"terminal\":true,\"cells\":%d,\"steps\":%d,\"velocity_m_s\":%.17g,\"dt_s\":%.17g,\"dx_m\":%.17g,\"minimum_k\":%.17g,\"maximum_k\":%.17g,\"inventory_error_k_m\":%.17g,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",method,uniform_adv_checks,uniform_adv_roundoff_cells,uniform_adv_max_excursion,n,steps,u,dt,dx,minimum,maximum,worst,budget.peak_bytes,budget.live_bytes);
}
