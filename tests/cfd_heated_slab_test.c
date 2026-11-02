#include "../src/tools/physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void){
 CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d s;int n[3]={4,4,64};double l[3]={.25,.25,.5},z[2]={0,0},t[2]={300,300};
 assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,1./1600,z,t,z,false,0,0,.1));
 for(int i=0;i<s.grid.count;i++)s.energy[i]=1206*s.grid.volume*10;
 double saved[1024];memcpy(saved,s.energy,sizeof saved);PhysicsSimHeatedFace f={2,-1,300,50,10,0,0},before=f;
 assert(!physics_sim_heated_slab_exchange(&s,&f,.1,.344,.375));
 assert(!memcmp(saved,s.energy,sizeof saved)&&!memcmp(&f,&before,sizeof f));
 f.h_w_m2_k=-1;before=f;assert(!physics_sim_heated_slab_exchange(&s,&f,.1,.34375,.375));
 assert(!memcmp(saved,s.energy,sizeof saved)&&!memcmp(&f,&before,sizeof f));
 f.h_w_m2_k=0;assert(physics_sim_heated_slab_exchange(&s,&f,.1,.34375,.375));assert(!memcmp(saved,s.energy,sizeof saved)&&f.gas_to_surface_j==0);
 f.h_w_m2_k=10;double initial=cfd_passive3d_total(s.energy,s.grid.count);
 assert(physics_sim_heated_slab_exchange(&s,&f,.1,.34375,.375));
 double cg=1206*.0625*.03125,cs=50,g=.625,q=(10/(1/cg+1/cs))*(-expm1(-g*(1/cg+1/cs)*.1));
 assert(fabs(f.gas_to_surface_j-q)<1e-6);assert(fabs(cfd_passive3d_total(s.energy,s.grid.count)+f.gas_to_surface_j-initial)<1e-10);
 for(int k=0;k<64;k++)if(k<44||k>=48)for(int j=0;j<16;j++)assert(s.energy[k*16+j]==saved[k*16+j]);
 cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);assert(!budget.live_bytes);puts("PASS slab alignment, zero transfer, refusal atomicity, analytic exchange, energy closure, unchanged outside support");
}
