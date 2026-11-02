/* Force-law tests before solver integration: native MAC geometry and mass. */
#include "app/cfd_open_atmosphere3d_weather.h"
#include "app/cfd_open_energy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void setup(CfdOpenAtmosphere3d *s,bool box) {
    int n[3]={8,8,8};double length[3]={1,1,1},p[2]={0},t[2]={300,300},m[2]={0};
    assert(cfd_open_atmosphere3d_init(s,n,length,1.2,1e-5,1000,300,0,0,
        .005,p,t,m,false,0,1./300,.1));
    s->ground=true;assert(cfd_open_atmosphere3d_lateral(s));
    if(box){int lo[3]={3,3,3},hi[3]={5,5,5};assert(cfd_open_atmosphere3d_box(s,lo,hi));}
}
static CfdOpenAtmosphereWeather constant(CfdOpenAtmosphere3d *s,double target,double strength) {
    CfdOpenAtmosphereWeather w={0};int step=0;
    assert(cfd_open_weather_configure(s,&w,.125,.1,false,1,&step,&target,&strength));
    return w;
}
static void reference(bool box) {
    CfdOpenAtmosphere3d s={0};setup(&s,box);CfdOpenAtmosphereWeather w=constant(&s,.5,1);
    double total=0;
    for(int step=0;step<100;step++) {
        double before=cfd_open_kinetic(&s,s.candidate_velocity),work;
        assert(cfd_open_weather_apply(&s,&w,&work));
        assert(work==cfd_open_kinetic(&s,s.candidate_velocity)-before);total+=work;
        for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int i=0;i<=8;i++) {
            int q=cfd_open_atmosphere3d_face(&s,0,i,j,k);
            double expected=cfd_open_atmosphere3d_blocked(&s,0,i,j,k)?0:
                .5*fmin(1,(k+.5)/1.)*(1-exp(-(step+1)*.005/.1));
            assert(fabs(s.candidate_velocity[q]-expected)<2e-15);
        }
        /* The force acts only on X; accepted state stays untouched. */
        for(int q=512;q<3*512+64;q++)assert(s.candidate_velocity[q]==0);
        for(int q=0;q<s.velocity_count;q++)assert(s.velocity[q]==0);
    }
    assert(fabs(total-cfd_open_kinetic(&s,s.candidate_velocity))<1e-14);
    /* Release preserves all candidate bits; a zero-speed positive driver brakes. */
    w.strength[0]=0;double saved[1728],work=9;
    assert(s.velocity_count==1728);memcpy(saved,s.candidate_velocity,sizeof(saved));
    assert(cfd_open_weather_apply(&s,&w,&work) && work==0);
    assert(!memcmp(saved,s.candidate_velocity,sizeof(saved)));
    w.strength[0]=1;w.target_m_s[0]=0;
    assert(cfd_open_weather_apply(&s,&w,&work) && work<0);
    w.target_m_s[0]=-.5;
    for(int i=0;i<100;i++)assert(cfd_open_weather_apply(&s,&w,&work));
    assert(s.candidate_velocity[cfd_open_atmosphere3d_face(&s,0,1,1,1)]<0);
    cfd_open_atmosphere3d_destroy(&s);
}
static void schedules_and_refusals(void) {
    CfdOpenAtmosphere3d s={0};setup(&s,false);
    int steps[3]={0,20,40};double targets[3]={.5,.5,-.5},strengths[3]={1,1,0};
    CfdOpenAtmosphereWeather w={0};
    assert(cfd_open_weather_configure(&s,&w,.25,.1,true,3,steps,targets,strengths));
    double u,g;assert(cfd_open_weather_command(&w,30,&u,&g) && u==0 && g==.5);
    assert(cfd_open_weather_command(&w,40,&u,&g) && u==-.5 && g==0);
    w.linear=false;assert(cfd_open_weather_command(&w,30,&u,&g) && u==.5 && g==1);
    assert(!cfd_open_weather_command(&w,-1,&u,&g));
    double before[1728];memcpy(before,s.candidate_velocity,sizeof(before));
    w.strength[0]=NAN;double work=123;
    assert(!cfd_open_weather_apply(&s,&w,&work) && work==123);
    assert(!memcmp(before,s.candidate_velocity,sizeof(before)));
    w.strength[0]=1;s.candidate_velocity[100]=NAN;
    memcpy(before,s.candidate_velocity,sizeof(before));
    assert(!cfd_open_weather_apply(&s,&w,&work) && work==123);
    assert(!memcmp(before,s.candidate_velocity,sizeof(before)));
    s.candidate_velocity[100]=0;s.wind.enabled=true;
    assert(!cfd_open_weather_valid(&s,&w));s.wind.enabled=false;
    CfdOpenAtmosphereWeather refused={0},empty=refused;strengths[0]=2;
    assert(!cfd_open_weather_configure(&s,&refused,.25,.1,false,3,steps,targets,strengths));
    assert(!memcmp(&refused,&empty,sizeof(empty)));
    cfd_open_atmosphere3d_destroy(&s);
}
int main(void) {
    reference(false);reference(true);schedules_and_refusals();
    puts("open weather force reference, release, reversal, work, geometry and refusal: passed");
    return 0;
}
