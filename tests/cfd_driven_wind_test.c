/* Independent mixed-pressure equation, commanded faces and atomic acceptance. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void setup(CfdOpenAtmosphere3d *s,int side,int body,int linear,int cache) {
    int g[3]={8,8,8},steps[3]={0,10,30};double l[3]={1,1,1},p[2]={0},t[2]={300,300},a[2]={0},speeds[3]={0,.3,0};
    assert(cfd_open_atmosphere3d_init(s,g,l,1,1e-5,1000,300,0,0,.01,p,t,a,true,0,1./300,.1));
    s->ground=true;s->cache_projection=cache;assert(cfd_open_atmosphere3d_lateral(s));
    int lo[3]={3,3,3},hi[3]={5,5,5};if(body)assert(cfd_open_atmosphere3d_box(s,lo,hi));
    assert(cfd_open_wind_configure(s,side,.25,linear,3,steps,speeds));
}
static void pressure_test(int side,int body,int cache) {
    CfdOpenAtmosphere3d s={0};setup(&s,side,body,0,cache);double exact[512];
    for(int q=0;q<512;q++)exact[q]=cfd_open_atmosphere3d_solid(&s,q)?0:sin(.2*(q%8))+.1*(q/64);
    for(int q=0;q<512;q++) {
        s.rhs[q]=0;if(cfd_open_atmosphere3d_solid(&s,q))continue;
        int p[3]={q%8,q/8%8,q/64};
        for(int a=0;a<3;a++)for(int sign=-1;sign<=1;sign+=2) {
            int v[3]={p[0],p[1],p[2]};v[a]+=sign;
            if(v[a]<0 || v[a]>=8) {
                if(!(a==2 && sign<0) && !(a==0 && sign==(side?1:-1)))s.rhs[q]+=128*exact[q];
            } else {int other=(v[2]*8+v[1])*8+v[0];if(!cfd_open_atmosphere3d_solid(&s,other))s.rhs[q]+=64*(exact[q]-exact[other]);}
        }
    }
    double residual;assert(cfd_open_atmosphere3d_box_pressure(&s,&residual));
    for(int q=0;q<512;q++)assert(fabs(s.candidate_pressure[q]-exact[q])<1e-9);
    cfd_open_atmosphere3d_destroy(&s);
}
static void flow_test(int side,int body,int linear) {
    CfdOpenAtmosphere3d s={0};setup(&s,side,body,linear,body);
    double source[512]={0},smoke[512]={0},volume=0;
    for(int step=0;step<50;step++) {
        double speed=linear?(step<10?.03*step:step<30?.3*(30-step)/20:0):(step>=10 && step<30?.3:0);
        assert(cfd_open_atmosphere3d_step(&s,source,smoke));
        for(int k=0;k<8;k++)for(int j=0;j<8;j++) {
            double expected=(side?-1:1)*speed*fmin(1,(k+.5)/2);
            int q=side?3*512+64+k*8+j:(k*8+j)*8;
            assert(fabs(s.velocity[q]-expected)<1e-14);
        }
        volume+=speed*.875*.01;
        assert(fabs(s.wind_inlet_volume-volume)<1e-14);
        assert(fabs(s.wind_air_in-s.wind_air_out)<1e-12);
        assert(cfd_open_atmosphere3d_valid(&s));
        for(int q=0;q<512;q++)if(cfd_open_atmosphere3d_solid(&s,q))assert(s.energy[q]==0 && s.smoke[q]==0);
    }
    /* Refusal after the projected candidate has been staged cannot publish air,
     * scalar fields, time, counters or projected accepted velocities. */
    double before[4096],flux[4096];int v=s.velocity_count,n=s.grid.count;
    assert(v+3*n<4096 && 4*s.boundary_faces<4096);
    memcpy(before,s.velocity,(size_t)(v+3*n)*sizeof(double));memcpy(flux,s.flux,(size_t)4*s.boundary_faces*sizeof(double));
    double air[3]={s.wind_air_in,s.wind_air_out,s.wind_inlet_volume};int steps=s.steps;
    source[0]=1e9;assert(!cfd_open_atmosphere3d_step(&s,source,smoke));
    assert(steps==s.steps && s.wind_air_in==air[0] && s.wind_air_out==air[1] && s.wind_inlet_volume==air[2]);
    assert(!memcmp(before,s.velocity,(size_t)(v+3*n)*sizeof(double)) && !memcmp(flux,s.flux,(size_t)4*s.boundary_faces*sizeof(double)));
    cfd_open_atmosphere3d_destroy(&s);
}
int main(void) {
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    for(int side=0;side<2;side++)for(int body=0;body<2;body++)for(int option=0;option<2;option++) {
        pressure_test(side,body,option);flow_test(side,body,option);
    }
    assert(memory.live_bytes==0);cfd_memory_scope(prior);puts("driven inlet pressure, gust, air ledger and atomic refusal passed");return 0;
}
