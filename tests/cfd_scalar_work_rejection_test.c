/* The failed128 packet is too expensive for ordinary CI. Reduce its budget
 * exhaustion to a stationary8^3 object-coupled control with exact rollback. */
#include "../src/tools/physics_sim_inert_coupling.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    CfdOpenAtmosphere3d s;int n[3]={8,8,8},lo[3]={3,3,3},hi[3]={4,4,4};
    double l[3]={1,1,1},p[2]={0,0},t[2]={300,300},zero[512]={0};
    assert(cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,.005,p,t,p,false,9.81,1./300,.1));
    s.ground=true;assert(cfd_open_atmosphere3d_lateral(&s));
    assert(cfd_open_atmosphere3d_box(&s,lo,hi));s.scalar_mc=true;s.scalar_rk2=true;
    PhysicsSimHeatedFace face={0,-1,300,10,2,0,0},original=face;
    size_t count=(size_t)2*s.velocity_count+9*(size_t)s.grid.count+8*(size_t)s.boundary_faces;
    double *saved=malloc(count*sizeof(double));assert(saved);
    memcpy(saved,s.storage,count*sizeof(double));
    s.max_scalar_work_cells=1023;
    assert(!physics_sim_inert_coupling_step(&s,&face,zero,zero,.125,false));
    assert(!strcmp(s.step_rejection,"scalar cumulative work budget"));
    assert(s.scalar_work_requested==1024 && s.scalar_work_cells==0);
    assert(s.steps==0 && s.time==0 && s.input_j==0 && s.input_kg==0);
    assert(!memcmp(saved,s.storage,count*sizeof(double)) && !memcmp(&face,&original,sizeof face));
    s.max_scalar_work_cells=1024;
    assert(s.max_scalar_cycles==4096);s.max_scalar_cycles=0;
    assert(!physics_sim_inert_coupling_step(&s,&face,zero,zero,.125,false));
    assert(!strcmp(s.step_rejection,"selected scalar cycle capacity"));
    assert(s.scalar_cycles_requested==1 && s.scalar_work_requested==0 && s.steps==0);
    assert(!memcmp(saved,s.storage,count*sizeof(double)) && !memcmp(&face,&original,sizeof face));
    s.max_scalar_cycles=4;
    assert(physics_sim_inert_coupling_step(&s,&face,zero,zero,.125,false));
    assert(s.steps==1 && s.time==.005 && s.scalar_work_cells==1024 && !s.step_rejection);
    memcpy(saved,s.storage,count*sizeof(double));original=face;
    assert(!physics_sim_inert_coupling_step(&s,&face,zero,zero,.125,false));
    assert(!strcmp(s.step_rejection,"scalar cumulative work budget"));
    assert(s.scalar_work_requested==1024 && s.scalar_work_cells==1024 && s.steps==1 && s.time==.005);
    assert(!memcmp(saved,s.storage,count*sizeof(double)) && !memcmp(&face,&original,sizeof face));
    s.max_scalar_work_cells=2048;
    assert(physics_sim_inert_coupling_step(&s,&face,zero,zero,.125,false));
    assert(s.steps==2 && s.time==.01 && s.scalar_work_cells==2048 && !s.step_rejection);
    free(saved);cfd_open_atmosphere3d_destroy(&s);assert(!memory.live_bytes && !memory.rejected_allocations);
    cfd_memory_scope(prior);puts("scalar work refusal reason, exact exhaustion, rollback and successful continuation passed");return 0;
}
