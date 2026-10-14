/* Manufactured projection impulse, not an evolving no-slip plume solution.
 * Positive cell-averaged thermal sine forcing admits independent continuum and
 * discrete mixed-boundary pressure eigenfunctions. Calls accepted momentum. */
#include "../app/cfd_open_atmosphere3d.c"
#include "app/cfd_memory.h"
#include <stdio.h>
#include <stdlib.h>
static double sinc(double x){return sin(x)/x;}
int main(int argc,char **argv){
    if(argc!=2)return 2;char *end;long side=strtol(argv[1],&end,10);
    if(*end || (side!=8 && side!=16 && side!=32 && side!=64 && side!=128))return 2;
    int n=(int)side,grid[3]={n,n,n};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;
    if(!cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1))return 3;
    s.ground=true;s.muscl=true;s.cache_projection=true;if(!cfd_open_atmosphere3d_lateral(&s))return 3;
    double pi=acos(-1),wave[3]={pi,pi,.5*pi},h=1./n,average[3],kd[3],lambda=0,K=0;
    for(int a=0;a<3;a++){average[a]=sinc(wave[a]*h/2);kd[a]=2*sin(wave[a]*h/2)/h;lambda+=kd[a]*kd[a];K+=wave[a]*wave[a];}
    double B=s.gravity*s.beta*10,coefficient=B*average[0]*average[1]*average[2]*cos(wave[2]*h/2);
    double capacity=s.rho*s.cp*s.grid.volume;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++)
        s.energy[(k*n+j)*n+i]=capacity*10*average[0]*average[1]*average[2]*sin(wave[0]*(i+.5)*h)*sin(wave[1]*(j+.5)*h)*sin(wave[2]*(k+.5)*h);
    double div,res;if(!momentum(&s,&div,&res))return 4;
    double pdelta=0,pnorm=0,vdelta=0,vnorm=0,discrete_p_max=0,discrete_v_max=0;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++){
        int q=(k*n+j)*n+i;double shape=sin(wave[0]*(i+.5)*h)*sin(wave[1]*(j+.5)*h)*cos(wave[2]*(k+.5)*h);
        double exact=-s.rho*coefficient*kd[2]/lambda*shape;
        discrete_p_max=fmax(discrete_p_max,fabs(s.candidate_pressure[q]-exact));
        double continuum=-s.rho*B*wave[2]/K*shape*average[0]*average[1]*average[2];
        pdelta+=(s.candidate_pressure[q]-continuum)*(s.candidate_pressure[q]-continuum);pnorm+=continuum*continuum;
    }
    for(int a=0;a<3;a++)for(int k=0;k<n+(a==2);k++)for(int j=0;j<n+(a==1);j++)for(int i=0;i<n+(a==0);i++){
        double x=wave[0]*(i+(a==0?0:.5))*h,y=wave[1]*(j+(a==1?0:.5))*h,z=wave[2]*(k+(a==2?0:.5))*h;
        double shape=a==0?cos(x)*sin(y)*cos(z):a==1?sin(x)*cos(y)*cos(z):sin(x)*sin(y)*sin(z);
        double exact=s.dt*coefficient*(a==2?1-kd[2]*kd[2]/lambda:kd[a]*kd[2]/lambda)*shape;
        double area=1;for(int b=0;b<3;b++)if(b!=a)area*=average[b];
        double continuum=s.dt*B*(a==2?1-wave[2]*wave[2]/K:wave[a]*wave[2]/K)*shape*area;
        int q=cfd_open_atmosphere3d_face(&s,a,i,j,k);double value=s.candidate_velocity[q];
        discrete_v_max=fmax(discrete_v_max,fabs(value-exact));vdelta+=(value-continuum)*(value-continuum);vnorm+=continuum*continuum;
    }
    double pressure_error=sqrt(pdelta/pnorm),velocity_error=sqrt(vdelta/vnorm);size_t peak=budget.peak_bytes;
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf("{\"schema\":\"buoyancy_projection_impulse_v1\",\"grid\":%d,\"flow_dt_s\":0.005,\"discrete_pressure_max_error_pa\":%.17g,\"discrete_velocity_max_error_m_s\":%.17g,\"continuum_pressure_relative_l2\":%.17g,\"continuum_face_velocity_relative_l2\":%.17g,\"maximum_divergence\":%.17g,\"pressure_residual\":%.17g,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",n,discrete_p_max,discrete_v_max,pressure_error,velocity_error,div,res,peak,budget.live_bytes,budget.rejected_allocations);
    return discrete_p_max<1e-10 && discrete_v_max<1e-10 && div<1e-8 && res<1e-11 && !budget.live_bytes && !budget.rejected_allocations ? 0:5;
}
