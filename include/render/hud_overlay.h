#ifndef HUD_OVERLAY_H
#define HUD_OVERLAY_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "render/renderer_sdl.h"  // for RendererHudInfo

// Initializes HUD overlay module for the given renderer.
// Assumes SDL_ttf has already been initialized via TTF_Init().
bool hud_overlay_init(SDL_Renderer *renderer);

// Releases HUD overlay resources (fonts, etc.).
void hud_overlay_shutdown(void);

// Draws the HUD panel for the given info. Safe to call even if init failed;
// it will just no-op if it has no font/renderer.
void hud_overlay_draw(const RendererHudInfo *hud);

#endif // HUD_OVERLAY_H
