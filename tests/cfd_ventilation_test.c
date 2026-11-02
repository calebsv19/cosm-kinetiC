/* Independent discrete pressure and SI conservation for opt-in vent boundaries. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void setup(CfdOpenAtmosphere3d *s,int balanced,int side,int body,int cache) {
    int g[3]={8,8,8},steps[3]={0,15,30};double l[3]={1,1,1},p[2]={0},t[2]={300,300},a[2]={0};
    double speeds[3]={.3,balanced?-.3:0,0};
    assert(cfd_open_atmosphere3d_init(s,g,l,1,1e-5,1000,300,0,0,.01,p,t,a,true,0,1./300,.1));
    s->ground=true;s->cache_projection=cache;assert(cfd_open_atmosphere3d_lateral(s));
    if(body){int lo[3]={3,3,3},hi[3]={5,5,5};assert(cfd_open_atmosphere3d_box(s,lo,hi));}
    if(balanced)assert(cfd_open_wind_balanced(s,.25,false,3,steps,speeds));
    else {
        assert(cfd_open_wind_configure(s,side,.25,false,3,steps,speeds));
        int lo[2]={2,2},hi[2]={6,6};assert(cfd_open_wind_patch(s,lo,hi));
    }
}
static void pressure_test(int balanced,int side,int body,int cache) {
    CfdOpenAtmosphere3d s={0};setup(&s,balanced,side,body,cache);double exact[512];
    for(int q=0;q<512;q++)exact[q]=cfd_open_atmosphere3d_solid(&s,q)?0:sin(.2*(q%8))+.1*(q/64);
    for(int q=0;q<512;q++) {
        s.rhs[q]=0;if(cfd_open_atmosphere3d_solid(&s,q))continue;
        int p[3]={q%8,q/8%8,q/64};
        for(int a=0;a<3;a++)for(int sign=-1;sign<=1;sign+=2) {
            int v[3]={p[0],p[1],p[2]};v[a]+=sign;
            if(v[a]<0 || v[a]>=8) {
                bool neumann=(a==2 && sign<0) || (balanced && a==0) ||
                    (!balanced && (a==1 || a==2 || (a==0 && sign==(side?1:-1))));
                if(!neumann)s.rhs[q]+=128*exact[q];
            } else {int other=(v[2]*8+v[1])*8+v[0];if(!cfd_open_atmosphere3d_solid(&s,other))s.rhs[q]+=64*(exact[q]-exact[other]);}
        }
    }
    double residual;assert(cfd_open_atmosphere3d_box_pressure(&s,&residual));
    for(int q=0;q<512;q++)assert(fabs(s.candidate_pressure[q]-exact[q])<1e-9);
    cfd_open_atmosphere3d_destroy(&s);
}
static void flow_test(int balanced,int side,int body,int cache) {
    CfdOpenAtmosphere3d s={0};setup(&s,balanced,side,body,cache);double e[512]={0},m[512]={0},volume=0;
    e[0]=1e-6;m[0]=1e-12;
    for(int step=0;step<45;step++) {
        double speed=step<15?.3:(balanced && step<30?-.3:0);
        assert(cfd_open_atmosphere3d_step(&s,e,m));
        for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int xside=0;xside<2;xside++) {
            if(!balanced && xside!=side)continue;
            double expected=balanced?speed*fmin(1,(k+.5)/2):
                ((j>=2 && j<6 && k>=2 && k<6)?(side?-speed:speed):0);
            int face=cfd_open_atmosphere3d_face(&s,0,xside?8:0,j,k);
            assert(fabs(s.velocity[face]-expected)<1e-14);
        }
        volume+=fabs(speed)*(balanced?.875:.25)*.01;
        assert(fabs(s.wind_inlet_volume-volume)<1e-14);
        assert(fabs(s.wind_air_in-s.wind_air_out)<1e-11);
        double total_e=0,total_m=0,net_e=0,net_m=0;
        for(int q=0;q<512;q++){total_e+=s.energy[q];total_m+=s.smoke[q];}
        for(int q=0;q<s.boundary_faces;q++) {
            net_e+=s.flux[s.boundary_faces+q]-s.flux[q];
            net_m+=s.flux[3*s.boundary_faces+q]-s.flux[2*s.boundary_faces+q];
        }
        assert(fabs(total_e+net_e-s.input_j)<1e-15);
        assert(fabs(total_m+net_m-s.input_kg)<1e-20);
        assert(cfd_open_atmosphere3d_valid(&s));
    }
    double saved[4096],flux[4096];int count=s.velocity_count+3*s.grid.count;
    assert(count<4096 && 4*s.boundary_faces<4096);
    memcpy(saved,s.velocity,(size_t)count*sizeof(double));memcpy(flux,s.flux,(size_t)4*s.boundary_faces*sizeof(double));
    int step=s.steps;double volume0=s.wind_inlet_volume,input=s.input_j;e[0]=1e12;
    assert(!cfd_open_atmosphere3d_step(&s,e,m));assert(s.steps==step && s.wind_inlet_volume==volume0 && s.input_j==input);
    assert(!memcmp(saved,s.velocity,(size_t)count*sizeof(double)) && !memcmp(flux,s.flux,(size_t)4*s.boundary_faces*sizeof(double)));
    cfd_open_atmosphere3d_destroy(&s);
}
static void closure_bound_test(int balanced) {
    CfdOpenAtmosphere3d s={0};setup(&s,balanced,0,0,0);
    s.velocity[cfd_open_atmosphere3d_face(&s,0,3,3,3)]=.3;
    int axis=balanced?2:0;
    for(int j=0;j<8;j++)for(int k=0;k<8;k++) {
        int x[3]={0};x[axis]=8;x[(axis+1)%3]=j;x[(axis+2)%3]=k;
        s.candidate_velocity[cfd_open_atmosphere3d_face(&s,axis,x[0],x[1],x[2])]=1e-15;
    }
    assert(cfd_open_wind_roundoff_balance(&s));
    for(int j=0;j<8;j++)for(int k=0;k<8;k++) {
        int x[3]={0};x[axis]=8;x[(axis+1)%3]=j;x[(axis+2)%3]=k;
        int q=cfd_open_atmosphere3d_face(&s,axis,x[0],x[1],x[2]);
        assert(fabs(s.candidate_velocity[q])<1e-28);s.candidate_velocity[q]=1e-4;
    }
    assert(!cfd_open_wind_roundoff_balance(&s));
    cfd_open_atmosphere3d_destroy(&s);
}
int main(void) {
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *old=cfd_memory_scope(&memory);
    for(int balanced=0;balanced<2;balanced++)for(int side=0;side<2;side++)for(int body=0;body<2;body++)for(int cache=0;cache<2;cache++) {
        pressure_test(balanced,side,body,cache);flow_test(balanced,side,body,cache);
    }
    closure_bound_test(0);closure_bound_test(1);
    assert(memory.live_bytes==0);cfd_memory_scope(old);puts("vent/reversal mixed pressure, boundary, heat/smoke/air and rollback passed");
}
