#include "../src/tools/physics_sim_nonuniform_advection.h"
#include "../src/tools/physics_sim_local_matrix.h"
#include "../src/tools/physics_sim_scalar_fct.h"
#include <assert.h>
#include <string.h>
static PhysicsSimNonuniformAdvection s;
static double width[4096],v[4096],old[4096],original[256],adapted[256];
static double gaussian(double lo,double hi){
 double sigma=.006,mu=.0625,a=(lo-mu)/(sqrt(2)*sigma),b=(hi-mu)/(sqrt(2)*sigma);
 double diff=a>=0?erfc(a)-erfc(b):b<=0?erfc(-b)-erfc(-a):erf(b)-erf(a);
 return .2*sigma*sqrt(acos(-1)/2)*diff/(hi-lo);
}
int main(void){
 unsigned parity=0,checks=0;double worst=0,parity_error=0;const double dt=1./25600;
 for(int n=8;n<=256;n*=2){
  for(int j=0;j<n;j++)width[j]=.25/n;
  assert(physics_sim_nonuniform_advection_init(&s,n,width,n/2));
  for(int pattern=0;pattern<3;pattern++)for(int sign=-1;sign<=1;sign+=2){
   for(int j=0;j<n;j++)v[j]=pattern==0?.2:pattern==1?(j<n/2?.2:0):.1+.05*cos(2*acos(-1)*j/n);
   double flux=0;
   assert(physics_sim_scalar_fct(v,original,n,sign*.025,0,.25/n,dt,false));
   assert(physics_sim_nonuniform_euler(&s,v,adapted,sign*.025,dt,2,&flux));
   for(int j=0;j<n;j++){double error=fabs(adapted[j]-original[j]);parity_error=fmax(parity_error,error);assert(error<64*DBL_EPSILON*.2);}
   parity++;
  }
 }
 int n=0,face=0;double parent=.25/128;
 for(int x=0;x<128;x++){
  if(x==48)face=n;int count=x<48||x>=112?32:1;
  for(int j=0;j<count;j++)width[n++]=parent/count;
 }
 assert(n==2112&&physics_sim_nonuniform_advection_init(&s,n,width,face));
 /* Exact finite-volume polynomials at the unequal face. */
 int index[4]={face-2,face-1,face,face+1};
 double h=.5*(width[face-1]+width[face]);
 double ends[5]={-(width[face-2]+width[face-1])/h,-width[face-1]/h,0,width[face]/h,(width[face]+width[face+1])/h};
 for(int p=0;p<4;p++){
  for(int j=0;j<4;j++)v[index[j]]=(pow(ends[j+1],p+1)-pow(ends[j],p+1))/((p+1)*(ends[j+1]-ends[j]));
  assert(fabs(physics_sim_nonuniform_face(&s,v,face)-(p==0))<1e-12);
 }
 for(int pattern=0;pattern<4;pattern++)for(int sign=-1;sign<=1;sign+=2){
  double initial=0;
  for(int j=0;j<n;j++){
   v[j]=pattern==0?.25:pattern==1?(sign>0?(j>=face-8&&j<face):(j>=face&&j<face+8))?.2:0:
        pattern==2?.1+.05*cos(j*2*acos(-1)/n):(j==face?1e-200:0);
   initial+=width[j]*v[j];old[j]=v[j];
  }
  for(int step=0;step<32;step++){
   assert(physics_sim_nonuniform_advection_step(&s,v,sign*.025,dt,2));
   double mass=0;for(int j=0;j<n;j++){assert(v[j]>=0&&v[j]<=.4+1e-10);mass+=width[j]*v[j];if(pattern==0)assert(v[j]==old[j]);}
   double error=fabs(mass/initial-1);worst=fmax(worst,error);assert(error<1e-11);checks++;
   if(pattern==1&&step==0)assert(v[sign>0?face:face-1]>0);
  }
 }
 double position=0;
 for(int j=0;j<n;j++){v[j]=gaussian(position,position+width[j]);position+=width[j];}
 double exact_face=.2*exp(-.5*pow((3./32-1./16)/.006,2));
 double high=physics_sim_nonuniform_face(&s,v,face)/exact_face;
 double donor=v[face-1]/exact_face;
 double limited=0;assert(physics_sim_nonuniform_euler(&s,v,old,.025,dt,2,&limited));
 limited*=s.unit_width/(dt*.025*exact_face);
 double diffusion=-(v[face]-v[face-1])/(.5*(width[face]+width[face-1]))/((3./32-1./16)/(.006*.006)*exact_face);
 /* Preserve nonnegativity at the originally failing representability scale. */
 for(int direction=-1;direction<=1;direction+=2){
  for(int j=0;j<n;j++)v[j]=0;
  v[direction>0?face-1:face]=1e-320;
  for(int step=0;step<16;step++)assert(physics_sim_nonuniform_advection_step(&s,v,direction*.025,dt,2));
  assert(v[direction>0?face:face-1]>0);
 }
 /* Positive physical diffusion must still deliver heat into both cold cells. */
 for(int j=0;j<n;j++)v[j]=(j>=face-4&&j<face)?1e-12:0;
 CfdMemoryBudget budget={.limit_bytes=1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,width,0,.026/1206,dt,true));
 assert(physics_sim_local_matrix_step(&matrix,v));assert(v[face]>0&&v[face-5]>0);
 assert(physics_sim_nonuniform_advection_step(&s,v,.025,dt,2));assert(v[face]>0&&v[face-5]>0);
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(budget.live_bytes==0);
 printf("{\"uniform_parity_cases\":%u,\"uniform_max_absolute_k\":%.17g,\"mixed_profile_steps\":%u,\"weighted_mass_relative\":%.17g,\"gaussian_donor_ratio\":%.17g,\"gaussian_high_face_ratio\":%.17g,\"gaussian_diffusive_face_ratio\":%.17g,\"gaussian_limited_advective_flux_ratio\":%.17g,\"subnormal_steps\":32,\"cold_advective_neighbors\":2,\"cold_diffusive_neighbors\":2,\"memory_live_bytes\":%zu}\n",parity,parity_error,checks,worst,donor,high,diffusion,limited,budget.live_bytes);
}
