#include "../src/tools/physics_sim_tail_diffusion.h"
#include <assert.h>
#include <stdio.h>
static PhysicsSimTailDiffusion s;
static double w[4096],v[4096],saved[4096];
int main(void){
 CfdMemoryBudget budget={.limit_bytes=2*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);unsigned checks=0;double worst=0;
 /* Independently integrate polynomial cell means: exact regular-face gradient
  * through degree five, without fitting to any reference simulation. */
 for(int j=0;j<8;j++)w[j]=.0625/256;
 assert(physics_sim_tail_diffusion_init(&s,8,w,true));
 for(int degree=0;degree<=5;degree++){
  for(int j=0;j<8;j++)v[j]=.2+.01*(j-2.5)+1e-5*(pow(j-2,degree+1)-pow(j-3,degree+1))/(degree+1);
  double expected=(.01+(degree==1?1e-5:0))/w[0];
  assert(fabs(physics_sim_tail_gradient(&s,v,3,2)/expected-1)<1e-11);
 }
 /* Independent analytic exponential cell averages: steep regular-face
  * gradients must retain shape across amplitudes and orientations. No
  * temperature samples or reference simulation are substituted for means. */
 for(int orientation=-1;orientation<=1;orientation+=2)for(int slope=1;slope<=6;slope++){
  double k=orientation*slope,amplitude=1e-200;
  for(int j=0;j<8;j++)v[j]=amplitude*exp(k*(j-3))*expm1(k)/k;
  double expected=k*amplitude/w[0];
  assert(fabs(physics_sim_tail_gradient(&s,v,3,2)/expected-1)<1e-8);
 }
 for(int mesh=0;mesh<2;mesh++){
  int n=256;if(!mesh)for(int j=0;j<n;j++)w[j]=.0625/n;
  else{n=0;for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int j=0;j<count;j++)w[n++]=.25/128/count;}}
  assert(physics_sim_tail_diffusion_init(&s,n,w,mesh));assert(s.stencil.limiter.width==s.widths);
  double original=w[0];w[0]=123;assert(s.widths[0]==original);w[0]=original;PhysicsSimLocalMatrix matrix={0};double dt=1./51200,alpha=.026/1206;
  assert(physics_sim_local_matrix_init(&matrix,n,w,0,alpha,dt/2,mesh));
  for(int pattern=0;pattern<8;pattern++){
   double mass0=0;
   for(int j=0;j<n;j++){v[j]=pattern==0?.25:pattern==1?(j>=n/4&&j<n/2?.2:0):pattern==2?(j==n/2?1e-200:0):pattern==3?.1+.05*cos(j*2*acos(-1)/n):pattern==4?0:pattern==5?(j==n/2?1e-320:0):pattern==6?(j>=n/4&&j<n/2?0x1.0b2p-1063:0):0x1.0b2p-1063;mass0+=w[j]*v[j];}
   for(int step=0;step<32;step++){
    assert(physics_sim_tail_diffusion_step(&s,&matrix,v,alpha,dt,2));double mass=0;
    for(int j=0;j<n;j++){assert(v[j]>=0&&v[j]<=.4+1e-10);if(pattern>=6)assert(v[j]<=0x1.0b2p-1063);mass+=w[j]*v[j];if(pattern==0)assert(fabs(v[j]-.25)<=64*DBL_EPSILON*.25);if(pattern==4)assert(v[j]==0);}
    if(pattern<5&&mass0){double e=fabs(mass/mass0-1);worst=fmax(worst,e);assert(e<1e-11);}
    if(pattern>=5)assert(fabs(mass-mass0)<=128*n*nextafter(0,1));
    if((pattern==1||pattern==2||pattern==5)&&step==0){int at=pattern==1?n/2:n/2+1;assert(v[at]>0);}
    checks++;
   }
  }
  memcpy(saved,v,(size_t)n*sizeof(double));assert(!physics_sim_tail_diffusion_step(&s,&matrix,v,alpha,-dt,2));assert(!memcmp(saved,v,(size_t)n*sizeof(double)));
  assert(!physics_sim_tail_diffusion_step(&s,&matrix,v,alpha,1,2));assert(!memcmp(saved,v,(size_t)n*sizeof(double)));
  physics_sim_local_matrix_destroy(&matrix);
 }
 cfd_memory_scope(prior);assert(!budget.live_bytes);printf("%u bounded diffusion stages + six polynomial moments + 12 steep exponential means; worst mass %.17g; cold and subnormal transfer preserved; zero live bytes\n",checks,worst);return 0;
}
