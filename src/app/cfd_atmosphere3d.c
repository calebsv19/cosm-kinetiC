#include "app/cfd_atmosphere3d.h"
#include <string.h>
void cfd_atmosphere3d_destroy(CfdAtmosphere3d *s) {
    if (!s) return;
    cfd_periodic3d_destroy(&s->flow);
    cfd_passive3d_destroy(&s->scalar);
}
bool cfd_atmosphere3d_init(CfdAtmosphere3d *s, const int n[3], const double length[3],
    double rho, double mu, double dt, const double *v, double cp, double ref, double k, double d) {
    if (!s) return false;
    memset(s,0,sizeof(*s));
    if (!cfd_passive3d_init(&s->scalar,n,length,rho,cp,ref,k,d) ||
        s->scalar.grid.count>32768 || !cfd_periodic3d_init_unforced(&s->flow,n,length,rho,mu,dt,v,(size_t)3*s->scalar.grid.count)) {
        cfd_atmosphere3d_destroy(s);return false;
    }
    s->viscosity=mu;s->conductivity=k;
    return true;
}
static bool clone(CfdAtmosphere3d *out, const CfdAtmosphere3d *s) {
    const CfdPeriodic3d *f=&s->flow;const CfdPassive3d *p=&s->scalar;
    if (!cfd_atmosphere3d_init(out,f->grid.n,f->grid.length,f->rho,s->viscosity,f->dt,f->velocity,
        p->cp,p->reference_k,s->conductivity,p->tracer_diffusivity)) return false;
    memcpy(out->flow.storage,f->storage,(size_t)27*f->grid.count*sizeof(double));
    out->flow.steps=f->steps;out->flow.time=f->time;
    out->flow.previous_energy=f->previous_energy;out->flow.older_energy=f->older_energy;
    out->flow.kinetic_j=f->kinetic_j;
    memcpy(out->scalar.storage,p->storage,(size_t)6*p->grid.count*sizeof(double));
    out->scalar.time=p->time;out->scalar.steps=p->steps;out->scalar.substeps=p->substeps;
    out->scalar.work_cells=p->work_cells;out->scalar.input_j=p->input_j;out->scalar.input_kg=p->input_kg;
    out->scalar.last_balance_j=p->last_balance_j;out->scalar.last_balance_kg=p->last_balance_kg;
    return true;
}
bool cfd_atmosphere3d_step(CfdAtmosphere3d *s, const double *j, const double *kg) {
    if (!s || !s->flow.storage || !s->scalar.storage || !j || !kg ||
        s->flow.time!=s->scalar.time || s->flow.steps!=(int)s->scalar.steps) return false;
    CfdAtmosphere3d candidate={0};
    if (!clone(&candidate,s)) return false;
    if (!cfd_periodic3d_step(&candidate.flow) ||
        !cfd_passive3d_step(&candidate.scalar,candidate.flow.velocity,j,kg,candidate.flow.dt) ||
        candidate.flow.time!=candidate.scalar.time) {
        cfd_atmosphere3d_destroy(&candidate);return false;
    }
    cfd_atmosphere3d_destroy(s);
    *s=candidate; /* Move exclusive ownership after both gates; no live alias. */
    return true;
}
