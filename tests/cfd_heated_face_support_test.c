#include "../src/tools/physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    CfdMemoryBudget memory={.limit_bytes=512*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    double greatest=0;
    for(int axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2) {
        double results[2];
        for(int fine=0;fine<2;fine++) {
            int side=32*(1+fine),n[3]={side,side,side},lo[3],hi[3];
            for(int a=0;a<3;a++){lo[a]=3*side/8;hi[a]=5*side/8;}
            CfdOpenAtmosphere3d s;double l[3]={1,1,1},p[2]={0,0},t[2]={300,300};
            assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,0,0,.0025,p,t,p,false,9.81,1./300,.1));
            s.ground=true;assert(cfd_open_atmosphere3d_box(&s,lo,hi));
            for(int q=0;q<s.grid.count;q++)if(!cfd_open_atmosphere3d_solid(&s,q)) {
                double x=(q%side+.5)/side,y=(q/side%side+.5)/side,z=(q/(side*side)+.5)/side;
                s.energy[q]=1206*s.grid.volume*(2+x+2*y+3*z);
            }
            PhysicsSimHeatedFace f={axis,sign,300,100,10,0,0};double rise;
            assert(physics_sim_heated_face_sample(&s,&f,1./32,&rise));
            double center=sign<0?.375-1./64:.625+1./64;
            double expected_mean=5+(axis+1)*(center-.5);
            assert(fabs(rise-expected_mean)<1e-12);
            double before=cfd_passive3d_total(s.energy,s.grid.count);
            double cg=1206*.0625/32,cs=100,g=.625;
            double expected_rise=cg*rise/(cg+cs)*(-expm1(-g*(1/cg+1/cs)*.2));
            for(int step=0;step<20;step++)assert(physics_sim_heated_face_exchange_depth(&s,&f,.01,1./32));
            double actual=f.temperature_k-300+f.temperature_correction_k;
            results[fine]=actual;greatest=fmax(greatest,fabs(actual-expected_rise));
            assert(fabs(actual-expected_rise)<1e-8);
            assert(fabs(cfd_passive3d_total(s.energy,s.grid.count)+f.gas_to_surface_j-before)<1e-9);
            assert(fabs(actual*100-f.gas_to_surface_j)<1e-10);
            /* Physical support refusal never changes either participant. */
            double heat=cfd_passive3d_total(s.energy,s.grid.count);PhysicsSimHeatedFace saved=f;
            for(int bad=0;bad<3;bad++) {
                double depth=(double[]){.031,1.,NAN}[bad];
                assert(!physics_sim_heated_face_exchange_depth(&s,&f,.1,depth));
                assert(!physics_sim_heated_face_sample(&s,&f,depth,&rise));
            }
            assert(!memcmp(&saved,&f,sizeof f));assert(cfd_passive3d_total(s.energy,s.grid.count)==heat);
            cfd_open_atmosphere3d_destroy(&s);
        }
        assert(fabs(results[0]-results[1])<1e-8);
    }
    assert(memory.live_bytes==0);cfd_memory_scope(prior);
    printf("{\"fixed_support_cases\":12,\"analytic_max_error_k\":%.17g,\"live_bytes\":0}\n",greatest);
}
