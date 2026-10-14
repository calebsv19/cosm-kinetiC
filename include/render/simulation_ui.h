#ifndef PHYSICS_SIM_SIMULATION_UI_H
#define PHYSICS_SIM_SIMULATION_UI_H
#include <SDL2/SDL.h>
void simulation_ui_attach(SDL_Window *, SDL_Renderer *);
void simulation_ui_detach(void);
void simulation_ui_layout_ready(void);
void simulation_ui_presented(void);
#endif
