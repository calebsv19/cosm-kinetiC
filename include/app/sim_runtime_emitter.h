#ifndef SIM_RUNTIME_EMITTER_H
#define SIM_RUNTIME_EMITTER_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_presets.h"
#include "app/sim_runtime_3d_domain.h"

typedef enum SimRuntimeEmitterSourceKind {
    SIM_RUNTIME_EMITTER_SOURCE_FREE = 0,
    SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT = 1,
    SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_IMPORT = 2
} SimRuntimeEmitterSourceKind;

typedef enum SimRuntimeEmitterFootprintKind {
    SIM_RUNTIME_EMITTER_FOOTPRINT_RADIAL_SPHERE = 0,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_OCCUPANCY = 1,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_OCCUPANCY = 2,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_PATCH = 3,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_PATCH = 4,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_SURFACE_SHELL = 5,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_SURFACE_SHELL = 6,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_OBJECT_HEATED_OBSTACLE = 7,
    SIM_RUNTIME_EMITTER_FOOTPRINT_ATTACHED_IMPORT_HEATED_OBSTACLE = 8
} SimRuntimeEmitterFootprintKind;

typedef struct SimRuntimeEmitterResolved {
    size_t emitter_index;
    FluidEmitterType type;
    SimRuntimeEmitterSourceKind source_kind;
    SimRuntimeEmitterFootprintKind primary_footprint;
    SimRuntimeEmitterFootprintKind fallback_footprint;
    int attached_object;
    int attached_import;
    float position_x;
    float position_y;
    float position_z;
    float radius;
    float strength;
    float dir_x;
    float dir_y;
    float dir_z;
    FluidEmitter3DSourceMode source_mode_3d;
    FluidEmitter3DSurface surface_3d;
    FluidEmitter3DObstacleMode obstacle_mode_3d;
    float thermal_buoyancy_3d;
    bool direction_has_magnitude;
} SimRuntimeEmitterResolved;

typedef struct SimRuntimeEmitterPlacement3D {
    int center_x;
    int center_y;
    int center_z;
    int radius_cells;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int min_z;
    int max_z;
} SimRuntimeEmitterPlacement3D;

bool sim_runtime_emitter_resolve(const FluidScenePreset *preset,
                                 size_t emitter_index,
                                 SimRuntimeEmitterResolved *out_resolved);
bool sim_runtime_emitter_resolve_3d_placement(const SimRuntime3DDomainDesc *domain,
                                              const SimRuntimeEmitterResolved *resolved,
                                              SimRuntimeEmitterPlacement3D *out_placement);

const char *sim_runtime_emitter_source_kind_label(SimRuntimeEmitterSourceKind kind);
const char *sim_runtime_emitter_footprint_kind_label(SimRuntimeEmitterFootprintKind kind);
const char *sim_runtime_emitter_3d_source_mode_label(FluidEmitter3DSourceMode mode);
const char *sim_runtime_emitter_3d_surface_label(FluidEmitter3DSurface surface);
const char *sim_runtime_emitter_3d_obstacle_mode_label(FluidEmitter3DObstacleMode mode);

#endif // SIM_RUNTIME_EMITTER_H
