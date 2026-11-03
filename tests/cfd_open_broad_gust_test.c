#include "app/cfd_open_atmosphere3d_weather.h"
#include "app/cfd_open_energy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void trial(int n) {
    CfdOpenAtmosphere3d s={0};int grid[3]={n,n,n};
    double length[3]={1,1,1},p[2]={0},temp[2]={300,300},smoke[2]={0};
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1e-5,1000,300,0,0,.005,p,temp,smoke,false,0,1./300,.1));
    s.ground=true;assert(cfd_open_atmosphere3d_lateral(&s));assert(cfd_open_atmosphere3d_rig(&s,true));
    int step=0;double target=.5,strength=1;CfdOpenAtmosphereWeather w={0};
    assert(cfd_open_weather_configure(&s,&w,.25,.2,false,1,&step,&target,&strength));
    w.broad_sweeping_shear=true;assert(cfd_open_weather_valid(&s,&w));
    double work=9;assert(cfd_open_weather_apply(&s,&w,&work));
    assert(work==cfd_open_kinetic(&s,s.candidate_velocity));
    int changed=0,beyond_old_strip=0;double lo=1,hi=0;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<=n;i++) {
        int q=cfd_open_atmosphere3d_face(&s,0,i,j,k);double u=s.candidate_velocity[q];
        if(cfd_open_atmosphere3d_blocked(&s,0,i,j,k)){assert(u==0);continue;}
        if(i*s.grid.h[0]>.4375){assert(u==0);continue;}
        if(u>0){changed++;if(i*s.grid.h[0]>.125)beyond_old_strip++;if(i==n/4){lo=fmin(lo,u);hi=fmax(hi,u);}}
        assert(u>=0 && u<=.5*(-expm1(-.005/.2)));
    }
    assert(changed>0 && beyond_old_strip>0 && hi-lo>1e-4);
    /* Other axes and accepted velocity are untouched. */
    int xfaces=(n+1)*n*n;
    for(int q=xfaces;q<s.velocity_count;q++)assert(s.candidate_velocity[q]==0);
    for(int q=0;q<s.velocity_count;q++)assert(s.velocity[q]==0);
    size_t bytes=(size_t)s.velocity_count*sizeof(double);
    double *saved=cfd_memory_calloc((size_t)s.velocity_count,sizeof(double));assert(saved);
    memcpy(saved,s.candidate_velocity,bytes);w.strength[0]=0;
    assert(cfd_open_weather_apply(&s,&w,&work) && work==0 && !memcmp(saved,s.candidate_velocity,bytes));
    w.strength[0]=1;w.target_m_s[0]=-.5;
    for(int i=0;i<50;i++){s.steps=i;assert(cfd_open_weather_apply(&s,&w,&work));}
    assert(s.candidate_velocity[cfd_open_atmosphere3d_face(&s,0,n/4,n/2,n/2)]<0);
    /* Invalid command preserves all candidate bytes and output. */
    memcpy(saved,s.candidate_velocity,bytes);w.strength[0]=NAN;work=123;
    assert(!cfd_open_weather_apply(&s,&w,&work) && work==123 && !memcmp(saved,s.candidate_velocity,bytes));
    cfd_memory_free(saved);cfd_open_atmosphere3d_destroy(&s);
}
int main(void){trial(16);trial(32);puts("broad gust extent, spatial variation, sign, release, work and refusal passed");return 0;}
