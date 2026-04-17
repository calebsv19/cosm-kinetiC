#ifndef SIM_RUNTIME_3D_FOOTPRINT_H
#define SIM_RUNTIME_3D_FOOTPRINT_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "app/sim_runtime_3d_domain.h"

typedef struct SimRuntime3DFootprintHalfExtents {
    int half_x_cells;
    int half_y_cells;
    int half_z_cells;
} SimRuntime3DFootprintHalfExtents;

int sim_runtime_3d_footprint_emitter_radius_cells(const SimRuntime3DDomainDesc *desc,
                                                  float emitter_radius);
int sim_runtime_3d_footprint_object_sphere_radius_cells(const SimRuntime3DDomainDesc *desc,
                                                        const PresetObject *object);
bool sim_runtime_3d_footprint_object_box_half_extents_cells(
    const SimRuntime3DDomainDesc *desc,
    const PresetObject *object,
    SimRuntime3DFootprintHalfExtents *out_half_extents);
bool sim_runtime_3d_footprint_import_box_half_extents_cells(
    const SimRuntime3DDomainDesc *desc,
    const ImportedShape *imp,
    SimRuntime3DFootprintHalfExtents *out_half_extents);

#endif // SIM_RUNTIME_3D_FOOTPRINT_H
