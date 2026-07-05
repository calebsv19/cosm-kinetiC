#include "app/scene_presets.h"

static const FluidScenePreset g_presets[] = {
    {
        .name = "Calm Box",
        .emitter_count = 0,
        .domain = SCENE_DOMAIN_BOX,
        .dimension_mode = SCENE_DIMENSION_MODE_2D,
        .domain_width = 1.0f,
        .domain_height = 1.0f,
    },
    {
        .name = "Central Hotspot",
        .emitter_count = 1,
        .domain = SCENE_DOMAIN_BOX,
        .dimension_mode = SCENE_DIMENSION_MODE_2D,
        .domain_width = 1.0f,
        .domain_height = 1.0f,
        .emitters = {
            {
                .type = EMITTER_DENSITY_SOURCE,
                .position_x = 0.5f,
                .position_y = 0.5f,
                .radius = 0.12f,
                .strength = 5.0f,
                .dir_x = 0.0f,
                .dir_y = -1.0f,
                .attached_object = -1,
                .attached_import = -1,
            },
        },
    },
    {
        .name = "Corner Jet",
        .emitter_count = 1,
        .domain = SCENE_DOMAIN_BOX,
        .dimension_mode = SCENE_DIMENSION_MODE_2D,
        .domain_width = 1.0f,
        .domain_height = 1.0f,
        .emitters = {
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.4f,
                .position_y = 0.85f,
                .radius = 0.08f,
                .strength = 55.0f,
                .dir_x = 0.9f,
                .dir_y = -0.2f,
                .attached_object = -1,
                .attached_import = -1,
            },
        },
    },
    {
        .name = "Dual Jets + Exhaust",
        .emitter_count = 3,
        .domain = SCENE_DOMAIN_BOX,
        .dimension_mode = SCENE_DIMENSION_MODE_2D,
        .domain_width = 1.0f,
        .domain_height = 1.0f,
        .emitters = {
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.15f,
                .position_y = 0.2f,
                .radius = 0.07f,
                .strength = 50.0f,
                .dir_x = 1.0f,
                .dir_y = 0.0f,
                .attached_object = -1,
                .attached_import = -1,
            },
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.85f,
                .position_y = 0.8f,
                .radius = 0.07f,
                .strength = 50.0f,
                .dir_x = -1.0f,
                .dir_y = 0.0f,
                .attached_object = -1,
                .attached_import = -1,
            },
            {
                .type = EMITTER_SINK,
                .position_x = 0.5f,
                .position_y = 0.1f,
                .radius = 0.2f,
                .strength = 30.0f,
                .dir_x = 0.0f,
                .dir_y = -1.0f,
                .attached_object = -1,
                .attached_import = -1,
            },
        },
    },
    {
        .name = "Tunnel – Cylinder",
        .emitter_count = 0,
        .domain = SCENE_DOMAIN_WIND_TUNNEL,
        .dimension_mode = SCENE_DIMENSION_MODE_2D,
        .domain_width = 4.0f,
        .domain_height = 1.0f,
        .boundary_flows = {
            [BOUNDARY_EDGE_LEFT] =  { .mode = BOUNDARY_FLOW_EMIT,    .strength = 40.0f },
            [BOUNDARY_EDGE_RIGHT] = { .mode = BOUNDARY_FLOW_RECEIVE, .strength = 40.0f },
            [BOUNDARY_EDGE_TOP] =   { .mode = BOUNDARY_FLOW_RECEIVE, .strength = 10.0f },
            [BOUNDARY_EDGE_BOTTOM] ={ .mode = BOUNDARY_FLOW_DISABLED, .strength = 0.0f },
        },
        .object_count = 1,
        .objects = {
            {
                .type = PRESET_OBJECT_CIRCLE,
                .position_x = 0.35f,
                .position_y = 0.5f,
                .size_x = 0.08f,
                .size_y = 0.08f,
                .angle = 0.0f,
                .is_static = true,
            },
        },
    },
    {
        .name = "Atmospheric Layers",
        .emitter_count = 0,
        .domain = SCENE_DOMAIN_ATMOSPHERIC,
        .dimension_mode = SCENE_DIMENSION_MODE_3D,
        .domain_width = 1.0f,
        .domain_height = 1.0f,
        .atmosphere = {
            .enabled = true,
            .seed = 1337u,
            .base_density = 0.0f,
            .density_scale = 7.0f,
            .density_threshold = 0.54f,
            .base_wind_x = 4.0f,
            .base_wind_y = 0.25f,
            .base_wind_z = 1.0f,
            .turbulence_strength = 2.0f,
            .noise_scale = 3.0f,
            .detail_scale = 10.0f,
            .band_min_y = 0.22f,
            .band_max_y = 0.78f,
            .band_edge_falloff = 0.16f,
            .region_count = 2,
            .regions = {
                {
                    .enabled = true,
                    .shape = ATMOSPHERIC_REGION_ELLIPSE,
                    .center_x = 0.38f,
                    .center_y = 0.50f,
                    .center_z = 0.48f,
                    .size_x = 0.28f,
                    .size_y = 0.16f,
                    .size_z = 0.22f,
                    .density = 2.0f,
                    .falloff = 0.35f,
                },
                {
                    .enabled = true,
                    .shape = ATMOSPHERIC_REGION_RECT,
                    .center_x = 0.70f,
                    .center_y = 0.62f,
                    .center_z = 0.55f,
                    .size_x = 0.18f,
                    .size_y = 0.12f,
                    .size_z = 0.18f,
                    .density = 1.25f,
                    .falloff = 0.45f,
                },
            },
        },
    },
    {
        .name = "Water Basin",
        .emitter_count = 0,
        .domain = SCENE_DOMAIN_WATER,
        .dimension_mode = SCENE_DIMENSION_MODE_3D,
        .domain_width = 4.0f,
        .domain_height = 1.0f,
    },
};

static const FluidScenePreset g_structural_default = {
    .name = "Structural",
    .emitter_count = 0,
    .is_custom = false,
    .object_count = 0,
    .import_shape_count = 0,
    .boundary_flows = {0},
    .domain = SCENE_DOMAIN_STRUCTURAL,
    .dimension_mode = SCENE_DIMENSION_MODE_2D,
    .domain_width = 1.0f,
    .domain_height = 1.0f,
    .structural_scene_path = ""
};

const FluidScenePreset *scene_presets_get_all(size_t *count) {
    if (count) {
        *count = sizeof(g_presets) / sizeof(g_presets[0]);
    }
    return g_presets;
}

const FluidScenePreset *scene_presets_get_default(void) {
    return scene_presets_get_default_for_domain(SCENE_DOMAIN_BOX);
}

const FluidScenePreset *scene_presets_get_default_for_domain(FluidSceneDomainType domain) {
    if (domain == SCENE_DOMAIN_STRUCTURAL) {
        return &g_structural_default;
    }
    size_t preset_count = sizeof(g_presets) / sizeof(g_presets[0]);
    for (size_t i = 0; i < preset_count; ++i) {
        if (g_presets[i].domain == domain) {
            return &g_presets[i];
        }
    }
    return (preset_count > 0) ? &g_presets[0] : NULL;
}

FluidSceneDomainType scene_preset_domain(const FluidScenePreset *preset) {
    if (!preset) return SCENE_DOMAIN_BOX;
    return preset->domain;
}
