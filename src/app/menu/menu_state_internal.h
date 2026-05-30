#ifndef MENU_STATE_INTERNAL_H
#define MENU_STATE_INTERNAL_H

#include "app/menu/menu_state.h"

FluidSceneDomainType menu_current_domain(const SceneMenuInteraction *ctx);
void menu_reload_settings_from_active_preset(SceneMenuInteraction *ctx);

#endif
