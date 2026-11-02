/* Same scalar operator and divergence-preserving prescribed flow, with an
 * aligned insulated box. The coarse native solution is an exact replay control.
 * Fine momentum and reciprocal thermal feedback are deliberately not solved. */
#include "physics_sim_matched_heat.h"
#include "app/cfd_passive3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
static volatile sig_atomic_t stopped;
static void stop(int signum){(void)signum;stopped=1;}
static int init(CfdOpenAtmosphere3d *s,int side,int mc){
    int n[3]={side,side,side},lo[3]={3*side/8,3*side/8,3*side/8},hi[3]={5*side/8,5*side/8,5*side/8};
    double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};
    if(!cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.0025,z,t,z,true,9.81,1./300,.1))return 0;
    s->ground=true;s->muscl=true;s->scalar_mc=mc;s->cache_projection=true;s->max_scalar_work_cells=1000000000;
    return cfd_open_atmosphere3d_lateral(s) && cfd_open_atmosphere3d_box(s,lo,hi);
}
static void observe(const CfdOpenAtmosphere3d *s){
    int n=s->grid.n[0],r=n/32;double bands[32]={0},column[32]={0},patch[64]={0};
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++){
        double e=s->energy[(k*n+j)*n+i];bands[k/r]+=e;
        if(i>=12*r && i<20*r && j>=12*r && j<20*r){
            column[k/r]+=e;
            if(k>=11*r && k<12*r)patch[(j/r-12)*8+i/r-12]+=e/(1206./32768);
        }
    }
    printf("{\"grid\":%d,\"time_s\":%.17g,\"bands_j\":[",n,s->time);
    for(int b=0;b<32;b++)printf("%s%.17g",b?",":"",bands[b]);
    printf("],\"column_j\":[");for(int b=0;b<32;b++)printf("%s%.17g",b?",":"",column[b]);
    printf("],\"patch_rise_k\":[");for(int b=0;b<64;b++)printf("%s%.17g",b?",":"",patch[b]);
    printf("]}\n");fflush(stdout);
}
static double balance(CfdOpenAtmosphere3d *s){return cfd_passive3d_total(s->energy,s->grid.count)-s->input_j-cfd_passive3d_total(s->flux,s->boundary_faces)+cfd_passive3d_total(s->flux+s->boundary_faces,s->boundary_faces);}
int main(int argc,char **argv){
    if(argc!=5 || (strcmp(argv[4],"mc") && strcmp(argv[4],"minmod")))return 2;
    if(signal(SIGTERM,stop)==SIG_ERR || signal(SIGINT,stop)==SIG_ERR)return 2;
    CfdMemoryBudget budget={.limit_bytes=536870912};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d driver,control,fine;int mc=!strcmp(argv[4],"mc");
    if(!init(&driver,32,mc) || !init(&control,32,mc) || !init(&fine,64,mc))return 3;
    int nc=driver.grid.count,nf=fine.grid.count;
    double *sc=cfd_memory_calloc((size_t)2*nc,sizeof(double)),*sf=cfd_memory_calloc((size_t)2*nf,sizeof(double));
    FILE *fc=fopen(argv[1],"rb"),*ff=fopen(argv[2],"rb");if(!sc || !sf || !fc || !ff)return 4;
    double maxdiv=0,maxbound=0,maxsource=0;observe(&control);observe(&fine);
    for(int frame=0;frame<32 && !stopped;frame++){
        if(fread(sc,sizeof(double),(size_t)2*nc,fc)!=(size_t)2*nc || fread(sf,sizeof(double),(size_t)2*nf,ff)!=(size_t)2*nf)return 4;
        for(int q=0;q<2*nc;q++)sc[q]/=40;
        for(int q=0;q<2*nf;q++)sf[q]/=40;
        for(int k=0;k<32;k++)for(int j=0;j<32;j++)for(int i=0;i<32;i++){
            double sum=0;for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++)sum+=sf[((2*k+z)*64+2*j+y)*64+2*i+x];
            double error=fabs(sum-sc[(k*32+j)*32+i]);maxsource=fmax(maxsource,error);if(error>1e-14)return 5;
        }
        for(int tick=0;tick<40 && !stopped;tick++){
            double div,bound;
            if(!cfd_open_atmosphere3d_step(&driver,sc,sc+nc)){fprintf(stderr,"driver: %s\n",driver.step_rejection);return 5;}
            memcpy(control.candidate_velocity,driver.velocity,(size_t)driver.velocity_count*sizeof(double));
            if(!matched_heat_advance(&control,sc,2,&bound) || memcmp(control.energy,driver.energy,(size_t)nc*sizeof(double))){fputs("coarse replay mismatch\n",stderr);return 5;}
            if(!matched_heat_velocity(&driver,&fine,&div) || !matched_heat_advance(&fine,sf,2,&bound)){fputs("fine replay refused\n",stderr);return 5;}
            maxdiv=fmax(maxdiv,div);maxbound=fmax(maxbound,bound);
            if(driver.steps%2==0){observe(&control);observe(&fine);}
        }
        fprintf(stderr,"window %d time %.4f\n",frame+1,driver.time);fflush(stderr);
    }
    int complete=driver.steps==1280;
    if(complete && (fgetc(fc)!=EOF || fgetc(ff)!=EOF))return 4;
    fclose(fc);fclose(ff);FILE *out=fopen(argv[3],"wb");if(!out)return 4;
    if(fwrite(control.energy,sizeof(double),(size_t)nc,out)!=(size_t)nc || fwrite(fine.energy,sizeof(double),(size_t)nf,out)!=(size_t)nf || fclose(out))return 4;
    printf("{\"terminal\":true,\"complete\":%s,\"operator\":\"%s\",\"time_s\":%.17g,\"steps\":%d,\"control_bitwise_equal\":true,\"max_fine_divergence\":%.17g,\"max_fine_courant\":%.17g,\"max_source_restriction_error_j\":%.17g,\"coarse_balance_j\":%.17g,\"fine_balance_j\":%.17g,\"memory_peak_bytes\":%zu",complete?"true":"false",argv[4],driver.time,driver.steps,maxdiv,maxbound,maxsource,balance(&control),balance(&fine),budget.peak_bytes);
    cfd_memory_free(sc);cfd_memory_free(sf);cfd_open_atmosphere3d_destroy(&driver);cfd_open_atmosphere3d_destroy(&control);cfd_open_atmosphere3d_destroy(&fine);cfd_memory_scope(prior);
    printf(",\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",budget.live_bytes,budget.rejected_allocations);
    return budget.live_bytes || budget.rejected_allocations?6:complete?0:7;
}
