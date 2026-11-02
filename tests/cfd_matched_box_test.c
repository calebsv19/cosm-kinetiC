#include "../src/tools/physics_sim_matched_heat.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static void init(CfdOpenAtmosphere3d *s,int n,int mc){
    int shape[3]={n,n,n},lo[3]={3*n/8,3*n/8,3*n/8},hi[3]={5*n/8,5*n/8,5*n/8};
    double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,shape,l,1.2,1.8e-5,1005,300,.026,.0005,.0025,z,t,z,true,9.81,1./300,.1));
    s->ground=true;s->muscl=true;s->scalar_mc=mc;s->cache_projection=true;
    assert(cfd_open_atmosphere3d_lateral(s));assert(cfd_open_atmosphere3d_box(s,lo,hi));
}
int main(void){
    CfdMemoryBudget budget={.limit_bytes=100000000};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    for(int rk=0;rk<2;rk++)for(int mc=0;mc<2;mc++){
        CfdOpenAtmosphere3d driver,control,fine;init(&driver,8,mc);init(&control,8,mc);init(&fine,16,mc);
        driver.scalar_rk2=control.scalar_rk2=fine.scalar_rk2=rk;
        double source[512]={0},smoke[512]={0},zero[4096]={0};source[27]=.001;fine.energy[27]=.001;
        for(int step=0;step<12;step++){
            assert(cfd_open_atmosphere3d_step(&driver,source,smoke));
            memcpy(control.candidate_velocity,driver.velocity,(size_t)driver.velocity_count*sizeof(double));
            double div,bound;assert(matched_heat_advance(&control,source,1,&bound));
            assert(!memcmp(control.energy,driver.energy,sizeof source));
            assert(matched_heat_velocity(&driver,&fine,&div));assert(div<1e-8);
            for(int a=0;a<3;a++)for(int k=0;k<8+(a==2);k++)for(int j=0;j<8+(a==1);j++)for(int i=0;i<8+(a==0);i++){
                double sum=0;for(int v=0;v<2;v++)for(int w=0;w<2;w++){
                    int p[3]={2*i,2*j,2*k};p[(a+1)%3]+=v;p[(a+2)%3]+=w;
                    sum+=fine.candidate_velocity[cfd_open_atmosphere3d_face(&fine,a,p[0],p[1],p[2])]/4;
                }
                assert(fabs(sum-driver.velocity[cfd_open_atmosphere3d_face(&driver,a,i,j,k)])<1e-15);
            }
            for(int a=0;a<3;a++)for(int k=0;k<16;k++)for(int j=0;j<16;j++)for(int i=0;i<16;i++)
                if(cfd_open_atmosphere3d_blocked(&fine,a,i,j,k))assert(fine.candidate_velocity[cfd_open_atmosphere3d_face(&fine,a,i,j,k)]==0);
            assert(matched_heat_advance(&fine,zero,1,&bound));
            for(int q=0;q<4096;q++)if(cfd_open_atmosphere3d_solid(&fine,q))assert(fine.energy[q]==0);
        }
        double div,bound;fine.solid_lo[0]++;assert(!matched_heat_velocity(&driver,&fine,&div));fine.solid_lo[0]--;
        int q=(3*8+3)*8+3;source[q]=1;
        double saved[512];memcpy(saved,control.energy,sizeof saved);
        assert(!matched_heat_advance(&control,source,1,&bound));assert(!memcmp(saved,control.energy,sizeof saved));source[q]=0;
        control.candidate_velocity[cfd_open_atmosphere3d_face(&control,0,3,3,3)]=.001;
        assert(!matched_heat_advance(&control,source,1,&bound));assert(!memcmp(saved,control.energy,sizeof saved));
        cfd_open_atmosphere3d_destroy(&driver);cfd_open_atmosphere3d_destroy(&control);cfd_open_atmosphere3d_destroy(&fine);
    }
    cfd_memory_scope(prior);assert(!budget.live_bytes && !budget.rejected_allocations);
    puts("matched box: both scalar operators replay exactly; prolonged flow respects walls/divergence; invalid geometry, wall flow and solid source refused; memory closed");
}
