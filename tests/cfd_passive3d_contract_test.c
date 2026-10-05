#include "app/cfd_passive3d.h"
#include "app/cfd_periodic3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    int n[3]={8,8,8};double length[3]={2,2.5,3};
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};CfdMemoryBudget *previous=cfd_memory_scope(&budget);
    CfdPassive3d scalar;
    assert(cfd_passive3d_init(&scalar,n,length,1,1000,300,.1,.001));
    int count=scalar.grid.count;
    double *v=calloc((size_t)3*count,sizeof(double)),*j=calloc(count,sizeof(double)),*kg=calloc(count,sizeof(double));
    double *saved=calloc((size_t)2*count,sizeof(double));assert(v && j && kg && saved);
    j[0]=6000;kg[0]=.0008;
    assert(cfd_passive3d_step(&scalar,v,j,kg,.05));
    assert(fabs(cfd_passive3d_total(scalar.energy,count)-6000)<1e-9);
    assert(fabs(cfd_passive3d_total(scalar.smoke,count)-.0008)<1e-15);
    memcpy(saved,scalar.energy,(size_t)2*count*sizeof(double));double time=scalar.time;
    unsigned steps=scalar.steps;double input=scalar.input_j;
    assert(!cfd_passive3d_step(&scalar,v,scalar.energy,kg,.05));
    scalar.work_cells=100000000;assert(!cfd_passive3d_step(&scalar,v,j,kg,.05));
    scalar.work_cells=count;
    v[0]=NAN;assert(!cfd_passive3d_step(&scalar,v,j,kg,.05));v[0]=0;
    kg[1]=-1;assert(!cfd_passive3d_step(&scalar,v,j,kg,.05));kg[1]=0;
    v[0]=1;assert(!cfd_passive3d_step(&scalar,v,j,kg,.05));v[0]=0;
    for (int q=0;q<count;q++) v[q]=1e9;
    assert(!cfd_passive3d_step(&scalar,v,j,kg,.05));
    assert(!memcmp(saved,scalar.energy,(size_t)2*count*sizeof(double)));
    assert(time==scalar.time && steps==scalar.steps && input==scalar.input_j);
    memset(j,0,(size_t)count*sizeof(double));memset(kg,0,(size_t)count*sizeof(double));
    cfd_passive3d_destroy(&scalar);
    assert(cfd_passive3d_init(&scalar,n,length,1,1000,300,.1,.001));
    scalar.energy[0]=6000;scalar.smoke[0]=.0008;
    for (int mode=0;mode<2;mode++) {
        if (mode) {
            cfd_passive3d_destroy(&scalar);
            assert(cfd_passive3d_init(&scalar,n,length,1,1000,300,.1,.001));
            scalar.energy[0]=6000;scalar.smoke[0]=.0008;
        }
        CfdPeriodic3d a,b;
        if (mode) {
            for (int axis=0;axis<3;axis++) for (int q=0;q<count;q++) {
                double xyz[3], exact[3];
                cfd_cartesian3d_position(&scalar.grid,q,axis,xyz);
                cfd_periodic3d_exact(length,xyz,0,1,.1,exact,NULL,NULL);
                v[axis*count+q]=exact[axis];
            }
            assert(cfd_periodic3d_init_unforced(&a,n,length,1,.1,.001,v,(size_t)3*count));
            assert(cfd_periodic3d_init_unforced(&b,n,length,1,.1,.001,v,(size_t)3*count));
        } else {
            assert(cfd_periodic3d_init(&a,n,length,1,.1,.001));
            assert(cfd_periodic3d_init(&b,n,length,1,.1,.001));
        }
        for (int step=0;step<8;step++) {
            assert(cfd_periodic3d_step(&a));assert(cfd_periodic3d_step(&b));
            memcpy(v,a.velocity,(size_t)3*count*sizeof(double));
            size_t allocations=budget.successful_allocations;
            assert(cfd_passive3d_step(&scalar,a.velocity,j,kg,.001));
            assert(allocations==budget.successful_allocations);
            assert(!memcmp(v,a.velocity,(size_t)3*count*sizeof(double)));
            assert(!memcmp(a.velocity,b.velocity,(size_t)3*count*sizeof(double)));
            assert(!memcmp(a.previous,b.previous,(size_t)3*count*sizeof(double)));
            assert(!memcmp(a.pressure,b.pressure,(size_t)count*sizeof(double)));
            assert(a.time==b.time && a.steps==b.steps && a.true_residual==b.true_residual);
            assert(scalar.time==a.time);
        }
        assert(fabs(cfd_passive3d_total(scalar.energy,count)-6000)<1e-8);
        cfd_periodic3d_destroy(&a);cfd_periodic3d_destroy(&b);
    }
    cfd_passive3d_destroy(&scalar);
    free(v);free(j);free(kg);free(saved);assert(budget.live_bytes==0);
    CfdMemoryBudget denied={.limit_bytes=1};cfd_memory_scope(&denied);
    assert(!cfd_passive3d_init(&scalar,n,length,1,1000,300,0,0));
    cfd_passive3d_destroy(&scalar);assert(denied.live_bytes==0);
    cfd_memory_scope(previous);puts("passive conservation, rejection rollback, memory and momentum parity passed");
    return 0;
}
