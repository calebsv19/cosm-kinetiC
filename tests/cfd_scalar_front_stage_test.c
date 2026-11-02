/* Verify missing diffusion at the second cold cell, not just formula parity. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
static double low[256],correction[256],captured_factor[256];
static void capture_face(int i,double l,double a,double f) {
    low[i]=l;correction[i]=a;captured_factor[i]=f;
}
#define PHYSICS_SIM_FCT_FACE_DIAGNOSTIC(i,l,a,f) capture_face(i,l,a,f)
#include "../src/tools/physics_sim_scalar_fct.h"
int main(void) {
    double h=.25/128,dt=1./3200,alpha=.026/1206;int checks=0;
    for(int direction=-1;direction<=1;direction++)for(int scale_case=0;scale_case<3;scale_case++) {
        double scale=scale_case==0?1:scale_case==1?1e-12:1e6;
        double v[128]={0},stage[128],out[128];for(int i=16;i<32;i++)v[i]=scale;
        assert(physics_sim_scalar_fct(v,stage,128,direction*.003125,alpha,h,dt,true));
        assert(stage[32]>0 && stage[33]==0);
        assert(physics_sim_scalar_fct(stage,out,128,direction*.003125,alpha,h,dt,true));
        /* At face 33 the polynomial derivative has the wrong sign. The
         * donor is zero, so the expected retained transfer is purely diffusion. */
        double expected=dt*alpha*stage[32]/(h*h);
        assert(out[33]>0 && fabs(out[33]/expected-1)<1e-12);
        assert(captured_factor[33]==1 && fabs((low[33]+correction[33])/expected-1)<1e-12);
        double sum=0;for(int i=0;i<128;i++)sum+=out[i];
        assert(fabs(sum/scale-16)<1e-12);
        checks++;
    }
    for(int i=0;i<2;i++){
        double f,g,oldf,oldg;double v[2][4]={{1,1,0,0},{1,1.1,1.2,1.3}};
        physics_sim_scalar_front_target(v[i][0],v[i][1],v[i][2],v[i][3],.003125,h,&f,&g);
        cfd_scalar_edge_candidate(v[i][0],v[i][1],v[i][2],v[i][3],.003125,h,&oldf,&oldg);
        assert(f==oldf && g==oldg);checks++;
    }
    printf("{\"front_stage_checks\":%d,\"second_cold_cell_diffusion_restored\":true}\n",checks);
}
