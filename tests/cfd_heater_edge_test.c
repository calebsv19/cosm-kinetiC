/* Metamorphic invariants and atomic setup; independent cell-oracle checks
 * are run separately by the bounded heater-edge qualification. */
#include "../src/tools/physics_sim_tail_time.h"
#include <assert.h>
#include <stdio.h>
static PhysicsSimTailDiffusion s;
static double power[256],w[256],field[256];
static void means(double v[4],double h,double alpha,double jump,double young,double old,double background) {
    double a[2];
    for(int k=0;k<2;k++)a[k]=jump*(old*physics_sim_heater_cold_mean(sqrt(4*alpha*old)/h,k)-(young>0?young*physics_sim_heater_cold_mean(sqrt(4*alpha*young)/h,k):0));
    v[0]=background+a[1];v[1]=background+a[0];v[2]=background+jump*(old-young)-a[0];v[3]=background+jump*(old-young)-a[1];
}
int main(void) {
    double h=.0625/256,alpha=.026/1206,width[4]={h,h,h,h},worst=0;
    int checks=0;
    const double scales[]={1e-300,.25,1,4};
    for(unsigned scale=0;scale<sizeof(scales)/sizeof(*scales);scale++)
    for(int tick=1;tick<=1024;tick*=2)
    for(int phase=0;phase<3;phase++) {
        double young=phase/51200.,old=young+tick/51200.,v[4],g=0;
        means(v,h,alpha,scales[scale],young,old,0);
        assert(physics_sim_heater_edge_fit(v,width,scales[scale],alpha,&g));
        double exact=scales[scale]*(sqrt(old)-sqrt(young))/sqrt(acos(-1)*alpha),error=fabs(g/exact-1);
        worst=fmax(worst,error);assert(error<1e-7);checks++;
        double reverse[4]={v[3],v[2],v[1],v[0]},back=0;
        assert(physics_sim_heater_edge_fit(reverse,width,-scales[scale],alpha,&back));assert(back==-g);checks++;
        if(scale>0){for(int k=0;k<4;k++)v[k]+=.001;assert(physics_sim_heater_edge_fit(v,width,scales[scale],alpha,&back));assert(fabs(back/g-1)<1e-7);checks++;}
    }
    double v[4],g=123;
    means(v,h,alpha,1,0,1./51200,0);
    assert(!physics_sim_heater_edge_fit(v,width,0,alpha,&g)&&g==123);
    assert(!physics_sim_heater_edge_fit(v,width,1,0,&g)&&g==123);
    width[1]*=2;assert(!physics_sim_heater_edge_fit(v,width,1,alpha,&g)&&g==123);width[1]=h;
    v[0]=NAN;assert(!physics_sim_heater_edge_fit(v,width,1,alpha,&g)&&g==123);
    for(int j=0;j<256;j++){w[j]=h;power[j]=(j>=64&&j<128);}
    assert(physics_sim_tail_diffusion_init(&s,256,w,false));assert(!s.heater_power);
    assert(physics_sim_tail_heater_source(&s,power,alpha));power[0]=-1;
    assert(!physics_sim_tail_heater_source(&s,power,alpha));power[0]=0;assert(s.heater_power==power&&s.heater_alpha==alpha);
    for(int step=0;step<128;step++) {
        field[64]+=1e-5;
        assert(physics_sim_tail_euler(&s,field,alpha,1./51200,2));
        double sum=0;for(int j=0;j<256;j++){assert(isfinite(field[j])&&field[j]>=0&&field[j]<=.4+1e-10);sum+=field[j];}
        assert(fabs(sum/((step+1)*1e-5)-1)<1e-12);
    }
    assert(field[63]>0&&field[65]>0);
    printf("%d heater fit invariance checks; worst relative %.17g; paired bounds, mass, cold transfer and atomic setup pass\n",checks,worst);
}
