#include "app/menu/menu_settings_schema.h"

static const MenuSettingsFieldDef MENU_SETTINGS_FIELDS[MENU_SETTINGS_FIELD_COUNT] = {
    [MENU_SETTINGS_FIELD_GRID_X] = {
        .id = MENU_SETTINGS_FIELD_GRID_X,
        .label = "X Cells",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 32.0,
        .max_value = 512.0,
        .step = 32.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_GRID_Y] = {
        .id = MENU_SETTINGS_FIELD_GRID_Y,
        .label = "Y Cells",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 32.0,
        .max_value = 512.0,
        .step = 32.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_GRID_Z] = {
        .id = MENU_SETTINGS_FIELD_GRID_Z,
        .label = "Z Cells",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 0.0,
        .max_value = 256.0,
        .step = 16.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_3D_APPLIED_AXIS] = {
        .id = MENU_SETTINGS_FIELD_3D_APPLIED_AXIS,
        .label = "Applied Axis",
        .kind = MENU_SETTINGS_FIELD_KIND_ACTION,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_3D,
        .first_rollout = true,
        .runtime_display_only = true
    },
    [MENU_SETTINGS_FIELD_3D_APPLIED_DEPTH] = {
        .id = MENU_SETTINGS_FIELD_3D_APPLIED_DEPTH,
        .label = "Applied Z",
        .kind = MENU_SETTINGS_FIELD_KIND_ACTION,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_3D,
        .first_rollout = true,
        .runtime_display_only = true
    },
    [MENU_SETTINGS_FIELD_3D_DEPTH_POLICY] = {
        .id = MENU_SETTINGS_FIELD_3D_DEPTH_POLICY,
        .label = "Z Policy",
        .kind = MENU_SETTINGS_FIELD_KIND_ACTION,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_3D,
        .first_rollout = true,
        .runtime_display_only = true
    },
    [MENU_SETTINGS_FIELD_QUALITY_PRESET] = {
        .id = MENU_SETTINGS_FIELD_QUALITY_PRESET,
        .label = "Preset",
        .kind = MENU_SETTINGS_FIELD_KIND_ACTION,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS] = {
        .id = MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS,
        .label = "Substeps",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 1.0,
        .max_value = 32.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_SOLVER_ITERATIONS] = {
        .id = MENU_SETTINGS_FIELD_SOLVER_ITERATIONS,
        .label = "Solver",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 1.0,
        .max_value = 64.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_DENSITY_DIFFUSION] = {
        .id = MENU_SETTINGS_FIELD_DENSITY_DIFFUSION,
        .label = "Diffusion",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 0.01,
        .step = 0.00005,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_DENSITY_DECAY] = {
        .id = MENU_SETTINGS_FIELD_DENSITY_DECAY,
        .label = "Decay",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 1.0,
        .step = 0.01,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_BUOYANCY] = {
        .id = MENU_SETTINGS_FIELD_BUOYANCY,
        .label = "Buoyancy",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 10.0,
        .step = 0.1,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER] = {
        .id = MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER,
        .label = "Emit Density",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 10.0,
        .step = 0.1,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER] = {
        .id = MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER,
        .label = "Emit Velocity",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 10.0,
        .step = 0.1,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER] = {
        .id = MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER,
        .label = "Sink Strength",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 10.0,
        .step = 0.1,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES] = {
        .id = MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES,
        .label = "Save Volume Frames",
        .kind = MENU_SETTINGS_FIELD_KIND_BOOL,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES] = {
        .id = MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES,
        .label = "Save Render Frames",
        .kind = MENU_SETTINGS_FIELD_KIND_BOOL,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT] = {
        .id = MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT,
        .label = "Headless Frame Count",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 0.0,
        .max_value = 100000.0,
        .step = 100.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_STRUCTURAL |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED] = {
        .id = MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED,
        .label = "Tunnel Inflow Speed",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 500.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY] = {
        .id = MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY,
        .label = "Inflow Density",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 100.0,
        .step = 0.5,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE] = {
        .id = MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE,
        .label = "Tunnel Visc Scale",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 5.0,
        .step = 0.05,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_WIND,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_VELOCITY_DAMPING] = {
        .id = MENU_SETTINGS_FIELD_VELOCITY_DAMPING,
        .label = "Viscosity",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 0.1,
        .step = 0.000002,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR] = {
        .id = MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR,
        .label = "Enable Render Blur",
        .kind = MENU_SETTINGS_FIELD_KIND_BOOL,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_COMMON |
                         MENU_SETTINGS_PROVIDER_BIT_2D |
                         MENU_SETTINGS_PROVIDER_BIT_3D |
                         MENU_SETTINGS_PROVIDER_BIT_WIND |
                         MENU_SETTINGS_PROVIDER_BIT_WATER |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .advanced = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED,
        .label = "Atmo Seed",
        .kind = MENU_SETTINGS_FIELD_KIND_INT,
        .min_value = 1.0,
        .max_value = 1000000000.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE,
        .label = "Atmo Density",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 50.0,
        .step = 0.25,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_THRESHOLD] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_THRESHOLD,
        .label = "Atmo Cutoff",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 1.0,
        .step = 0.02,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X,
        .label = "Wind X",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = -200.0,
        .max_value = 200.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Y] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Y,
        .label = "Wind Y",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = -200.0,
        .max_value = 200.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z,
        .label = "Wind Z",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = -200.0,
        .max_value = 200.0,
        .step = 1.0,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_TURBULENCE] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_TURBULENCE,
        .label = "Turbulence",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 100.0,
        .step = 0.5,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_NOISE_SCALE] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_NOISE_SCALE,
        .label = "Noise Scale",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.01,
        .max_value = 128.0,
        .step = 0.25,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN,
        .label = "Band Min",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 1.0,
        .step = 0.02,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX,
        .label = "Band Max",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 1.0,
        .step = 0.02,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_2D |
                         MENU_SETTINGS_PROVIDER_BIT_ATMOSPHERIC_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_ATMOSPHERIC_INITIAL_STATE] = {
        .id = MENU_SETTINGS_FIELD_ATMOSPHERIC_INITIAL_STATE,
        .label = "Atmo Init",
        .kind = MENU_SETTINGS_FIELD_KIND_BOOL,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_3D,
        .first_rollout = true
    },
    [MENU_SETTINGS_FIELD_WATER_LEVEL] = {
        .id = MENU_SETTINGS_FIELD_WATER_LEVEL,
        .label = "Water Level",
        .kind = MENU_SETTINGS_FIELD_KIND_FLOAT,
        .min_value = 0.0,
        .max_value = 1.0,
        .step = 0.05,
        .provider_bits = MENU_SETTINGS_PROVIDER_BIT_WATER,
        .first_rollout = true
    }
};

