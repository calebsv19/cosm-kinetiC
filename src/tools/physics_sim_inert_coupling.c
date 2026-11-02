#include "physics_sim_inert_coupling.h"
#include <string.h>

bool physics_sim_inert_coupling_step(CfdOpenAtmosphere3d *s,
    PhysicsSimHeatedFace *face,const double *source_j,const double *source_kg,
    double depth,bool fail_after_flow) {
    double rise;
    if(!s || !face || !source_j || !source_kg ||
       !physics_sim_heated_face_sample(s,face,depth,&rise))return false;
    size_t count=(size_t)2*s->velocity_count+9*(size_t)s->grid.count+8*(size_t)s->boundary_faces;
    double *saved=cfd_memory_calloc(count,sizeof(double));if(!saved)return false;
    memcpy(saved,s->storage,count*sizeof(double));
    CfdOpenAtmosphere3d accepted=*s;PhysicsSimHeatedFace prior=*face;
    bool ok=cfd_open_atmosphere3d_step(s,source_j,source_kg) && !fail_after_flow &&
        physics_sim_heated_face_exchange_depth(s,face,s->dt,depth);
    if(!ok) {
        const char *rejection=s->step_rejection;
        unsigned long long requested=s->scalar_work_requested;
        unsigned cycles=s->scalar_cycles_requested;double courant=s->momentum_courant;
        double residual=s->attempted_projection_residual,divergence=s->attempted_projection_divergence;
        if(!rejection)rejection=fail_after_flow?"injected failure after flow":"object heat exchange";
        /* Pressure caches are disposable and may have been constructed during
         * preparation. Keep their actual ownership; restore all physical state,
         * clocks, receipts and backing arrays. Geometry is immutable here. */
        CfdCartesian3dLinear *projection=s->projection;bool ground=s->projection_ground;
        CfdSparseMg *solid=s->solid_projection;int *row=s->solid_row,*col=s->solid_col;
        double *values=s->solid_values;
        *s=accepted;s->projection=projection;s->projection_ground=ground;
        s->solid_projection=solid;s->solid_row=row;s->solid_col=col;s->solid_values=values;
        memcpy(s->storage,saved,count*sizeof(double));*face=prior;
        /* Diagnostics describe the refused attempt; accepted physical state
         * and accounting above remain transactional. */
        s->step_rejection=rejection;s->scalar_work_requested=requested;
        s->scalar_cycles_requested=cycles;s->momentum_courant=courant;
        s->attempted_projection_residual=residual;s->attempted_projection_divergence=divergence;
    }
    cfd_memory_free(saved);return ok;
}
