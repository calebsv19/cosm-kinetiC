/* Persistent numerical subcells for the unresolved sharp-interface startup.
 * Original initial field only. No reference input or analytic state replacement.
 * Coarse history remains 128 means; fine state is retained until diffusion's
 * RMS distance sqrt(2*alpha*t) reaches h, then conservatively restricted once. */
static double stage_transfer[256],coarse_flux[128];
static void capture_startup_face(int i,double low,double correction,double factor) {
    stage_transfer[i]=low+factor*correction;
}
#define PHYSICS_SIM_FCT_FACE_DIAGNOSTIC(i,l,a,f) capture_startup_face(i,l,a,f)
#include "physics_sim_scalar_fct.h"
#include "physics_sim_scalar_subcell.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double v[4096],a[4096],b[4096];
static struct physics_sim_subcell fine;
static const double h=.25/128;
static double minimum=1,maximum=0,balance=0,budget_closure=0;
static void observe(int step,double dt,int ratio) {
    printf("{\"step\":%d,\"time_s\":%.17g,\"profile\":[",step,step*dt);
    for(int i=0;i<128;i++){
        double mean=0;for(int j=0;j<ratio;j++)mean+=v[i*ratio+j]/ratio;
        printf("%s%.17g",i?",":"",mean);
    }
    printf("],\"integrated_flux_k_m\":[");
    for(int i=0;i<128;i++)printf("%s%.17g",i?",":"",coarse_flux[i]);puts("]}");
}
static void evolve(int n,double u,double alpha,double dx,double dt) {
    assert(physics_sim_scalar_fct(v,a,n,u,alpha,dx,dt,true));
    double first[256];for(int i=0;i<n;i++)first[i]=stage_transfer[i];
    assert(physics_sim_scalar_fct(a,b,n,u,alpha,dx,dt,true));
    double mass=0;
    for(int i=0;i<n;i++){
        coarse_flux[i]+=.5*dx*(first[i]+stage_transfer[i]);
        v[i]=.5*(v[i]+b[i]);mass+=v[i]*dx;
        minimum=fmin(minimum,v[i]);maximum=fmax(maximum,v[i]);
        if(!(v[i]>=0 && v[i]<=1+1e-12))fprintf(stderr,"startup bound violation i=%d value=%.17g\n",i,v[i]);
        assert(v[i]>=0 && v[i]<=1+1e-12);
    }
    balance=fmax(balance,fabs(mass-1./32));assert(balance<1e-12);
}
int main(int argc,char **argv) {
    if(argc!=6&&argc!=7)return 2;
    int steps=atoi(argv[1]),requested=atoi(argv[2]),end=atoi(argv[5]);
    int time_refinement=argc==7?atoi(argv[6]):1;
    double u=atof(argv[3]),alpha=atof(argv[4])/1206,dt=3.2/steps;
    if((time_refinement!=1&&time_refinement!=2)||(steps!=10240&&steps!=20480)||(requested!=1&&requested!=16&&requested!=32)||
       fabs(u)>.05||(alpha!=0&&alpha!=.026/1206)||end<1||end>steps)return 2;
    int ratio=(alpha>0?requested:1),n=128*ratio;
    double fine_h=h/ratio;
    int substeps=(int)ceil(dt*(fabs(u)/fine_h+2*alpha/(fine_h*fine_h))/.1);
    if(substeps<1)substeps=1;substeps*=time_refinement;
    int handoff=(ratio>1?(int)ceil(h*h/(2*alpha*dt)):0);
    double restriction_residual=0;
    if(ratio>1)assert(physics_sim_subcell_init(&fine,n,u,alpha,fine_h,dt/substeps));
    for(int i=0;i<n;i++)v[i]=(i>=16*ratio&&i<32*ratio)?1:0;
    observe(0,dt,ratio);
    double first_outside=0,first_cell32=0;
    for(int step=1;step<=end;step++){
        int count=ratio>1?substeps:1;
        for(int j=0;j<count;j++) {
            if(ratio>1){
                assert(physics_sim_subcell_step(&fine,v));double mass=0;
                for(int face=0;face<128;face++){
                    int right=face*ratio,left=(right+n-1)%n;
                    double adv=u*(u>=0?v[left]:v[right]);
                    double diff=-alpha*(v[right]-v[left])/(h/ratio);
                    coarse_flux[face]+=dt/count*(adv+diff);
                }
                for(int i=0;i<n;i++){
                    minimum=fmin(minimum,v[i]);maximum=fmax(maximum,v[i]);mass+=v[i]*h/ratio;
                    assert(v[i]>=0 && v[i]<=1+1e-12);
                }
                balance=fmax(balance,fabs(mass-1./32));assert(balance<1e-12);
            }else evolve(n,u,alpha,h,dt);
        }
        if(step==1){
            for(int i=0;i<n;i++)if(i<16*ratio||i>=32*ratio)first_outside+=v[i]*h/ratio;
            for(int j=0;j<ratio;j++)first_cell32+=v[32*ratio+j]/ratio;
        }
        if(ratio>1 && step==handoff){
            double before=0,after=0;
            for(int i=0;i<n;i++)before+=v[i]*h/ratio;
            for(int i=0;i<128;i++){
                double mean=0;for(int j=0;j<ratio;j++)mean+=v[i*ratio+j]/ratio;
                a[i]=mean;after+=mean*h;
            }
            for(int i=0;i<128;i++)v[i]=a[i];
            restriction_residual=fabs(after-before);assert(restriction_residual<1e-12);
            ratio=1;n=128;
        }
        if(step%(steps/128)==0){
            for(int patch=0;patch<8;patch++){
                double inventory=0;
                for(int i=patch*16;i<(patch+1)*16;i++)for(int j=0;j<ratio;j++)inventory+=v[i*ratio+j]*h/ratio;
                double initial=(patch==1?1./32:0),transfer=coarse_flux[patch*16]-coarse_flux[((patch+1)*16)%128];
                budget_closure=fmax(budget_closure,fabs(inventory-initial-transfer));
            }
            assert(budget_closure<1e-12);observe(step,dt,ratio);
        }
    }
    printf("{\"terminal\":true,\"n\":128,\"steps\":%d,\"end_step\":%d,\"startup_ratio\":%d,\"startup_cells\":%d,\"substeps\":%d,\"time_refinement\":%d,\"budget_closure_k_m\":%.17g,\"handoff_s\":%.17g,\"restriction_residual_k_m\":%.17g,\"first_outside_k_m\":%.17g,\"first_cell32_k\":%.17g,\"minimum\":%.17g,\"maximum\":%.17g,\"balance_k_m\":%.17g}\n",steps,end,requested,128*(alpha>0?requested:1),substeps,time_refinement,budget_closure,handoff*dt,restriction_residual,first_outside,first_cell32,minimum,maximum,balance);
}
