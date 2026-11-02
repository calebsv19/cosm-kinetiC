#include "../src/tools/physics_sim_source_age.h"
#include <assert.h>
#include <stdio.h>
static PhysicsSimTailDiffusion s;static PhysicsSimTailTime rk;static PhysicsSimSourceAge age;
static PhysicsSimSourceInterval source;static double w[256],power[256],v[256],base[256],saved[256];
int main(void){double dt=1./51200,alpha=.026/1206,h=.0625/256;
for(int i=0;i<256;i++){w[i]=h;power[i]=i>=64&&i<128;}
assert(physics_sim_tail_diffusion_init(&s,256,w,false));assert(physics_sim_tail_heater_source(&s,power,alpha));assert(physics_sim_source_interval_prepare_complete(&source,256,w,power,alpha,dt,false));assert(physics_sim_source_interval_add(&source,v,dt));memcpy(saved,v,sizeof v);
assert(physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));
/* Independent positive source-age/time Simpson8192/16384 reference; K*m. */
const double expected[7]={-1.7759839546872922e-292,-1.243689185703011e-207,-4.3708827496485e-138,-8.440192763375802e-84,-1.0896878248207211e-44,-1.6483607129161804e-20,-1.248809455345751e-10};
long double total=0,mass=0;
for(int i=0;i<256;i++){
 assert(v[i]>=0&&v[i]<=dt);mass+=(long double)v[i]*h;
 total+=(long double)h*(v[i]-saved[i]);int face=i+1;
 if(face>=58&&face<=64){double ref=expected[face-58];assert(fabs(age.flux[face]*s.stencil.limiter.unit_width/ref-1)<.005);assert(fabsl(-total/ref-1)<.005);}
}
assert(v[57]>0&&saved[57]==0);assert(fabsl(mass/(dt/64)-1)<1e-12);memcpy(base,v,sizeof v);
/* Actual perturbation must change flux and spread; no state substitution. */
memcpy(v,saved,sizeof v);v[200]=1e-5;assert(physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));assert(v[199]>base[199]&&v[201]>base[201]&&v[200]<1e-5&&v[200]>base[200]);
long double extra=0;for(int i=0;i<256;i++){assert(v[i]>=0&&v[i]<=dt);extra+=((long double)v[i]-base[i])*h;}assert(fabsl(extra/(1e-5*h)-1)<1e-12);
/* Unsupported source/geometry and nonfinite field refuse before mutation. */
memcpy(saved,v,sizeof v);power[200]=1;assert(!physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));assert(!memcmp(saved,v,sizeof v));power[200]=0;
s.widths[100]*=2;assert(!physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));assert(!memcmp(saved,v,sizeof v));s.widths[100]/=2;
v[0]=NAN;memcpy(saved,v,sizeof v);assert(!physics_sim_source_age_step(&s,&rk,&age,v,alpha,dt));assert(!memcmp(saved,v,sizeof v));
puts("source-age first-interval requested/admitted flux, cold arrival, perturbation transport, mass, bounds and atomic invalid-input refusal pass");return 0;}
