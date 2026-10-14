#include "physics_sim_prescribed_flow.h"
#include <math.h>
/* Include the accepted operator once; retain its admission, scalar timing,
 * conservation, pressure solve and commit/refusal paths. No operator copy. */
static _Thread_local const double *prescribed_thermal;
static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *,double *,double *);
#define PHYSICS_SIM_OPEN_PRESCRIBED_THERMAL_QUALIFIER 1
#include "../app/cfd_open_atmosphere3d.c"

static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *s,double *div,double *res) {
    if(!prescribed_thermal)return momentum(s,div,res);
    double *accepted=s->energy;
    s->energy=(double *)prescribed_thermal; /* momentum reads this array only */
    bool ok=momentum(s,div,res);
    s->energy=accepted;
    return ok;
}

bool prescribed_flow_step(CfdOpenAtmosphere3d *s,const double *forcing,
                          const double *heat,const double *smoke) {
    if(!s || !s->storage || !forcing || prescribed_thermal || !s->ground ||
       !s->lateral_open || s->solid_box || !s->buoyancy)return false;
    double capacity=s->rho*s->cp*s->grid.volume;
    if(!isfinite(capacity) || capacity<=0)return false;
    for(int q=0;q<s->grid.count;q++)if(!isfinite(forcing[q]) || forcing[q]<0 ||
        forcing[q]/capacity/s->reference_k>s->max_contrast)return false;
    prescribed_thermal=forcing;
    bool ok=cfd_open_atmosphere3d_step(s,heat,smoke);
    prescribed_thermal=NULL;
    return ok;
}

bool prescribed_flow_prolong(const CfdOpenAtmosphere3d *c,
                             const CfdOpenAtmosphere3d *f,double *out) {
    if(!c || !f || !out || !c->storage || !f->storage || c->solid_box || f->solid_box ||
       c->rho!=f->rho || c->cp!=f->cp || c->reference_k!=f->reference_k)return false;
    for(int a=0;a<3;a++)if(f->grid.n[a]!=2*c->grid.n[a] ||
        f->grid.length[a]!=c->grid.length[a])return false;
    for(int q=0;q<c->grid.count;q++)if(!isfinite(c->energy[q]) || c->energy[q]<0)return false;
    for(int k=0;k<f->grid.n[2];k++)for(int j=0;j<f->grid.n[1];j++)for(int i=0;i<f->grid.n[0];i++)
        out[(k*f->grid.n[1]+j)*f->grid.n[0]+i]=c->energy[((k/2)*c->grid.n[1]+j/2)*c->grid.n[0]+i/2]/8;
    return true;
}
