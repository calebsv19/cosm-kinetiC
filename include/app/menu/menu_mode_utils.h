#ifndef MENU_MODE_UTILS_H
#define MENU_MODE_UTILS_H

#include "app/app_config.h"

void menu_clamp_grid_size(AppConfig *cfg);
SimulationMode menu_normalize_sim_mode(SimulationMode mode);
const char *menu_mode_label(SimulationMode mode);
SpaceMode menu_normalize_space_mode(SpaceMode mode);
const char *menu_space_mode_label(SpaceMode mode);

#endif
