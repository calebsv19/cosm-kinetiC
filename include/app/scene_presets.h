#ifndef SCENE_PRESETS_H
#define SCENE_PRESETS_H

#include <stdbool.h>
#include "physics/math/math2d.h"
#include <stddef.h>
#include <stdint.h>

typedef enum FluidEmitterType {
    EMITTER_DENSITY_SOURCE = 0,
    EMITTER_VELOCITY_JET,
    EMITTER_SINK
} FluidEmitterType;

typedef enum FluidEmitter3DSourceMode {
    EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT = 0,
    EMITTER_3D_SOURCE_MODE_VOLUME_FILL,
    EMITTER_3D_SOURCE_MODE_SURFACE_PATCH,
    EMITTER_3D_SOURCE_MODE_SURFACE_SHELL,
    EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE
} FluidEmitter3DSourceMode;

typedef enum FluidEmitter3DSurface {
    EMITTER_3D_SURFACE_AUTO = 0,
    EMITTER_3D_SURFACE_TOP,
    EMITTER_3D_SURFACE_BOTTOM,
    EMITTER_3D_SURFACE_LEFT,
    EMITTER_3D_SURFACE_RIGHT,
    EMITTER_3D_SURFACE_FRONT,
    EMITTER_3D_SURFACE_BACK,
    EMITTER_3D_SURFACE_ALL_FACES
} FluidEmitter3DSurface;

typedef enum FluidEmitter3DObstacleMode {
    EMITTER_3D_OBSTACLE_MODE_AUTO = 0,
    EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED,
    EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED
} FluidEmitter3DObstacleMode;

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
    FluidEmitter3DSourceMode source_mode_3d;
    FluidEmitter3DSurface surface_3d;
    FluidEmitter3DObstacleMode obstacle_mode_3d;
    float thermal_buoyancy_3d;
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
    float angle;      // legacy XY rotation compatibility carrier
    bool  orientation_basis_valid;
    float orientation_u_x;
    float orientation_u_y;
    float orientation_u_z;
    float orientation_v_x;
    float orientation_v_y;
    float orientation_v_z;
    float orientation_w_x;
    float orientation_w_y;
    float orientation_w_z;
    bool  is_static;
    bool  gravity_enabled;
    float initial_velocity_x; // reduced compatibility carrier for runtime body bootstrap
    float initial_velocity_y; // reduced compatibility carrier for runtime body bootstrap
    float initial_velocity_z; // additive dimensional carry-through; current runtime ignores z
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
    SCENE_DOMAIN_STRUCTURAL,
    SCENE_DOMAIN_ATMOSPHERIC
} FluidSceneDomainType;

typedef enum FluidSceneDimensionMode {
    SCENE_DIMENSION_MODE_2D = 0,
    SCENE_DIMENSION_MODE_3D
} FluidSceneDimensionMode;

typedef enum AtmosphericRegionShape {
    ATMOSPHERIC_REGION_RECT = 0,
    ATMOSPHERIC_REGION_ELLIPSE
} AtmosphericRegionShape;

typedef struct AtmosphericDensityRegion {
    bool enabled;
    AtmosphericRegionShape shape;
    float center_x;
    float center_y;
    float center_z;
    float size_x;
    float size_y;
    float size_z;
    float density;
    float falloff;
} AtmosphericDensityRegion;

#define MAX_ATMOSPHERIC_DENSITY_REGIONS 8

typedef struct AtmosphericPresetSettings {
    bool enabled;
    uint32_t seed;
    float base_density;
    float density_scale;
    float density_threshold;
    float base_wind_x;
    float base_wind_y;
    float base_wind_z;
    float turbulence_strength;
    float noise_scale;
    float detail_scale;
    float band_min_y;
    float band_max_y;
    float band_edge_falloff;
    size_t region_count;
    AtmosphericDensityRegion regions[MAX_ATMOSPHERIC_DENSITY_REGIONS];
} AtmosphericPresetSettings;

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
    bool atmospheric_initial_state_enabled;
    AtmosphericPresetSettings atmosphere;
} FluidScenePreset;

const FluidScenePreset *scene_presets_get_all(size_t *count);
const FluidScenePreset *scene_presets_get_default(void);
const FluidScenePreset *scene_presets_get_default_for_domain(FluidSceneDomainType domain);
FluidSceneDomainType scene_preset_domain(const FluidScenePreset *preset);

#endif // SCENE_PRESETS_H
