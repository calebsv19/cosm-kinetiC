/* Fixed .5s interface qualification. No wall/solid/normal solve or feedback. */
#include "physics_sim_local_matrix.h"
#include "physics_sim_nonuniform_advection.h"
#include <assert.h>
#include <stdlib.h>
static double value[4096],width[4096];static int offset[129];
static PhysicsSimNonuniformAdvection advection;
int main(int argc,char **argv){
 if(argc!=4)return 2;
 int uniform=atoi(argv[1]),method=atoi(argv[3]);double u=strtod(argv[2],NULL);
 if((uniform!=0&&uniform!=1)||(u!=0&&u!=.025)||method<0||method>2)return 2;
 int n=0;const double dt=1./25600,alpha=.026/1206,parent=.25/128;
 for(int x=0;x<128;x++){
  offset[x]=n;int count=uniform||x<48||x>=112?32:1;
  for(int j=0;j<count;j++)width[n++]=parent/count;
 }offset[128]=n;
 assert(physics_sim_nonuniform_advection_init(&advection,n,width,offset[48]));
 CfdMemoryBudget budget={.limit_bytes=1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,method==0?u:0,alpha,dt,true));
 double worst=0,minimum=0,maximum=0,diffused=0;
 for(int step=0;step<=12800;step++){
  double mass=0;for(int j=0;j<n;j++){
   mass+=width[j]*value[j];minimum=fmin(minimum,value[j]);maximum=fmax(maximum,value[j]);
   assert(value[j]>=0&&value[j]<=.4+1e-10&&isfinite(value[j]));
  }
  double source=fmin(step,10240)*dt/32;worst=fmax(worst,fabs(mass-source));assert(fabs(mass-source)<1e-11);
  if(step%640==0){
   double main=0,tail=0;
   for(int j=offset[32];j<offset[48];j++)main+=value[j]*width[j]*32;
   for(int j=offset[48];j<offset[64];j++)tail+=value[j]*width[j]*32;
   printf("{\"step\":%d,\"time_s\":%.17g,\"transport_k\":%.17g,\"tail_k\":%.17g,\"inventory_k_m\":%.17g,\"source_k_m\":%.17g,\"interface_advected_k_m\":%.17g,\"interface_diffused_k_m\":%.17g}\n",step,step*dt,main,tail,mass,source,advection.advected_interface,diffused);
  }
  if(step==12800)break;
  if(step<10240)for(int j=offset[16];j<offset[32];j++)value[j]+=dt;
  if(method)assert(physics_sim_nonuniform_advection_step(&advection,value,u,dt,method));
  assert(physics_sim_local_matrix_step(&matrix,value));
  int i=offset[48];diffused-=dt*alpha*(value[i]-value[i-1])/(.5*(width[i]+width[i-1]));
  if(method==0)advection.advected_interface+=dt*u*value[i-1];
 }
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"terminal\":true,\"cells\":%d,\"uniform\":%d,\"method\":%d,\"steps\":12800,\"stage_checks\":%llu,\"roundoff_cells\":%llu,\"max_excursion_k\":%.17g,\"minimum_k\":%.17g,\"maximum_k\":%.17g,\"inventory_error_k_m\":%.17g,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",n,uniform,method,advection.checks,advection.roundoff_cells,advection.max_excursion,minimum,maximum,worst,budget.peak_bytes,budget.live_bytes);
}
