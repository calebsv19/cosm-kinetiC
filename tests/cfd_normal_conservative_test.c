#include "../src/tools/physics_sim_normal_conservative.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const double alpha=.026/1206,dt=1./51200,h=.0625/512;
    PhysicsSimLocalMatrix matrix={0};double width[512],v[512],low[512]={0};
    for(int i=0;i<512;i++){width[i]=h;v[i]=20;}
    assert(physics_sim_local_matrix_init(&matrix,512,width,0,alpha,dt,false));
    for(int step=0;step<51200;step++)assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,dt,1));
    double worst=0;for(int i=0;i<512;i++){worst=fmax(worst,fabs(v[i]-21));assert(v[i]==21);}
    int cold=0;
    for(int scale=0;scale<3;scale++) {
        double amplitude=scale==0?20:scale==1?1e-200:1e-310;
        for(int i=0;i<512;i++){v[i]=i==0?amplitude:0;low[i]=0;}
        for(int step=0;step<32;step++) {
            assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,0,1));
            assert(v[1]>0&&v[0]>=0);double total=0;for(int i=0;i<512;i++){assert(v[i]>=0&&v[i]<=amplitude);total+=v[i];}
            assert(fabs(total-amplitude)<=1e-12*amplitude+512*0x1p-1074);cold++;
        }
    }
    int parity=0;double max_be_error=0;
    for(int pattern=0;pattern<3;pattern++) {
        double reference[512];
        for(int i=0;i<512;i++){v[i]=pattern==0?1+.2*sin(i*.1):pattern==1?(i==200?1:0):(i%3==0?1e-200:0);reference[i]=v[i];low[i]=0;}
        for(int step=0;step<16;step++) {
            assert(physics_sim_local_matrix_step(&matrix,reference));
            assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,0,1));
            for(int i=0;i<512;i++) {
                double error=fabs(v[i]-reference[i]);max_be_error=fmax(max_be_error,error);
                assert(error<=4096*DBL_EPSILON*fmax(fabs(v[i]),fabs(reference[i]))+16*0x1p-1074);
            }
            parity++;
        }
    }
    double saved[512],saved_low[512];memcpy(saved,v,sizeof v);memcpy(saved_low,low,sizeof low);
    assert(!physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,NAN,1));assert(!memcmp(v,saved,sizeof v)&&!memcmp(low,saved_low,sizeof low));
    physics_sim_local_matrix_destroy(&matrix);
    assert(physics_sim_local_matrix_init(&matrix,512,width,0,alpha,dt*.5,false));
    for(int i=0;i<512;i++){v[i]=20;low[i]=0;}
    for(int step=0;step<51200;step++)assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,dt,.5));
    for(int i=0;i<512;i++)assert(v[i]==21);
    const double pi=acos(-1),eigen=4*alpha/(h*h)*pow(sin(pi/(2*512)),2);
    const double gain=(1-dt*eigen/2)/(1+dt*eigen/2);
    for(int i=0;i<512;i++){v[i]=1+.2*cos(pi*(i+.5)/512);low[i]=0;}
    for(int step=0;step<64;step++)assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,0,.5));
    double cn_error=0;for(int i=0;i<512;i++){double expected=1+.2*pow(gain,64)*cos(pi*(i+.5)/512);cn_error=fmax(cn_error,fabs(v[i]-expected));assert(cn_error<2e-14);}
    for(int scale=0;scale<3;scale++) {
        double amplitude=scale==0?20:scale==1?1e-200:1e-310;
        for(int i=0;i<512;i++){v[i]=i==0?amplitude:0;low[i]=0;}
        for(int step=0;step<32;step++){assert(physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,dt,h,0,.5));assert(v[1]>0);for(int i=0;i<512;i++)assert(v[i]>=0&&v[i]<=amplitude);}
    }
    memcpy(saved,v,sizeof v);memcpy(saved_low,low,sizeof low);
    assert(!physics_sim_normal_conservative_step(&matrix,v,low,512,alpha,1,h,0,.5));assert(!memcmp(saved,v,sizeof v)&&!memcmp(saved_low,low,sizeof low));
    physics_sim_local_matrix_destroy(&matrix);
    printf("{\"uniform_heater_steps\":51200,\"uniform_error\":%.17g,\"cold_subnormal_steps\":%d,\"be_parity_steps\":%d,\"be_max_error\":%.17g,\"cn_eigenmode_error\":%.17g,\"cn_cold_steps\":96,\"refusal_atomic\":true}\n",worst,cold,parity,max_be_error,cn_error);
    return 0;
}
