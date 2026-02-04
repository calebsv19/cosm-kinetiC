#ifndef PHYSICS_SIM_TIMER_HUD_ADAPTER_H
#define PHYSICS_SIM_TIMER_HUD_ADAPTER_H

#include <SDL2/SDL.h>

void timer_hud_register_backend(void);
void timer_hud_bind_renderer(SDL_Renderer* renderer);

#endif // PHYSICS_SIM_TIMER_HUD_ADAPTER_H
