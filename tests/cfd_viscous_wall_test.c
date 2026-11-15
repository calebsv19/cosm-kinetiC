/* A one-cell gap cannot supply a quadratic wall stencil. Its rejection must
 * preserve accepted state, while the original closure still admits the case. */
#include "../src/app/cfd_open_atmosphere3d.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s={0};int n[3]={8,8,8},lo[3]={2,2,1},hi[3]={6,6,5};
    double length[3]={1,1,1},pressure[2]={0},ambient[2]={300,300},zero[512]={0};
    assert(cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,0,0,
                                    .005,pressure,ambient,pressure,false,0,0,.1));
    s.ground=true;assert(cfd_open_atmosphere3d_box(&s,lo,hi));
    size_t count=(size_t)s.velocity_count;
    double *before=cfd_memory_calloc(count,sizeof(double));assert(before);
    memcpy(before,s.velocity,count*sizeof(double));
    s.viscous_wall_curvature=true;
    assert(!cfd_open_atmosphere3d_step(&s,zero,zero));
    assert(!strcmp(s.step_rejection,"curvature wall stencil"));
    assert(!memcmp(before,s.velocity,count*sizeof(double)) && s.steps==0 && s.time==0);
    s.viscous_wall_curvature=false;
    assert(cfd_open_atmosphere3d_step(&s,zero,zero));
    assert(!memcmp(before,s.velocity,count*sizeof(double)) && s.steps==1);
    cfd_memory_free(before);cfd_open_atmosphere3d_destroy(&s);
    assert(budget.live_bytes==0);cfd_memory_scope(prior);
    puts("curvature stencil refusal, rollback and original-path reuse passed");return 0;
}
