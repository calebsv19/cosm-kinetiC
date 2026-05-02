#ifndef MENU_SETTINGS_TYPES_H
#define MENU_SETTINGS_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_config.h"

typedef enum MenuSettingsFieldKind {
    MENU_SETTINGS_FIELD_KIND_INT = 0,
    MENU_SETTINGS_FIELD_KIND_FLOAT,
    MENU_SETTINGS_FIELD_KIND_BOOL,
    MENU_SETTINGS_FIELD_KIND_ACTION
} MenuSettingsFieldKind;

typedef enum MenuSettingsProviderId {
    MENU_SETTINGS_PROVIDER_COMMON = 0,
    MENU_SETTINGS_PROVIDER_2D,
    MENU_SETTINGS_PROVIDER_3D,
    MENU_SETTINGS_PROVIDER_WIND,
    MENU_SETTINGS_PROVIDER_STRUCTURAL
} MenuSettingsProviderId;

typedef enum MenuSettingsFieldId {
    MENU_SETTINGS_FIELD_GRID_X = 0,
    MENU_SETTINGS_FIELD_GRID_Y,
    MENU_SETTINGS_FIELD_GRID_Z,
    MENU_SETTINGS_FIELD_3D_APPLIED_AXIS,
    MENU_SETTINGS_FIELD_3D_APPLIED_DEPTH,
    MENU_SETTINGS_FIELD_3D_DEPTH_POLICY,
    MENU_SETTINGS_FIELD_QUALITY_PRESET,
    MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS,
    MENU_SETTINGS_FIELD_SOLVER_ITERATIONS,
    MENU_SETTINGS_FIELD_DENSITY_DIFFUSION,
    MENU_SETTINGS_FIELD_DENSITY_DECAY,
    MENU_SETTINGS_FIELD_BUOYANCY,
    MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER,
    MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER,
    MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER,
    MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES,
    MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES,
    MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT,
    MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED,
    MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY,
    MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE,
    MENU_SETTINGS_FIELD_VELOCITY_DAMPING,
    MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR,
    MENU_SETTINGS_FIELD_COUNT
} MenuSettingsFieldId;

enum {
    MENU_SETTINGS_PROVIDER_BIT_COMMON = 1u << MENU_SETTINGS_PROVIDER_COMMON,
    MENU_SETTINGS_PROVIDER_BIT_2D = 1u << MENU_SETTINGS_PROVIDER_2D,
    MENU_SETTINGS_PROVIDER_BIT_3D = 1u << MENU_SETTINGS_PROVIDER_3D,
    MENU_SETTINGS_PROVIDER_BIT_WIND = 1u << MENU_SETTINGS_PROVIDER_WIND,
    MENU_SETTINGS_PROVIDER_BIT_STRUCTURAL = 1u << MENU_SETTINGS_PROVIDER_STRUCTURAL
};

typedef struct MenuSettingsFieldDef {
    MenuSettingsFieldId id;
    const char *label;
    MenuSettingsFieldKind kind;
    double min_value;
    double max_value;
    double step;
    unsigned provider_bits;
    bool first_rollout;
    bool advanced;
    bool runtime_display_only;
} MenuSettingsFieldDef;

typedef struct MenuSettingsDraft {
    int grid_x;
    int grid_y;
    int grid_z;
    int quality_index;
    int physics_substeps;
    int fluid_solver_iterations;
    int headless_frame_count;
    float density_diffusion;
    float density_decay;
    float fluid_buoyancy_force;
    float emitter_density_multiplier;
    float emitter_velocity_multiplier;
    float emitter_sink_multiplier;
    float tunnel_inflow_speed;
    float tunnel_inflow_density;
    float tunnel_viscosity_scale;
    float velocity_damping;
    bool save_volume_frames;
    bool save_render_frames;
    bool enable_render_blur;
    unsigned dirty_bits;
} MenuSettingsDraft;

typedef struct MenuSettingsShellState {
    MenuSettingsDraft draft;
    MenuSettingsDraft saved_draft;
    MenuSettingsProviderId provider;
    bool initialized;
    bool saved_snapshot_initialized;
} MenuSettingsShellState;

#endif
