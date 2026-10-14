/* Independent full-face pressure equation and native lateral transport budgets. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    for(int mode=0;mode<4;mode++) {
        int body=mode%2;
        CfdOpenAtmosphere3d s={0};int g[3]={8,8,8};double length[3]={1,1,1},p[2]={0,0},t[2]={300,300},a[2]={0,0};
        assert(cfd_open_atmosphere3d_init(&s,g,length,1,1e-9,1000,300,0,0,.005,p,t,a,false,9.81,1./300,.1));
        s.ground=true;s.cache_projection=mode>=2;assert(cfd_open_atmosphere3d_lateral(&s));
        int lo[3]={3,3,3},hi[3]={5,5,5};if(body)assert(cfd_open_atmosphere3d_box(&s,lo,hi));
        double exact[512];
        for(int q=0;q<512;q++)exact[q]=(body && q%8>=3 && q%8<5 && q/8%8>=3 && q/8%8<5 && q/64>=3 && q/64<5)?0:sin(.3*(q%8))+.1*(q/64);
        for(int q=0;q<512;q++) {
            if(body && q%8>=3 && q%8<5 && q/8%8>=3 && q/8%8<5 && q/64>=3 && q/64<5){s.rhs[q]=0;continue;}
            s.rhs[q]=0;int xyz[3]={q%8,q/8%8,q/64};
            for(int axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2) {
                int v[3]={xyz[0],xyz[1],xyz[2]};v[axis]+=sign;
                if(v[axis]<0 || v[axis]>=8){if(axis!=2 || sign>0)s.rhs[q]+=128*exact[q];continue;}
                bool solid=body && v[0]>=3 && v[0]<5 && v[1]>=3 && v[1]<5 && v[2]>=3 && v[2]<5;
                if(!solid)s.rhs[q]+=64*(exact[q]-exact[(v[2]*8+v[1])*8+v[0]]);
            }
        }
        double residual;assert(cfd_open_atmosphere3d_box_pressure(&s,&residual));
        for(int q=0;q<512;q++)assert(fabs(s.candidate_pressure[q]-exact[q])<1e-9);
        /* Cached preconditioning must also solve a different RHS without
         * retaining a stale solution or leaking between true residual checks. */
        for(int q=0;q<512;q++)s.rhs[q]*=.37;
        assert(cfd_open_atmosphere3d_box_pressure(&s,&residual));
        for(int q=0;q<512;q++)assert(fabs(s.candidate_pressure[q]-.37*exact[q])<1e-9);
        cfd_open_atmosphere3d_destroy(&s);
    }
    assert(memory.live_bytes==0);cfd_memory_scope(prior);puts("independent lateral/box pressure equation passed");
    return 0;
}
