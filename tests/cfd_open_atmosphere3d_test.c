#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void init(CfdOpenAtmosphere3d *s,int nz,double dt,double ambient,bool buoyancy,double beta,const double pressure[2]) {
    int n[3]={8,8,nz};double length[3]={2,2,2},temperatures[2]={ambient,ambient},smoke[2]={0,0};
    assert(cfd_open_atmosphere3d_init(s,n,length,1,.02,1000,300,0,0,dt,pressure,temperatures,smoke,buoyancy,9.81,beta,.1));
}
static double ledger(const CfdOpenAtmosphere3d *s,int block) {return cfd_passive3d_total(s->flux+block*2*s->plane,2*s->plane);}
int main(void) {
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    double datum[2]={0,0};CfdOpenAtmosphere3d s;init(&s,8,.01,303,false,0,datum);int n=s.grid.count;
    double *j=calloc(n,sizeof(double)),*kg=calloc(n,sizeof(double));assert(j && kg);
    double capacity=s.rho*s.cp*s.grid.volume;
    for(int q=0;q<n;q++)s.energy[q]=3*capacity;
    for(int q=0;q<n+s.plane;q++)s.velocity[2*n+q]=1;
    s.initial_j=cfd_passive3d_total(s.energy,n);
    for(int i=0;i<10;i++)assert(cfd_open_atmosphere3d_step(&s,j,kg));
    assert(fabs(cfd_passive3d_total(s.energy,n)-s.initial_j)<1e-8);
    assert(fabs(ledger(&s,0)-1200)<1e-8 && fabs(ledger(&s,1)-1200)<1e-8);
    assert(fabs(s.flux[0]-1200/64.)<1e-9);assert(s.divergence<1e-12);
    cfd_open_atmosphere3d_destroy(&s);
    /* Analytic one-step top pulse exit with zero ambient/backflow scalar. */
    init(&s,8,.01,300,false,0,datum);s.smoke[n-1]=.02;s.initial_kg=.02;
    for(int q=0;q<n+s.plane;q++)s.velocity[2*n+q]=1;
    assert(cfd_open_atmosphere3d_step(&s,j,kg));assert(fabs(s.smoke[n-1]-.0192)<1e-14);
    assert(fabs(ledger(&s,3)-.0008)<1e-14 && ledger(&s,2)==0);
    for(int i=0;i<99;i++)assert(cfd_open_atmosphere3d_step(&s,j,kg));
    assert(fabs(cfd_passive3d_total(s.smoke,n)+ledger(&s,3)-.02)<1e-14);
    cfd_open_atmosphere3d_destroy(&s);
    /* Flow reverses: top supplies its authored hot/smoky reservoir. */
    init(&s,8,.01,300,false,0,datum);s.ambient_k[1]=303;s.ambient_smoke[1]=.1;
    for(int q=0;q<n+s.plane;q++)s.velocity[2*n+q]=-1;
    assert(cfd_open_atmosphere3d_step(&s,j,kg));assert(fabs(ledger(&s,0)-120)<1e-9 && fabs(ledger(&s,2)-.004)<1e-14);
    assert(ledger(&s,1)==0 && ledger(&s,3)==0);cfd_open_atmosphere3d_destroy(&s);
    /* Hydrostatic dynamic-pressure balance of uniform buoyancy, independent exact p. */
    double beta=1./300,a=9.81*beta*3;datum[0]=-a;datum[1]=a;
    init(&s,8,.01,303,true,beta,datum);
    for(int q=0;q<n;q++){s.energy[q]=3*capacity;s.pressure[q]=a*((q/s.plane+.5)*.25-1);}
    s.initial_j=cfd_passive3d_total(s.energy,n);
    for(int i=0;i<5;i++)assert(cfd_open_atmosphere3d_step(&s,j,kg));
    for(int q=0;q<s.velocity_count;q++)assert(fabs(s.velocity[q])<1e-12);
    for(int q=0;q<n;q++)assert(fabs(s.pressure[q]-a*((q/s.plane+.5)*.25-1))<1e-12);
    /* Late contrast failure must preserve accepted flow, scalars and boundary receipts. */
    size_t accepted=(size_t)s.velocity_count+3*n;double *saved=malloc((accepted+8*s.plane)*sizeof(double));assert(saved);
    memcpy(saved,s.velocity,accepted*sizeof(double));memcpy(saved+accepted,s.flux,8*s.plane*sizeof(double));
    double time=s.time,inputs=s.input_j;int steps=s.steps;j[0]=1e9;
    assert(!cfd_open_atmosphere3d_step(&s,j,kg));
    assert(!memcmp(saved,s.velocity,accepted*sizeof(double)) && !memcmp(saved+accepted,s.flux,8*s.plane*sizeof(double)));
    assert(s.time==time && s.steps==steps && s.input_j==inputs);j[0]=0;
    size_t live=memory.live_bytes;memory.limit_bytes=live;assert(!cfd_open_atmosphere3d_step(&s,j,kg));memory.limit_bytes=128*1024*1024;
    assert(memory.live_bytes==live && !memcmp(saved,s.velocity,accepted*sizeof(double)));assert(cfd_open_atmosphere3d_step(&s,j,kg));
    cfd_open_atmosphere3d_destroy(&s);free(j);free(kg);free(saved);assert(memory.live_bytes==0);cfd_memory_scope(prior);
    puts("open reservoir uniform/pulse/backflow budgets, hydrostatic buoyancy, joint rollback and memory controls passed");return 0;
}
