#include "../src/tools/physics_sim_source_interval.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
static PhysicsSimSourceInterval s,saved;static double w[4096],power[4096],v[4096],before[4096];
int main(void){double dt=1./51200,alpha=.026/1206,worst=0,kernel=0;unsigned checks=0;
 const double reference[2][4]={{0x1.4b7dacca44590p-21,0x1.5717dcccd2b01p-83,0x1.5124be5c1417cp-240,0x1.c0be0ca9ed97ap-498},{0x1.49de5027735a4p-19,0x1.9f4981a1c8a23p-27,0x1.3212a546f7df5p-39,0x1.38694bbf67a86p-57}};
 for(int mesh=0;mesh<2;mesh++){
  int n=mesh?2112:256;if(mesh){int j=0;for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int k=0;k<count;k++)w[j++]=.25/128/count;}assert(j==n);}else for(int j=0;j<n;j++)w[j]=.0625/n;
  int start=mesh?512:64,end=mesh?1024:128;
  for(int pattern=0;pattern<5;pattern++){
   double amplitude=pattern==2?1e-200:pattern==3?1e-300:1;
   for(int j=0;j<n;j++){power[j]=pattern==0?0:pattern==4?amplitude:(j>=start&&j<end?amplitude:0);v[j]=0;}
   assert(physics_sim_source_interval_prepare(&s,n,w,power,alpha,dt,mesh));
   double mass=0,expected=0;for(int j=0;j<n;j++){assert(s.delta[j]>=0&&s.delta[j]<=amplitude*dt);mass+=w[j]*(s.delta[j]/amplitude);expected+=dt*w[j]*(power[j]/amplitude);}
   if(expected){double e=fabs(mass/expected-1);assert(e<1e-12);worst=fmax(worst,e);}
   assert(s.omitted_fraction<1e-12);assert(physics_sim_source_interval_add(&s,v,dt));
   if(pattern>0&&pattern<4){assert(v[start-1]>0&&v[end]>0);/* Relative kernel checks use unit power; some tiny-power outer
    * coefficients are below binary64, so those profiles test positivity
    * and conservation without claiming relative point qualification. */
    if(pattern==1)for(int k=0;k<4;k++){double value=v[end+k]/amplitude,e=fabs(value/reference[mesh][k]-1);assert(e<.005);kernel=fmax(kernel,e);}}
   if(pattern==0)for(int j=0;j<n;j++)assert(v[j]==0);
   if(pattern==4)for(int j=0;j<n;j++)assert(v[j]==dt);
   memcpy(before,v,(size_t)n*sizeof(double));assert(!physics_sim_source_interval_add(&s,v,2*dt));assert(!memcmp(before,v,(size_t)n*sizeof(double)));
   for(int j=0;j<n;j++)v[j]=.4+1e-10;memcpy(before,v,(size_t)n*sizeof(double));/* Cap refusal is specified for representable unit-power additions.
    * Tiny additions to a warm field may round to the same value. */
   if(pattern==1||pattern==4){assert(!physics_sim_source_interval_add(&s,v,dt));assert(!memcmp(before,v,(size_t)n*sizeof(double)));}
   checks++;
  }
  saved=s;power[0]=-1;assert(!physics_sim_source_interval_prepare(&s,n,w,power,alpha,dt,mesh));assert(!memcmp(&s,&saved,sizeof s));power[0]=0;
 }
 for(int j=0;j<32;j++){w[j]=.0625/256;power[j]=j==0?1:0;}saved=s;
 assert(!physics_sim_source_interval_prepare(&s,32,w,power,alpha,dt,false));assert(!memcmp(&s,&saved,sizeof s));
 for(int j=0;j<32;j++)power[j]=j>=8&&j<16;w[16]*=2;
 assert(!physics_sim_source_interval_prepare(&s,32,w,power,alpha,dt,true));assert(!memcmp(&s,&saved,sizeof s));w[16]/=2;
 assert(!physics_sim_source_interval_prepare(&s,32,w,power,alpha,1,true));assert(!memcmp(&s,&saved,sizeof s));
 assert(physics_sim_source_interval_prepare(&s,32,w,power,0,dt,true));for(int j=0;j<32;j++)assert(s.delta[j]==dt*power[j]);
 printf("%u source profiles; max weighted mass %.17g; max independent kernel error %.17g; cold conduction, bounds, power/clock and atomic refusals pass\n",checks,worst,kernel);return 0;}
