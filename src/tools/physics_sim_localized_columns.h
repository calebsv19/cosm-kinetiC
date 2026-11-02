#ifndef PHYSICS_SIM_LOCALIZED_COLUMNS_H
#define PHYSICS_SIM_LOCALIZED_COLUMNS_H
#include "app/cfd_open_atmosphere3d.h"
#include "physics_sim_heated_face.h"
#include <stdint.h>
int physics_sim_localized_init(const CfdOpenAtmosphere3d *gas);
int physics_sim_localized_gas_begin(void);
int physics_sim_localized_begin(const CfdOpenAtmosphere3d *old,CfdOpenAtmosphere3d *exchanged,double dt,const PhysicsSimHeatedFace *initial_faces);
int physics_sim_localized_step(CfdOpenAtmosphere3d *gas);
void physics_sim_localized_commit(void);
void physics_sim_localized_destroy(void);
uint64_t physics_sim_localized_checksum(void);
void physics_sim_localized_fields(void);
#endif
