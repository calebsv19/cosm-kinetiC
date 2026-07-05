#include "app/water_mode.h"

#include <math.h>

bool water_mode_route_active(SimulationMode mode, SpaceMode space_mode) {
    return mode == SIM_MODE_WATER && space_mode == SPACE_MODE_3D;
}

float water_mode_level_clamp(float water_level) {
    if (!isfinite(water_level)) return WATER_MODE_LEVEL_DEFAULT;
    if (water_level < 0.0f) return 0.0f;
    if (water_level > 1.0f) return 1.0f;
    return water_level;
}

WaterModeConfig water_mode_config_default(const AppConfig *cfg) {
    WaterModeConfig config;
    config.active = true;
    config.water_level = water_mode_level_clamp(cfg ? cfg->water_level : WATER_MODE_LEVEL_DEFAULT);
    config.world_min_x = 0.0f;
    config.world_min_y = 0.0f;
    config.world_min_z = 0.0f;
    config.world_max_x = WATER_MODE_DOMAIN_WIDTH_DEFAULT;
    config.world_max_y = WATER_MODE_DOMAIN_HEIGHT_DEFAULT;
    config.world_max_z = WATER_MODE_DOMAIN_DEPTH_DEFAULT;
    config.surface_y = config.world_min_y +
                       (config.world_max_y - config.world_min_y) * config.water_level;
    return config;
}

bool water_mode_config_validate(const WaterModeConfig *config) {
    if (!config || !config->active) return false;
    if (!isfinite(config->water_level) ||
        config->water_level < 0.0f ||
        config->water_level > 1.0f) {
        return false;
    }
    if (!isfinite(config->world_min_x) ||
        !isfinite(config->world_min_y) ||
        !isfinite(config->world_min_z) ||
        !isfinite(config->world_max_x) ||
        !isfinite(config->world_max_y) ||
        !isfinite(config->world_max_z) ||
        !isfinite(config->surface_y)) {
        return false;
    }
    if (config->world_max_x <= config->world_min_x ||
        config->world_max_y <= config->world_min_y ||
        config->world_max_z <= config->world_min_z) {
        return false;
    }
    return config->surface_y >= config->world_min_y &&
           config->surface_y <= config->world_max_y;
}

void water_mode_apply_preset(const WaterModeConfig *config, FluidScenePreset *preset) {
    WaterModeConfig fallback;
    const WaterModeConfig *resolved = config;
    if (!preset) return;
    if (!resolved || !water_mode_config_validate(resolved)) {
        fallback = water_mode_config_default(NULL);
        resolved = &fallback;
    }
    preset->domain = SCENE_DOMAIN_WATER;
    preset->dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset->domain_width = resolved->world_max_x - resolved->world_min_x;
    preset->domain_height = resolved->world_max_y - resolved->world_min_y;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        preset->boundary_flows[edge].mode = BOUNDARY_FLOW_DISABLED;
        preset->boundary_flows[edge].strength = 0.0f;
    }
}
