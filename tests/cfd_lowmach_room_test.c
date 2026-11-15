#include "app/cfd_lowmach_room.h"
#include "app/cfd_memory.h"
#include "../src/app/cfd_lowmach_room_internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void refusal(void) {
    CfdLmRoom s={0};assert(cfd_lm_create(&s,16,true,true,-9.81));
    double *heat=calloc((size_t)s.cells,sizeof(double));assert(heat);
    CfdLmReceipt r;memset(&r,0x5a,sizeof(r));CfdLmReceipt saved=r;
    size_t bytes=(size_t)s.cells*sizeof(double);
    double *rho=malloc(bytes);assert(rho);memcpy(rho,s.rho,bytes);
    assert(!cfd_lm_step(&s,NAN,heat,&r));
    assert(!cfd_lm_step(&s,.02,heat,&r));
    heat[0]=1;assert(!cfd_lm_step(&s,.001,heat,&r));heat[0]=0;
    for(int q=0;q<s.cells;q++)if(!s.solid[q]){heat[q]=NAN;break;}
    assert(!cfd_lm_step(&s,.001,heat,&r));
    assert(!memcmp(&r,&saved,sizeof(r))&&!memcmp(rho,s.rho,bytes)&&s.steps==0&&s.time==0);
    memset(heat,0,bytes);
    CfdMemoryBudget denied={.limit_bytes=1};CfdMemoryBudget *prior=cfd_memory_scope(&denied);
    assert(!cfd_lm_step(&s,.001,heat,&r));cfd_memory_scope(prior);
    assert(!memcmp(&r,&saved,sizeof(r))&&!memcmp(rho,s.rho,bytes)&&s.steps==0);
    s.time=.2;assert(!cfd_lm_step(&s,.001,heat,&r));s.time=0;
    s.rho[16+1]=NAN;assert(!cfd_lm_valid(&s));
    free(heat);free(rho);cfd_lm_destroy(&s);
}
static void uniform(bool vent) {
    CfdLmRoom s={0};assert(cfd_lm_create(&s,16,vent,true,0));
    double *heat=calloc((size_t)s.cells,sizeof(double));assert(heat);
    const double qdot=1e5,dt=.001;
    for(int q=0;q<s.cells;q++)if(!s.solid[q])heat[q]=qdot;
    CfdLmReceipt r;
    assert(cfd_lm_step(&s,dt,heat,&r));
    double expected_p=CFD_LM_P0+(vent?0:dt*qdot*CFD_LM_R/(CFD_LM_CP-CFD_LM_R));
    double expected_rho=CFD_LM_P0/(CFD_LM_R*CFD_LM_T0)*(vent?1-dt*qdot*CFD_LM_R/(CFD_LM_CP*CFD_LM_P0):1);
    assert(fabs(s.p0-expected_p)<1e-9);
    for(int q=0;q<s.cells;q++)if(!s.solid[q])assert(fabs(s.rho[q]-expected_rho)<1e-11);
    assert(fabs(r.mass_residual_kg)<1e-10&&fabs(r.enthalpy_residual_j)<1e-6&&r.divergence_error<1e-8);
    if(vent)assert(r.outward_mass_kg>0&&r.outward_enthalpy_j>0);else assert(r.outward_mass_kg==0);
    free(heat);cfd_lm_destroy(&s);
}
static void cold(void) {
    CfdLmRoom s={0};assert(cfd_lm_create(&s,16,true,true,-9.81));
    double *heat=calloc((size_t)s.cells,sizeof(double));assert(heat);CfdLmReceipt r;
    for(int i=0;i<10;i++)assert(cfd_lm_step(&s,.01,heat,&r));
    for(int f=0;f<s.faces;f++)assert(s.velocity[f]==0);
    for(int q=0;q<s.cells;q++)assert(s.pressure[q]==0);
    assert(r.heat_j==0&&r.outward_mass_kg==0);
    free(heat);cfd_lm_destroy(&s);
}
static void variable_projection(bool vent) {
    CfdLmRoom s={0};assert(cfd_lm_create(&s,16,vent,true,0));
    double *target=calloc((size_t)s.cells,sizeof(double)),*exact=calloc((size_t)s.cells,sizeof(double));
    double *u=calloc((size_t)s.faces,sizeof(double)),*p=calloc((size_t)s.cells,sizeof(double));
    assert(target&&exact&&u&&p);
    for(int q=0;q<s.cells;q++)if(!s.solid[q]) {
        double x=(q%s.n+.5)/s.n,z=(q/(s.n*s.n)+.5)/s.n;
        s.rho[q]=CFD_LM_P0/(CFD_LM_R*(300+400*x));
        exact[q]=x*x+.3*z*z;
    }
    /* Discrete manufactured operator: arbitrary pressure and nonuniform
     * density. Verify recovered face gradients, including half-cell vent. */
    for(int f=0;f<s.faces;f++) {
        CfdLmFace v=s.face[f];
        double inverse=v.right<0?1/s.rho[v.left]:2/(s.rho[v.left]+s.rho[v.right]);
        double flux=.001*inverse*s.n*(v.right<0?2:1)*(exact[v.left]-(v.right<0?0:exact[v.right]));
        target[v.left]+=s.n*flux;if(v.right>=0)target[v.right]-=s.n*flux;
    }
    CfdLmReceipt r={0};assert(cfd_lm_project(&s,.001,target,u,p,&r));
    for(int f=0;f<s.faces;f++) {
        CfdLmFace v=s.face[f];
        double inverse=v.right<0?1/s.rho[v.left]:2/(s.rho[v.left]+s.rho[v.right]);
        double expected=.001*inverse*s.n*(v.right<0?2:1)*(exact[v.left]-(v.right<0?0:exact[v.right]));
        assert(fabs(u[f]-expected)<1e-10);
    }
    free(target);free(exact);free(u);free(p);cfd_lm_destroy(&s);
}
int main(void) {
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};cfd_memory_scope(&budget);
    CfdLmRoom invalid={0};assert(!cfd_lm_create(&invalid,64,true,true,-9.81));
    refusal();uniform(false);uniform(true);cold();variable_projection(false);variable_projection(true);
    assert(budget.live_bytes==0);cfd_memory_scope(NULL);
    puts("lowmach room: cold, analytic heating/expansion, variable-density projection, atomic refusals and memory checks passed");return 0;
}
