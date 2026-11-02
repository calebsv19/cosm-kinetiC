/* Manufactured smooth-tail face check. The Gaussian is an independent
 * advection/diffusion solution, not a replacement for the coupled history. */
#include "../src/tools/physics_sim_local_matrix.h"
#include <assert.h>
#include <stdio.h>
static double average(double lo,double hi,double mu,double sigma,double amplitude){
 double a=(lo-mu)/(sqrt(2)*sigma),b=(hi-mu)/(sqrt(2)*sigma);
 double diff=a>=0?erfc(a)-erfc(b):b<=0?erfc(-b)-erfc(-a):erf(b)-erf(a);
 return amplitude*sigma*sqrt(acos(-1)/2)*diff/(hi-lo);
}
static double periodic(double lo,double hi,double mu,double sigma,double amplitude){
 double sum=0;for(int image=-1;image<=1;image++)sum+=average(lo,hi,mu+.25*image,sigma,amplitude);return sum;
}
int main(void){
 CfdMemoryBudget budget={.limit_bytes=4*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 const double dx=.25/128,dt=1./25600,alpha=.026/1206,mu=.0625,sigma=.006;
 for(int end=48;end<=64;end+=16)for(int mode=0;mode<3;mode++){
  double width[4096],center[4096],value[4096],before[4096];int n=0,face=0;
  for(int x=0;x<128;x++){
   if(x==48)face=n;
   int count=x<end||x>=112?32:1;
   for(int j=0;j<count;j++){width[n]=dx/count;center[n]=x*dx+(j+.5)*width[n];before[n]=value[n]=periodic(center[n]-width[n]/2,center[n]+width[n]/2,mu,sigma,1);n++;}
  }
  double u=mode==0?0:.025,diff=mode==1?0:alpha;
  PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,u,diff,dt,true));
  assert(physics_sim_local_matrix_step(&matrix,value));
  double x=48*dx,tface=exp(-.5*pow((x-mu)/sigma,2));
  double expected_adv=u*tface,expected_diff=diff*(x-mu)/(sigma*sigma)*tface;
  double disc_adv=u*before[face-1],disc_diff=-diff*(before[face]-before[face-1])/(.5*(width[face]+width[face-1]));
  double next_sigma=sqrt(sigma*sigma+2*diff*dt);
  double exact=periodic(center[face]-width[face]/2,center[face]+width[face]/2,mu+u*dt,next_sigma,sigma/next_sigma);
  printf("{\"fine_end\":%d,\"mode\":%d,\"dt_s\":%.17g,\"advective_flux_ratio\":%.17g,\"diffusive_flux_ratio\":%.17g,\"first_right_exact_increment_k\":%.17g,\"first_right_native_increment_k\":%.17g}\n",end,mode,dt,expected_adv?disc_adv/expected_adv:0,expected_diff?disc_diff/expected_diff:0,exact-before[face],value[face]-before[face]);
  double initial_mass=0,final_mass=0;
  for(int j=0;j<n;j++){initial_mass+=width[j]*before[j];final_mass+=width[j]*value[j];}
  assert(fabs(final_mass/initial_mass-1)<1e-11);
  if(end==64)assert(fabs((value[face]-before[face])/(exact-before[face])-1)<.03);
  physics_sim_local_matrix_destroy(&matrix);
 }
 cfd_memory_scope(prior);assert(budget.live_bytes==0);
}
