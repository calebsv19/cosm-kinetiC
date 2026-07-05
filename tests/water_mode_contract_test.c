#include "app/water_mode.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static bool test_route_activation_is_water_3d_only(void) {
    if (!water_mode_route_active(SIM_MODE_WATER, SPACE_MODE_3D)) return false;
    if (water_mode_route_active(SIM_MODE_WATER, SPACE_MODE_2D)) return false;
    if (water_mode_route_active(SIM_MODE_BOX, SPACE_MODE_3D)) return false;
    if (water_mode_route_active(SIM_MODE_WIND_TUNNEL, SPACE_MODE_3D)) return false;
    if (water_mode_route_active(SIM_MODE_ATMOSPHERIC, SPACE_MODE_3D)) return false;
    return true;
}

static bool test_default_config_uses_standard_domain_and_water_level(void) {
    AppConfig cfg = {0};
    cfg.water_level = 0.35f;
    WaterModeConfig config = water_mode_config_default(&cfg);
    if (!config.active) return false;
    if (!nearly_equal(config.water_level, 0.35f)) return false;
    if (!nearly_equal(config.world_min_x, 0.0f)) return false;
    if (!nearly_equal(config.world_min_y, 0.0f)) return false;
    if (!nearly_equal(config.world_min_z, 0.0f)) return false;
    if (!nearly_equal(config.world_max_x, WATER_MODE_DOMAIN_WIDTH_DEFAULT)) return false;
    if (!nearly_equal(config.world_max_y, WATER_MODE_DOMAIN_HEIGHT_DEFAULT)) return false;
    if (!nearly_equal(config.world_max_z, WATER_MODE_DOMAIN_DEPTH_DEFAULT)) return false;
    if (!nearly_equal(config.surface_y, 0.35f)) return false;
    return water_mode_config_validate(&config);
}

static bool test_water_level_clamps_invalid_values(void) {
    AppConfig cfg = {0};
    WaterModeConfig config = {0};

    cfg.water_level = -1.0f;
    config = water_mode_config_default(&cfg);
    if (!nearly_equal(config.water_level, 0.0f)) return false;
    if (!water_mode_config_validate(&config)) return false;

    cfg.water_level = 2.0f;
    config = water_mode_config_default(&cfg);
    if (!nearly_equal(config.water_level, 1.0f)) return false;
    if (!water_mode_config_validate(&config)) return false;

    cfg.water_level = NAN;
    config = water_mode_config_default(&cfg);
    if (!nearly_equal(config.water_level, WATER_MODE_LEVEL_DEFAULT)) return false;
    return water_mode_config_validate(&config);
}

static bool test_apply_preset_sets_water_domain_without_erasing_content(void) {
    FluidScenePreset preset = {0};
    WaterModeConfig config = water_mode_config_default(NULL);
    preset.domain = SCENE_DOMAIN_BOX;
    preset.dimension_mode = SCENE_DIMENSION_MODE_2D;
    preset.domain_width = 1.0f;
    preset.domain_height = 1.0f;
    preset.object_count = 1;
    preset.objects[0].type = PRESET_OBJECT_CIRCLE;
    preset.objects[0].position_x = 0.5f;
    preset.objects[0].position_y = 0.5f;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].mode = BOUNDARY_FLOW_EMIT;
    preset.boundary_flows[BOUNDARY_EDGE_LEFT].strength = 12.0f;

    water_mode_apply_preset(&config, &preset);
    if (preset.domain != SCENE_DOMAIN_WATER) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (!nearly_equal(preset.domain_width, WATER_MODE_DOMAIN_WIDTH_DEFAULT)) return false;
    if (!nearly_equal(preset.domain_height, WATER_MODE_DOMAIN_HEIGHT_DEFAULT)) return false;
    if (preset.object_count != 1) return false;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        if (preset.boundary_flows[edge].mode != BOUNDARY_FLOW_DISABLED) return false;
        if (!nearly_equal(preset.boundary_flows[edge].strength, 0.0f)) return false;
    }
    return true;
}

int main(void) {
    if (!test_route_activation_is_water_3d_only()) {
        fprintf(stderr, "water_mode_contract_test: route activation failed\n");
        return 1;
    }
    if (!test_default_config_uses_standard_domain_and_water_level()) {
        fprintf(stderr, "water_mode_contract_test: default config failed\n");
        return 1;
    }
    if (!test_water_level_clamps_invalid_values()) {
        fprintf(stderr, "water_mode_contract_test: water level clamp failed\n");
        return 1;
    }
    if (!test_apply_preset_sets_water_domain_without_erasing_content()) {
        fprintf(stderr, "water_mode_contract_test: preset application failed\n");
        return 1;
    }
    fprintf(stdout, "water_mode_contract_test: success\n");
    return 0;
}
