#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_open_energy.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL fluid %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void init(CfdOpenAtmosphere3d *s){int n[3]={8,8,8};double l[3]={1,1,1},zero[2]={0},t[2]={293.15,293.15};CHECK(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,293.15,0,0,.005,zero,t,zero,false,9.81,0,.1));s->ground=true;}
static void same(CfdOpenAtmosphere3d *a,CfdOpenAtmosphere3d *b){CHECK(!memcmp(a->velocity,b->velocity,(a->velocity_count+3*a->grid.count)*sizeof(double)));CHECK(!memcmp(a->flux,b->flux,4*a->boundary_faces*sizeof(double)));CHECK(a->steps==b->steps&&a->time==b->time&&a->input_j==b->input_j&&a->input_kg==b->input_kg&&a->scalar_work_cells==b->scalar_work_cells&&a->divergence==b->divergence&&a->residual==b->residual);}
int main(void){CfdOpenAtmosphere3d a,b;init(&a);init(&b);double z[512]={0},j[1600]={0};CfdOpenMomentumReceipt r={0};
 for(unsigned t=0;t<10;t++){CHECK(cfd_open_atmosphere3d_step(&a,z,z));CHECK(cfd_open_atmosphere3d_step_momentum(&b,z,z,j,1600,&r));same(&a,&b);}
 cfd_open_atmosphere3d_destroy(&a);cfd_open_atmosphere3d_destroy(&b);init(&a);init(&b);
 CfdOpenMomentumReceipt sentinel;memset(&sentinel,0x55,sizeof(sentinel));r=sentinel;
 CHECK(!cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1599,&r));CHECK(!memcmp(&r,&sentinel,sizeof(r)));same(&a,&b);
 j[0]=1e-8;CHECK(!cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1600,&r));same(&a,&b);j[0]=0;
 int face=cfd_open_atmosphere3d_face(&a,0,4,3,2);CHECK(face==4+8*(3+8*2));j[face]=NAN;CHECK(!cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1600,&r));same(&a,&b);
 j[face]=1e6;CHECK(!cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1600,&r));CHECK(!memcmp(&r,&sentinel,sizeof(r)));same(&a,&b);
 j[face]=1e-5;a.max_scalar_work_cells=1;CHECK(!cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1600,&r));CHECK(!memcmp(&r,&sentinel,sizeof(r)));same(&a,&b);a.max_scalar_work_cells=100000000;
 CHECK(cfd_open_atmosphere3d_step_momentum(&a,z,z,j,1600,&r));CHECK(cfd_open_atmosphere3d_valid(&a));CHECK(fabs(r.impulse_ns[0]-1e-5)<1e-18);CHECK(fabs(r.forced_momentum_ns[0]-r.predictor_momentum_ns[0]-1e-5)<1e-18);CHECK(fabs(r.work_j-1e-10/(2*1.2/512))<1e-18);CHECK(fabs(r.work_j-r.quadratic_j)<1e-18);CHECK(fabs(cfd_open_kinetic(&a,a.velocity)-r.work_j-r.projection_work_j)<1e-18);CHECK(a.divergence<1e-8&&a.residual<1e-8);
 printf("PASS native momentum inlet: %u checks; zero parity, native face order/dual mass, finite/boundary/CFL/late-scalar rollback, projected impulse and kinetic work\n",checks);cfd_open_atmosphere3d_destroy(&a);cfd_open_atmosphere3d_destroy(&b);return 0;}
