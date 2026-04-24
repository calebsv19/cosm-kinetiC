#ifndef SIM_RUNTIME_BACKEND_3D_ORIENTED_BOX_H
#define SIM_RUNTIME_BACKEND_3D_ORIENTED_BOX_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "app/sim_runtime_3d_domain.h"

typedef struct SimRuntimeOrientedBox3DCells {
    int center_x;
    int center_y;
    int center_z;
    float axis_u_x;
    float axis_u_y;
    float axis_u_z;
    float axis_v_x;
    float axis_v_y;
    float axis_v_z;
    float axis_w_x;
    float axis_w_y;
    float axis_w_z;
    float half_u_cells;
    float half_v_cells;
    float half_w_cells;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
} SimRuntimeOrientedBox3DCells;

bool sim_runtime_backend_3d_build_preset_object_oriented_box(
    const SimRuntime3DDomainDesc *desc,
    const PresetObject *object,
    int center_x,
    int center_y,
    int center_z,
    SimRuntimeOrientedBox3DCells *out_box);
bool sim_runtime_backend_3d_cell_in_oriented_box(const SimRuntimeOrientedBox3DCells *box,
                                                 int x,
                                                 int y,
                                                 int z);

#endif
