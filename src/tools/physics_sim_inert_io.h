#ifndef PHYSICS_SIM_INERT_IO_H
#define PHYSICS_SIM_INERT_IO_H
#include "physics_sim_inert_coupling.h"
#include <json-c/json.h>
typedef struct {
    bool enabled;
    double depth_m,initial_k;
    PhysicsSimHeatedFace face;
} PhysicsSimInertIo;
/* Optional object belongs to the restored gas checkpoint and its exact clock. */
bool physics_inert_load(PhysicsSimInertIo *object,CfdOpenAtmosphere3d *gas,
    struct json_object *request);
bool physics_inert_step(PhysicsSimInertIo *object,CfdOpenAtmosphere3d *gas,
    const double *energy,const double *smoke);
void physics_inert_fields(const PhysicsSimInertIo *object,
    const CfdOpenAtmosphere3d *gas,struct json_object *fields,struct json_object *state);
#endif
