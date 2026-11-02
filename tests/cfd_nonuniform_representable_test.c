/* Captured subnormal coarse-to-fine Euler stage, read as exact hex doubles. */
#include "../src/tools/physics_sim_nonuniform_advection.h"
#include <assert.h>
static PhysicsSimNonuniformAdvection s;
static double width[4096],value[4096],out[4096];
int main(void){
 int n;double u,dt;assert(scanf("%d %la %la",&n,&u,&dt)==3&&n<=4096);
 for(int j=0;j<n;j++)assert(scanf("%la %la",&width[j],&value[j])==2);
 assert(physics_sim_nonuniform_advection_init(&s,n,width,0));
 double flux=0;bool ok=physics_sim_nonuniform_euler(&s,value,out,u,dt,2,&flux);
 double minimum=out[0];for(int j=0;j<n;j++)minimum=fmin(minimum,out[j]);
 for(int j=0;j<n;j++)assert(isfinite(out[j])&&out[j]>=0&&out[j]<=.4+1e-10);
 assert(ok&&s.representable_faces>0);
 printf("{\"ok\":%s,\"cells\":%d,\"minimum_k\":%.17g,\"representable_faces\":%llu}\n",ok?"true":"false",n,minimum,s.representable_faces);
 return !ok;
}