size_t menu_settings_provider_common_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_2d_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_3d_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_wind_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_structural_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_atmospheric_2d_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_atmospheric_3d_fields(const MenuSettingsFieldId **out_fields);
size_t menu_settings_provider_water_fields(const MenuSettingsFieldId **out_fields);

const MenuSettingsFieldDef *menu_settings_schema_field(MenuSettingsFieldId id) {
    if (id < 0 || id >= MENU_SETTINGS_FIELD_COUNT) return NULL;
    return &MENU_SETTINGS_FIELDS[id];
}

size_t menu_settings_schema_provider_fields(MenuSettingsProviderId provider,
                                            const MenuSettingsFieldId **out_fields) {
    switch (provider) {
    case MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D:
        return menu_settings_provider_atmospheric_3d_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_ATMOSPHERIC_2D:
        return menu_settings_provider_atmospheric_2d_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_STRUCTURAL:
        return menu_settings_provider_structural_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_WIND:
        return menu_settings_provider_wind_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_WATER:
        return menu_settings_provider_water_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_3D:
        return menu_settings_provider_3d_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_2D:
        return menu_settings_provider_2d_fields(out_fields);
    case MENU_SETTINGS_PROVIDER_COMMON:
    default:
        return menu_settings_provider_common_fields(out_fields);
    }
}

MenuSettingsProviderId menu_settings_schema_provider_for_modes(SimulationMode sim_mode,
                                                               SpaceMode space_mode) {
    (void)space_mode;
    switch (sim_mode) {
    case SIM_MODE_STRUCTURAL:
        return MENU_SETTINGS_PROVIDER_STRUCTURAL;
    case SIM_MODE_WIND_TUNNEL:
        return MENU_SETTINGS_PROVIDER_WIND;
    case SIM_MODE_ATMOSPHERIC:
        return (space_mode == SPACE_MODE_3D)
                   ? MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D
                   : MENU_SETTINGS_PROVIDER_ATMOSPHERIC_2D;
    case SIM_MODE_WATER:
        return MENU_SETTINGS_PROVIDER_WATER;
    case SIM_MODE_BOX:
    default:
        return (space_mode == SPACE_MODE_3D)
                   ? MENU_SETTINGS_PROVIDER_3D
                   : MENU_SETTINGS_PROVIDER_2D;
    }
}
