#include "../src/tools/physics_sim_nonuniform_diffusion.h"
#include <assert.h>
static PhysicsSimNonuniformDiffusion d;
static double width[4096],value[4096];
static double gaussian(double lo,double hi,double time){
 double sigma=sqrt(.006*.006+2*(.026/1206)*time),mu=.0625;
 double a=(lo-mu)/(sqrt(2)*sigma),b=(hi-mu)/(sqrt(2)*sigma);
 double diff=a>=0?erfc(a)-erfc(b):b<=0?erfc(-b)-erfc(-a):erf(b)-erf(a);
 return .2*.006*sqrt(acos(-1)/2)*diff/(hi-lo);
}
int main(void){
 int n=0,face=0;const double parent=.25/128,alpha=.026/1206,dt=1./25600;
 for(int x=0;x<128;x++){
  if(x==48)face=n;int count=x<48||x>=112?32:1;
  for(int j=0;j<count;j++)width[n++]=parent/count;
 }
 assert(physics_sim_nonuniform_diffusion_init(&d,n,width,face)&&d.enabled_faces==6);
 CfdMemoryBudget budget={.limit_bytes=1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,0,alpha,dt,true));
 double position=0;
 for(int j=0;j<n;j++){value[j]=gaussian(position,position+width[j],0);position+=width[j];}
 double distance=3./32-1./16;
 double exact=.2*exp(-.5*pow(distance/.006,2))*(-distance/(.006*.006));
 double central=(value[face]-value[face-1])/(.5*(width[face]+width[face-1]));
 double target=physics_sim_nonuniform_gradient(&d,value,face);
 double flux_before=d.accepted_interface;
 assert(physics_sim_nonuniform_diffusion_step(&d,&matrix,value,alpha,dt));
 double sigma=sqrt(.006*.006+2*alpha*dt);
 double exact_end=.2*.006/sigma*exp(-.5*pow(distance/sigma,2))*(-distance/(sigma*sigma));
 double accepted=(d.accepted_interface-flux_before)/(-dt*alpha*exact_end);
 assert(fabs(target/exact-1)<.1&&fabs(accepted-1)<.1);
 unsigned checks=0;double worst=0;
 for(int pattern=0;pattern<4;pattern++){
  double mass0=0;
  for(int j=0;j<n;j++){value[j]=pattern==0?.25:pattern==1?(j>=face-8&&j<face?.2:0):pattern==2?.1+.05*cos(j*2*acos(-1)/n):(j==face?1e-200:0);mass0+=width[j]*value[j];}
  for(int k=0;k<32;k++){
   assert(physics_sim_nonuniform_diffusion_step(&d,&matrix,value,alpha,dt));
   double mass=0;for(int j=0;j<n;j++){assert(value[j]>=0&&value[j]<=.4+1e-10);mass+=width[j]*value[j];if(pattern==0)assert(fabs(value[j]-.25)<=64*DBL_EPSILON*.25);}
   double error=fabs(mass/mass0-1);worst=fmax(worst,error);assert(error<1e-11);checks++;
   if(pattern==1&&k==0)assert(value[face]>0&&value[face-9]>0);
  }
 }
 for(int side=-1;side<=1;side+=2){
  for(int j=0;j<n;j++)value[j]=0;
  value[side>0?face-1:face]=1e-320;
  for(int k=0;k<16;k++)assert(physics_sim_nonuniform_diffusion_step(&d,&matrix,value,alpha,dt));
  assert(value[side>0?face:face-1]>0);
 }
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"central_face_ratio\":%.17g,\"target_face_ratio\":%.17g,\"accepted_flux_ratio_new_time\":%.17g,\"corrected_faces\":%d,\"profile_steps\":%u,\"subnormal_steps\":32,\"weighted_mass_relative\":%.17g,\"memory_live_bytes\":%zu}\n",central/exact,target/exact,accepted,d.enabled_faces,checks,worst,budget.live_bytes);
}
