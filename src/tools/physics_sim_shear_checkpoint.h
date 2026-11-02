#ifndef PHYSICS_SIM_SHEAR_CHECKPOINT_H
#define PHYSICS_SIM_SHEAR_CHECKPOINT_H
#include "physics_sim_localized_state.h"
#include "receiver.h"
#define PHYSICS_SIM_SHEAR_CHECKPOINT_PROFILE "shear-spatial-checkpoint-v1"
typedef struct {
    CfdOpenAtmosphere3d *gas;
    ThermalReceiver *receivers;
    double *replay;
    unsigned refusals,tested;
    int pending_end;
    uint64_t operation,last_operation;
} PhysicsSimShearView;
typedef enum {SHEAR_CP_OK=0,SHEAR_CP_REFUSED=1,SHEAR_CP_IO=2,SHEAR_CP_RELOAD=3} PhysicsSimShearCheckpointStatus;
/* engine is a caller-verified SHA256 binding receiver binary and its manifest.
 * digest0 is absent destination; otherwise compare-and-swap exact file digest.
 * pending_end0 means committed. Nonzero target binds a single recoverable range. */
PhysicsSimShearCheckpointStatus physics_sim_shear_save(const char *path,PhysicsSimShearView *view,
    const char *engine,uint64_t expected,int pending_end,uint64_t last_operation,int fail,uint64_t *digest);
int physics_sim_shear_load(const char *path,PhysicsSimShearView *view,const char *engine,uint64_t *digest);
#endif
