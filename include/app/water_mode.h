#ifndef WATER_MODE_H
#define WATER_MODE_H

#include <stdbool.h>

#include "app/app_config.h"
#include "app/scene_presets.h"

#define WATER_MODE_DOMAIN_WIDTH_DEFAULT 4.0f
#define WATER_MODE_DOMAIN_HEIGHT_DEFAULT 1.0f
#define WATER_MODE_DOMAIN_DEPTH_DEFAULT 4.0f
#define WATER_MODE_LEVEL_DEFAULT 0.5f

typedef struct WaterModeConfig {
    bool active;
    float water_level;
    float world_min_x;
    float world_min_y;
    float world_min_z;
    float world_max_x;
    float world_max_y;
    float world_max_z;
    float surface_y;
} WaterModeConfig;

bool water_mode_route_active(SimulationMode mode, SpaceMode space_mode);
float water_mode_level_clamp(float water_level);
WaterModeConfig water_mode_config_default(const AppConfig *cfg);
bool water_mode_config_validate(const WaterModeConfig *config);
void water_mode_apply_preset(const WaterModeConfig *config, FluidScenePreset *preset);

#endif // WATER_MODE_H
