/* Explicit diagnostic build: retain a prescribed, divergence-free MAC field.
 * Reuse the native scalar operator, object exchange, CFL/work/positivity/budget
 * checks and atomic commit. This is not a momentum or pressure solution. The
 * ordinary worker never links this translation unit.
 */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <string.h>
static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *,double *,double *);
#define PHYSICS_SIM_OPEN_PRESCRIBED_THERMAL_QUALIFIER 1
#include "../app/cfd_open_atmosphere3d.c"

static bool physics_sim_prescribed_momentum(CfdOpenAtmosphere3d *s,double *div,double *res) {
    /* Keep the ordinary operator compiled for source parity; never fall back
     * silently from this diagnostic's deliberately narrow admission. */
    (void)momentum;
    if(!s->ground || !s->lateral_open || !s->solid_box || s->buoyancy ||
       !s->wind.quiescent_backflow || s->weather.enabled)return false;
    double maximum=0;
    for(int q=0;q<s->grid.count;q++)if(!cfd_open_atmosphere3d_solid(s,q))
        maximum=fmax(maximum,fabs(divergence(s,s->velocity,q)));
    if(!isfinite(maximum) || maximum>1e-8)return false;
    memcpy(s->candidate_velocity,s->velocity,(size_t)s->velocity_count*sizeof(double));
    memcpy(s->candidate_pressure,s->pressure,(size_t)s->grid.count*sizeof(double));
    memcpy(s->candidate_kinetic_ledger,s->kinetic_ledger,sizeof(s->kinetic_ledger));
    if(s->steps==0)s->candidate_kinetic_ledger[KE_INITIAL]=cfd_open_kinetic(s,s->velocity);
    *div=maximum;*res=0;
    return true;
}
