#include "physics_sim_heated_face.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void){
 CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d s;int n[3]={4,4,64};double l[3]={.25,.25,.0625},z[2]={0,0},t[2]={300,300};
 assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,1./1600,z,t,z,false,0,0,.1));s.ground=true;
 double cg=1206*s.grid.volume;
 for(int j=0;j<4;j++)s.energy[j*4+1]=10*cg;
 double saved[1024];memcpy(saved,s.energy,sizeof saved);
 PhysicsSimHeatedFace f[4],before[4];for(int i=0;i<4;i++)f[i]=(PhysicsSimHeatedFace){2,-1,300,12.5,10,0,0};
 memcpy(before,f,sizeof f);f[3].h_w_m2_k=-1;PhysicsSimHeatedFace bad[4];memcpy(bad,f,sizeof f);
 assert(!physics_sim_heated_columns_exchange(&s,f,4,.01));assert(!memcmp(f,bad,sizeof f)&&!memcmp(saved,s.energy,sizeof saved));
 memcpy(f,before,sizeof f);assert(!physics_sim_heated_columns_exchange(&s,f,3,.01));assert(!memcmp(saved,s.energy,sizeof saved));
 for(int i=0;i<4;i++)f[i].h_w_m2_k=0;
 assert(physics_sim_heated_columns_exchange(&s,f,4,.01));assert(!memcmp(saved,s.energy,sizeof saved));
 memcpy(f,before,sizeof f);assert(physics_sim_heated_columns_exchange(&s,f,4,.001));
 for(int i=0;i<4;i++)if(i!=1)assert(f[i].gas_to_surface_j==0&&f[i].temperature_k==300);
 double debit=0;for(int q=0;q<s.grid.count;q++){if(q>=16||q%4!=1)assert(s.energy[q]==saved[q]);debit+=saved[q]-s.energy[q];}
 assert(fabs(debit-f[1].gas_to_surface_j)<1e-14);
 double cap=4*cg,g=10*.0625/4,exact=10/(1/cap+1/12.5)*(-expm1(-g*(1/cap+1/12.5)*.001));
 assert(fabs(debit/exact-1)<1e-7);
 cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!budget.live_bytes);
 puts("PASS local column mapping, unchanged exterior/cold columns, area scaling, zero law, late refusal rollback, analytic transfer, conservation");
}
