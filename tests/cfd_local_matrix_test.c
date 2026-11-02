#include "../src/tools/physics_sim_local_matrix.h"
#include "../src/tools/physics_sim_scalar_subcell.h"
#include <assert.h>
#include <stdio.h>
static struct physics_sim_subcell uniform;
int main(void){
 int checks=0;double worst=0;CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 for(int size=8;size<=128;size*=2)for(int pattern=0;pattern<3;pattern++)for(int direction=-1;direction<=1;direction++){
  double width[128],v[128],sumw=0;
  for(int i=0;i<size;i++){width[i]=pattern==0?1:(i%3==0?.03125:1);sumw+=width[i];}
  double initial=0;for(int i=0;i<size;i++){width[i]*=.25/sumw;v[i]=pattern==1?(i==size/2):1+.2*sin(i);initial+=width[i]*v[i];}
  PhysicsSimLocalMatrix m={0};assert(physics_sim_local_matrix_init(&m,size,width,direction*.05,.026/1206,.00001,true));
  if(pattern==0){assert(physics_sim_subcell_init(&uniform,size,direction*.05,.026/1206,.25/size,.00001));}
  double other[128];for(int i=0;i<size;i++)other[i]=v[i];
  for(int step=0;step<32;step++){
   assert(physics_sim_local_matrix_step(&m,v));double mass=0;
   if(pattern==0)assert(physics_sim_subcell_step(&uniform,other));
   for(int i=0;i<size;i++){assert(v[i]>=0&&v[i]<=1.3);mass+=width[i]*v[i];if(pattern==0)assert(fabs(v[i]-other[i])<1e-12);}
   double error=fabs(mass/initial-1);worst=fmax(worst,error);assert(error<1e-11);checks++;
  }
  physics_sim_local_matrix_destroy(&m);
 }
 /* Reflecting normal columns must conserve their volume-weighted heat too. */
 for(int size=8;size<=128;size*=2){
  double width[128],v[128],mass=0;
  for(int i=0;i<size;i++){width[i]=.0625/size;v[i]=i==size/2;mass+=width[i]*v[i];}
  PhysicsSimLocalMatrix m={0};assert(physics_sim_local_matrix_init(&m,size,width,0,.026/1206,1./3200,false));
  for(int step=0;step<32;step++){
   assert(physics_sim_local_matrix_step(&m,v));double total=0;
   for(int i=0;i<size;i++){assert(v[i]>=0&&v[i]<=1);total+=width[i]*v[i];}
   double error=fabs(total/mass-1);worst=fmax(worst,error);assert(error<1e-11);checks++;
  }
  physics_sim_local_matrix_destroy(&m);
 }
 cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"localized_stage_checks\":%d,\"max_weighted_mass_relative\":%.17g,\"live_bytes\":%zu}\n",checks,worst,budget.live_bytes);
}
