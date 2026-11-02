/* Fixed-source spatial/time diagnostic using the ordinary native operators.
 * Binary inputs/outputs are local X-fast IEEE double arrays, never a journal.
 * No solver tolerances or production capacities are changed by this probe. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_open_momentum_diagnostics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
CfdOpenMomentumDiagnostics cfd_open_momentum_diagnostics;
void cfd_open_momentum_diagnostics_begin(void) {
    memset(&cfd_open_momentum_diagnostics,0,sizeof(cfd_open_momentum_diagnostics));
}
static void setup(CfdOpenAtmosphere3d *s,int n,double dt,int box,int muscl) {
    int grid[3]={n,n,n};double length[3]={1,1,1},p[2]={0},t[2]={300,300},smoke[2]={0};
    assert(cfd_open_atmosphere3d_init(s,grid,length,1.2,1.8e-5,1005,300,.026,.0005,dt,p,t,smoke,true,9.81,1./300,.1));
    s->ground=true;s->cache_projection=true;s->muscl=muscl;
    s->max_scalar_work_cells=1000000000;
    assert(cfd_open_atmosphere3d_lateral(s));
    if(box) { int lo[3]={3*n/8,3*n/8,3*n/8},hi[3]={5*n/8,5*n/8,5*n/8};assert(cfd_open_atmosphere3d_box(s,lo,hi)); }
    int ticks[3]={0,(int)llround(.4/dt),(int)llround(1.2/dt)};double speeds[3]={0,.5,0};
    assert(cfd_open_wind_configure(s,0,.25,false,3,ticks,speeds));
}
static void write_values(FILE *f,const double *p,size_t n) { assert(fwrite(p,sizeof(double),n,f)==n); }
static void read_values(FILE *f,double *p,size_t n) { assert(fread(p,sizeof(double),n,f)==n); }
/* State is round-tripped through a file into a fresh native solver. Scratch,
 * operator cache, pointers and diagnostics are deliberately excluded. */
