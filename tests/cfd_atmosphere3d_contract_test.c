#include "app/cfd_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    int n[3]={8,8,8};double length[3]={2,2,2};int count=512;
    double *v=calloc(3*count,sizeof(double)),*j=calloc(count,sizeof(double)),*kg=calloc(count,sizeof(double));
    assert(v && j && kg);
    for(int q=0;q<count;q++){int y=(q/8)%8;v[q]=.2+.05*cos(2*3.141592653589793*(y+.5)/8);v[count+q]=.05;v[2*count+q]=.1;}
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdAtmosphere3d a;CfdPeriodic3d reference;
    assert(cfd_atmosphere3d_init(&a,n,length,1.225,.0245,.01,v,1000,300,.1,.001));
    assert(cfd_periodic3d_init_unforced(&reference,n,length,1.225,.0245,.01,v,3*count));
    j[0]=100;kg[0]=.01;
    for(int step=0;step<5;step++) {
        assert(cfd_periodic3d_step(&reference));assert(cfd_atmosphere3d_step(&a,j,kg));
        for(int q=0;q<3*count;q++)assert(fabs(a.flow.velocity[q]-reference.velocity[q])<1e-11);
        assert(a.flow.time==a.scalar.time && a.flow.steps==(int)a.scalar.steps);
        assert(fabs(cfd_passive3d_total(a.scalar.energy,count)-(step+1)*100)<1e-9);
    }
    assert(memcmp(a.flow.velocity,v,3*count*sizeof(double)));
    double *saved=malloc(33*count*sizeof(double));assert(saved);
    memcpy(saved,a.flow.storage,27*count*sizeof(double));memcpy(saved+27*count,a.scalar.storage,6*count*sizeof(double));
    double time=a.flow.time,input=a.scalar.input_j;int steps=a.flow.steps;size_t live=budget.live_bytes;
    kg[0]=-1;assert(!cfd_atmosphere3d_step(&a,j,kg));kg[0]=.01;
    assert(!memcmp(saved,a.flow.storage,27*count*sizeof(double)) && !memcmp(saved+27*count,a.scalar.storage,6*count*sizeof(double)));
    assert(a.flow.time==time && a.scalar.input_j==input && a.flow.steps==steps && !a.flow.failed && budget.live_bytes==live);
    budget.limit_bytes=live;assert(!cfd_atmosphere3d_step(&a,j,kg));budget.limit_bytes=128*1024*1024;
    assert(!memcmp(saved,a.flow.storage,27*count*sizeof(double)) && budget.live_bytes==live);
    assert(cfd_atmosphere3d_step(&a,j,kg));
    cfd_atmosphere3d_destroy(&a);cfd_periodic3d_destroy(&reference);assert(budget.live_bytes==0);cfd_memory_scope(prior);
    free(v);free(j);free(kg);free(saved);puts("evolving momentum parity, scalar conservation, joint rollback, retry and owned-memory controls passed");return 0;
}
