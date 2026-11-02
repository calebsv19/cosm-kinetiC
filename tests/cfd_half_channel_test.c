#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void init(CfdOpenAtmosphere3d *s,double height) {
    int n[3]={8,8,8},steps[1]={0};double l[3]={1,1,1},p[2]={0},a[2]={300,300},v[1]={.1};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,.12,1005,300,0,0,.002,p,a,p,false,0,0,.1));
    s->ground=true;assert(cfd_open_atmosphere3d_lateral(s));
    assert(cfd_open_wind_configure(s,0,height,false,1,steps,v));
}
int main(void) {
    CfdMemoryBudget m={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&m);
    CfdOpenAtmosphere3d s={0};init(&s,1);
    assert(!cfd_open_wind_half_channel(&s));int lo[2]={0,0},hi[2]={8,8};
    assert(cfd_open_wind_patch(&s,lo,hi));assert(cfd_open_wind_half_channel(&s));
    assert(!cfd_open_wind_half_channel(&s));
    for(int k=0;k<8;k++){double z=(k+.5)/8;assert(fabs(cfd_open_wind_profile(&s,k,0)-.1*z*(2-z))<1e-15);}
    double e[512]={0};assert(cfd_open_atmosphere3d_step(&s,e,e));assert(cfd_open_atmosphere3d_valid(&s));
    assert(!cfd_open_wind_half_channel(&s));cfd_open_atmosphere3d_destroy(&s);
    init(&s,.25);assert(cfd_open_wind_patch(&s,lo,hi));assert(!cfd_open_wind_half_channel(&s));
    assert(!s.wind.half_channel);cfd_open_atmosphere3d_destroy(&s);
    assert(m.live_bytes==0);cfd_memory_scope(prior);puts("half-channel profile and immutable admission passed");
}