static void checkpoint(CfdOpenAtmosphere3d *s,FILE *f,int load) {
    double counters[10]={s->time,s->initial_j,s->initial_kg,s->input_j,s->input_kg,s->wind_air_in,s->wind_air_out,s->wind_inlet_volume,(double)s->steps,(double)s->scalar_work_cells};
    if(load) { read_values(f,counters,10);s->time=counters[0];s->initial_j=counters[1];s->initial_kg=counters[2];s->input_j=counters[3];s->input_kg=counters[4];s->wind_air_in=counters[5];s->wind_air_out=counters[6];s->wind_inlet_volume=counters[7];s->steps=(int)counters[8];s->scalar_work_cells=(unsigned long long)counters[9]; }
    else write_values(f,counters,10);
    double *parts[5]={s->energy,s->smoke,s->velocity,s->pressure,s->flux};size_t counts[5]={(size_t)s->grid.count,(size_t)s->grid.count,(size_t)s->velocity_count,(size_t)s->grid.count,(size_t)4*s->boundary_faces};
    for(int k=0;k<5;k++) { if(load)read_values(f,parts[k],counts[k]);else write_values(f,parts[k],counts[k]); }
}
static void equal_state(const CfdOpenAtmosphere3d *a,const CfdOpenAtmosphere3d *b) {
    assert(a->steps==b->steps && a->time==b->time && a->scalar_work_cells==b->scalar_work_cells);
    assert(a->input_j==b->input_j && a->input_kg==b->input_kg && a->wind_air_in==b->wind_air_in && a->wind_air_out==b->wind_air_out && a->wind_inlet_volume==b->wind_inlet_volume);
    size_t n=(size_t)a->grid.count;assert(!memcmp(a->energy,b->energy,2*n*sizeof(double)));
    assert(!memcmp(a->velocity,b->velocity,(size_t)a->velocity_count*sizeof(double)));
    assert(!memcmp(a->pressure,b->pressure,n*sizeof(double)));assert(!memcmp(a->flux,b->flux,(size_t)4*a->boundary_faces*sizeof(double)));
}
static double total(const double *a,int n) { double value=0;for(int k=0;k<n;k++)value+=a[k];return value; }
static void dump(const CfdOpenAtmosphere3d *s,const char *root,int window) {
    char path[4096];assert(snprintf(path,sizeof(path),"%s/fields-%02d.bin",root,window)>0);FILE *f=fopen(path,"wbx");assert(f);
    write_values(f,s->energy,(size_t)s->grid.count);write_values(f,s->smoke,(size_t)s->grid.count);write_values(f,s->velocity,(size_t)s->velocity_count);assert(!fclose(f));
}
int main(int argc,char **argv) {
    assert(argc==8);int n=atoi(argv[1]),half=atoi(argv[2]),box=atoi(argv[3]),muscl=atoi(argv[4]);
    assert((n==8 || n==16 || n==32 || n==64) && (half==0 || half==1) && (box==0 || box==1) && (muscl==0 || muscl==1));
    double dt=half?.0025:.005;int per=half?40:20;const char *root=argv[6];int windows=atoi(argv[7]);assert(windows>=1 && windows<=32);
    CfdMemoryBudget memory={.limit_bytes=2147483648ULL};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    CfdOpenAtmosphere3d s={0},restart={0};setup(&s,n,dt,box,muscl);int cells=s.grid.count;
    double *input=calloc((size_t)4*cells,sizeof(double));assert(input);double *e=input,*m=e+cells,*se=m+cells,*sm=se+cells;
    FILE *source=fopen(argv[5],"rb");assert(source);char path[4096];snprintf(path,sizeof(path),"%s/trajectory.csv",root);FILE *csv=fopen(path,"wx");assert(csv);
    fprintf(csv,"window,time_s,energy_j,smoke_kg,inlet_m3,air_in_m3,air_out_m3,max_divergence,max_residual,max_courant,pressure_iterations\n");
    dump(&s,root,0);double all_div=0,all_res=0,all_cfl=0;int restarted=0;
    for(int w=0;w<windows;w++) {
        read_values(source,e,(size_t)cells);read_values(source,m,(size_t)cells);
        for(int q=0;q<cells;q++) { assert(isfinite(e[q]) && e[q]>=0 && isfinite(m[q]) && m[q]>=0);se[q]=e[q]/per;sm[q]=m[q]/per; }
        double maxdiv=0,maxres=0,maxcfl=0;int maxit=0;
        for(int k=0;k<per;k++) {
            if(!cfd_open_atmosphere3d_step(&s,se,sm)) {
                fprintf(stderr,"REFUSED grid=%d step=%d time=%.17g stage=%s check=%s linear=%s courant=%.17g limit=%.17g residual=%.17g true=%.17g recursive=%.17g div=%.17g cell=%d\n",n,s.steps,s.time,s.step_rejection,cfd_open_momentum_diagnostics.check?cfd_open_momentum_diagnostics.check:"none",cfd_open_momentum_diagnostics.linear_failure?cfd_open_momentum_diagnostics.linear_failure:"none",cfd_open_momentum_diagnostics.courant,cfd_open_momentum_diagnostics.courant_limit,cfd_open_momentum_diagnostics.pressure_residual,cfd_open_momentum_diagnostics.cg_true_residual,cfd_open_momentum_diagnostics.cg_recursive_residual,cfd_open_momentum_diagnostics.maximum_divergence,cfd_open_momentum_diagnostics.divergence_cell);return 2;
            }
            maxdiv=fmax(maxdiv,s.divergence);maxres=fmax(maxres,s.residual);maxcfl=fmax(maxcfl,cfd_open_momentum_diagnostics.courant);if(cfd_open_momentum_diagnostics.pressure_iterations>maxit)maxit=cfd_open_momentum_diagnostics.pressure_iterations;
            if(w==12 && restarted) { assert(cfd_open_atmosphere3d_step(&restart,se,sm));equal_state(&s,&restart); }
        }
        assert(cfd_open_atmosphere3d_valid(&s));assert(s.divergence<1e-8 && s.residual<1e-11);
        if(w==11) {
            snprintf(path,sizeof(path),"%s/restart.bin",root);FILE *f=fopen(path,"wbx");assert(f);checkpoint(&s,f,0);assert(!fclose(f));setup(&restart,n,dt,box,muscl);f=fopen(path,"rb");assert(f);checkpoint(&restart,f,1);assert(!fclose(f));assert(cfd_open_atmosphere3d_valid(&restart));equal_state(&s,&restart);restarted=1;
        }
        if(w==12 && restarted)cfd_open_atmosphere3d_destroy(&restart);
        all_div=fmax(all_div,maxdiv);all_res=fmax(all_res,maxres);all_cfl=fmax(all_cfl,maxcfl);
        fprintf(csv,"%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d\n",w+1,s.time,total(s.energy,cells),total(s.smoke,cells),s.wind_inlet_volume,s.wind_air_in,s.wind_air_out,maxdiv,maxres,maxcfl,maxit);fflush(csv);dump(&s,root,w+1);fprintf(stderr,"accepted grid=%d window=%d time=%.3f\n",n,w+1,s.time);fflush(stderr);
    }
    assert(fgetc(source)==EOF);fclose(source);fclose(csv);if(restart.storage)cfd_open_atmosphere3d_destroy(&restart);cfd_open_atmosphere3d_destroy(&s);free(input);
    snprintf(path,sizeof(path),"%s/native-summary.json",root);FILE *f=fopen(path,"wx");assert(f);fprintf(f,"{\"status\":\"passed\",\"grid\":%d,\"dt_s\":%.17g,\"windows\":%d,\"box\":%d,\"muscl\":%d,\"max_divergence\":%.17g,\"max_residual\":%.17g,\"max_courant\":%.17g,\"restart_exact\":%s,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",n,dt,windows,box,muscl,all_div,all_res,all_cfl,restarted?"true":"false",memory.peak_bytes,memory.live_bytes,memory.rejected_allocations);fclose(f);assert(memory.live_bytes==0 && memory.rejected_allocations==0);cfd_memory_scope(prior);return 0;
}
