/* Bounded diffusion reduction of the unchanged source geometry and fine spacing.
 * Exchange-off isolates the gas operator; this is not a coupled wall verdict. */
#include "physics_sim_tail_time.h"
static PhysicsSimTailTime rk;
#include "physics_sim_source_interval.h"
static PhysicsSimSourceInterval interval;
static PhysicsSimTailDiffusion tail;
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double widths[4096],v[4096];
int main(int argc,char **argv){
 if(argc!=5&&argc!=6)return 2;int axis=atoi(argv[1]),n=atoi(argv[2]),div=atoi(argv[3]),method=atoi(argv[4]);
 if((axis!=0&&axis!=1&&axis!=2)||(n!=256&&n!=512&&n!=2048&&n!=2112&&n!=4096)||(div!=1&&div!=2&&div!=4)||method<0||method>9)return 2;
 double duration=argc==6?strtod(argv[5],0):.1;if(duration!=.025&&duration!=.1)return 2;
 int centered=(method>=1&&method<=3)||method>=5;
 double length=axis==1?.25:axis==0?.125:.0625,h=length/n,dt=1./(51200*div),alpha=.026/1206;
 CfdMemoryBudget budget={.limit_bytes=2*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 double source[4096]={0},position=0;
 if(axis==1){int j=0;for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int k=0;k<count;k++){widths[j]=.25/128/count;source[j]=(x>=16&&x<32);j++;}}assert(j==n);}
 else for(int i=0;i<n;i++){widths[i]=h;source[i]=i>=n/4&&i<n/2;}
 PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,n,widths,0,alpha,centered?dt/2:dt,axis==1));
 assert(physics_sim_tail_diffusion_init(&tail,n,widths,axis==1));
 if(method>=8)assert(physics_sim_source_interval_prepare(&interval,n,widths,source,alpha,dt,axis==1));
 if(method==9)assert(physics_sim_tail_heater_source(&tail,source,alpha));
 for(int tick=0;tick<(int)lround(duration/dt);tick++){
  if(method<8)for(int i=0;i<n;i++)v[i]+=source[i]*(centered?dt/2:dt);
  if(method>=8){assert(physics_sim_tail_time_step(&tail,&rk,v,alpha,dt,2));assert(physics_sim_source_interval_add(&interval,v,dt));}
  else if(method>=5){int cycles=1<<(method-5);for(int c=0;c<cycles;c++)assert(physics_sim_tail_time_step(&tail,&rk,v,alpha,dt/cycles,2));for(int i=0;i<n;i++)v[i]+=source[i]*dt/2;}
  else if(!method)assert(physics_sim_local_matrix_step(&matrix,v));
  else if(method==4)assert(physics_sim_nonuniform_diffusion_step(&tail.stencil,&matrix,v,alpha,dt));
  else {assert(physics_sim_tail_diffusion_step(&tail,&matrix,v,alpha,dt,method?method-1:0));for(int i=0;i<n;i++)v[i]+=source[i]*dt/2;}
  for(int i=0;i<n;i++)assert(v[i]>=0&&v[i]<=.1+1e-10);
 }
 if(axis==1){double a[4]={v[1534],v[1535],v[1536],v[1537]},w[4]={widths[1534],widths[1535],widths[1536],widths[1537]},g=0;int fit=physics_sim_tail_fit(a,w,&g);fprintf(stderr,"interface means %a %a %a %a fit %d gradient %a target %a\n",a[0],a[1],a[2],a[3],fit,g,physics_sim_tail_gradient(&tail,v,1536,2));}
 double mass=0,band=0;for(int i=0;i<n;i++){mass+=widths[i]*v[i];if(axis==1?(position>=3./32&&position<1./8):axis==0?i>=3*n/4:i<n/8)band+=v[i]*widths[i]/(axis==1?1./32:axis==0?length/4:length/8);position+=widths[i];}
 printf("{\"method\":%d,\"axis\":%d,\"n\":%d,\"dt\":%.17g,\"time_s\":%.17g,\"wall_cell_k\":%.17g,\"band_k\":%.17g,\"mass\":%.17g,\"expected_mass\":%.17g}",method,axis,n,dt,duration,v[0],band,mass,duration*(axis==1?1./32:length/4));
 physics_sim_local_matrix_destroy(&matrix);cfd_memory_scope(prior);assert(!budget.live_bytes);puts("");return 0;
}
