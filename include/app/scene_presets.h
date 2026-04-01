#ifndef SCENE_PRESETS_H
#define SCENE_PRESETS_H

#include <stdbool.h>
#include "physics/math/math2d.h"
#include <stddef.h>

typedef enum FluidEmitterType {
    EMITTER_DENSITY_SOURCE = 0,
    EMITTER_VELOCITY_JET,
    EMITTER_SINK
} FluidEmitterType;

typedef struct FluidEmitter {
    FluidEmitterType type;
    float position_x;   // normalized 0..1
    float position_y;
    float position_z;   // additive dimensional field (defaults to 0 for 2D compatibility)
    float radius;       // normalized radius (fraction of grid)
    float strength;     // general scalar (density per second or velocity magnitude)
    float dir_x;        // for velocity jets / sinks
    float dir_y;
    float dir_z;        // additive dimensional field (defaults to 0 for 2D compatibility)
    int   attached_object; // -1 if free; otherwise index into preset objects
    int   attached_import; // -1 if free; otherwise index into imported shapes
} FluidEmitter;

#define MAX_FLUID_EMITTERS 32

typedef enum BoundaryFlowMode {
    BOUNDARY_FLOW_DISABLED = 0,
    BOUNDARY_FLOW_EMIT,
    BOUNDARY_FLOW_RECEIVE
} BoundaryFlowMode;

typedef enum BoundaryFlowEdge {
    BOUNDARY_EDGE_TOP = 0,
    BOUNDARY_EDGE_RIGHT,
    BOUNDARY_EDGE_BOTTOM,
    BOUNDARY_EDGE_LEFT,
    BOUNDARY_EDGE_COUNT
} BoundaryFlowEdge;

typedef struct BoundaryFlow {
    BoundaryFlowMode mode;
    float strength;
} BoundaryFlow;

typedef enum PresetObjectType {
    PRESET_OBJECT_CIRCLE = 0,
    PRESET_OBJECT_BOX
} PresetObjectType;

typedef struct PresetObject {
    PresetObjectType type;
    float position_x;
    float position_y;
    float position_z; // additive dimensional field (defaults to 0 for 2D compatibility)
    float size_x;
    float size_y;
    float size_z;     // additive dimensional field (defaults to size_x when omitted)
    float angle;
    bool  is_static;
    bool  gravity_enabled;
} PresetObject;

#define MAX_PRESET_OBJECTS 64

typedef struct ImportedShape {
    char  path[256];      // asset name or path to ShapeLib JSON
    int   shape_id;       // resolved index into ShapeAsset library (-1 if unresolved)
    float position_x;     // normalized 0..1
    float position_y;     // normalized 0..1
    float position_z;     // additive dimensional field (defaults to 0 for 2D compatibility)
    float rotation_deg;   // degrees
    float scale;          // uniform scale (1 = fit as-authored)
    float density;        // physics density override
    float friction;       // physics friction override
    bool  is_static;      // merge into static mask
    bool  enabled;
    bool  gravity_enabled;
    int   collider_vert_count;      // legacy single collider verts (fallback)
    Vec2  collider_verts[32];       // legacy
    int   collider_part_count;
    int   collider_part_offsets[16]; // start index into collider_parts_verts
    int   collider_part_counts[16];  // vert count per part
    Vec2  collider_parts_verts[128]; // pooled verts for parts (cap total)
} ImportedShape;

#define MAX_IMPORTED_SHAPES 64

typedef enum FluidSceneDomainType {
    SCENE_DOMAIN_BOX = 0,
    SCENE_DOMAIN_WIND_TUNNEL,
    SCENE_DOMAIN_STRUCTURAL
} FluidSceneDomainType;

typedef enum FluidSceneDimensionMode {
    SCENE_DIMENSION_MODE_2D = 0,
    SCENE_DIMENSION_MODE_3D
} FluidSceneDimensionMode;

typedef struct FluidScenePreset {
    const char *name;
    size_t emitter_count;
    bool   is_custom;
    FluidEmitter emitters[MAX_FLUID_EMITTERS];
    size_t object_count;
    PresetObject objects[MAX_PRESET_OBJECTS];
    size_t import_shape_count;
    ImportedShape import_shapes[MAX_IMPORTED_SHAPES];
    BoundaryFlow boundary_flows[BOUNDARY_EDGE_COUNT];
    FluidSceneDomainType domain;
    FluidSceneDimensionMode dimension_mode;
    float domain_width;
    float domain_height;
    char  structural_scene_path[256];
} FluidScenePreset;

const FluidScenePreset *scene_presets_get_all(size_t *count);
const FluidScenePreset *scene_presets_get_default(void);
const FluidScenePreset *scene_presets_get_default_for_domain(FluidSceneDomainType domain);
FluidSceneDomainType scene_preset_domain(const FluidScenePreset *preset);

#endif // SCENE_PRESETS_H
