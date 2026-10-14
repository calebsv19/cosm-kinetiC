#ifndef PHYSICS_SIM_TIMER_HUD_ADAPTER_H
#define PHYSICS_SIM_TIMER_HUD_ADAPTER_H

#include <SDL2/SDL.h>
#include "app/app_config.h"

void timer_hud_register_backend(void);
void timer_hud_apply_startup_env_overrides(void);
void timer_hud_bind_renderer(SDL_Renderer* renderer);
void timer_hud_unbind_renderer(void);
bool timer_hud_reload_font(const AppConfig *);

#endif // PHYSICS_SIM_TIMER_HUD_ADAPTER_H
