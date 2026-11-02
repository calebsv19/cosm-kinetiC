#include "../src/tools/physics_sim_source_patch.h"
#include "../src/tools/physics_sim_source_interval.h"
#include <assert.h>
#include <stdio.h>
static double wx[2112],wz[256],px[2112],pz[256];
static PhysicsSimSourceInterval sx,sz;
int main(void){int n=0;double dt=1./51200,alpha=.026/1206;
for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int j=0;j<count;j++){wx[n]=.25/128/count;px[n++]=(x>=16&&x<32);}}
for(int k=0;k<256;k++){wz[k]=.0625/256;pz[k]=(k>=64&&k<128);}
CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
PhysicsSimSourcePatch a={0},b={0};assert(physics_sim_source_patch_prepare(&a,n,256,wx,wz,px,pz,alpha,dt,64));assert(physics_sim_source_patch_prepare(&b,n,256,wx,wz,px,pz,alpha,dt,96));
assert(physics_sim_source_interval_prepare(&sx,n,wx,px,alpha,dt,true));assert(physics_sim_source_interval_prepare(&sz,256,wz,pz,alpha,dt,false));
double mass=0,worst=0,marginal=0;
for(int j=0;j<n;j++){double sum=0;for(int k=0;k<256;k++){double v=a.delta[k*n+j],r=b.delta[k*n+j];assert(v>=0&&v<=dt);if(r>0)worst=fmax(worst,fabs(v/r-1));sum+=v*wz[k];mass+=v*wz[k]*wx[j];}if(sx.delta[j]>0)marginal=fmax(marginal,fabs(sum/(sx.delta[j]/64)-1));}
for(int k=0;k<256;k++){double sum=0;for(int j=0;j<n;j++)sum+=a.delta[k*n+j]*wx[j];if(sz.delta[k]>0)marginal=fmax(marginal,fabs(sum/(sz.delta[k]/32)-1));}
printf("mass relative %.17g; refinement %.17g; marginal %.17g\n",mass/(dt/2048)-1,worst,marginal);
assert(fabs(mass/(dt/2048)-1)<1e-12);assert(worst<.005&&marginal<.005);assert(a.delta[63*n+511]>0);
double *field=cfd_memory_calloc((size_t)n*256,sizeof(double));assert(field);
assert(!physics_sim_source_patch_prepare(&a,n,256,wx,wz,px,pz,alpha,dt,64));
assert(!physics_sim_source_patch_add(&a,field,dt*2));assert(field[0]==0);
field[64*n+512]=.4+1e-10;assert(!physics_sim_source_patch_add(&a,field,dt));assert(field[63*n+511]==0);
field[64*n+512]=NAN;assert(!physics_sim_source_patch_add(&a,field,dt));assert(field[63*n+511]==0);
field[64*n+512]=0;assert(physics_sim_source_patch_add(&a,field,dt));for(int j=0;j<n*256;j++)assert(field[j]==a.delta[j]);cfd_memory_free(field);
physics_sim_source_patch_destroy(&a);physics_sim_source_patch_destroy(&b);
assert(!physics_sim_source_patch_prepare(&a,n,256,wx,wz,px,pz,-alpha,dt,64)&&!a.delta);
assert(!physics_sim_source_patch_prepare(&a,n,256,wx,wz,px,pz,alpha,dt,32)&&!a.delta);
assert(!physics_sim_source_patch_prepare(&a,n,256,NULL,wz,px,pz,alpha,dt,64)&&!a.delta);
cfd_memory_scope(prior);assert(!budget.live_bytes);
}
