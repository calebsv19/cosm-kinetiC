#ifndef PHYSICS_SIM_LOCALIZED_STATE_H
#define PHYSICS_SIM_LOCALIZED_STATE_H
#include "app/cfd_open_atmosphere3d.h"
#include <stdint.h>
typedef struct {
    int n,nz,last_source_tick,restriction_tick;
    uint64_t work_cells;
    double max_lift_balance,max_restriction,minimum,maximum;
    size_t count;
    double *values;
} PhysicsSimLocalizedState;
/* Accepted physical field and retained diagnostics only; matrices, geometric
 * factors, row work and unit-response caches are reconstructed, never persisted. */
int physics_sim_localized_capture(PhysicsSimLocalizedState *out);
int physics_sim_localized_state_valid(const PhysicsSimLocalizedState *state,const CfdOpenAtmosphere3d *gas);
int physics_sim_localized_restore(const PhysicsSimLocalizedState *state,const CfdOpenAtmosphere3d *gas);
void physics_sim_localized_state_free(PhysicsSimLocalizedState *state);
#endif
