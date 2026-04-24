#ifndef PHYSICS_SIM_RENDER_FONT_BRIDGE_H
#define PHYSICS_SIM_RENDER_FONT_BRIDGE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "app/app_config.h"
#include "core_base.h"
#include "kit_render.h"

typedef enum PhysicsSimFontSlot {
    PHYSICS_SIM_FONT_SLOT_MENU_TITLE = 0,
    PHYSICS_SIM_FONT_SLOT_MENU_BODY = 1,
    PHYSICS_SIM_FONT_SLOT_MENU_SMALL = 2,
    PHYSICS_SIM_FONT_SLOT_HUD_OVERLAY = 3,
    PHYSICS_SIM_FONT_SLOT_TIMER_HUD = 4,
    PHYSICS_SIM_FONT_SLOT_STRUCTURAL_SMALL = 5,
    PHYSICS_SIM_FONT_SLOT_STRUCTURAL_HUD = 6
} PhysicsSimFontSlot;

typedef struct PhysicsSimResolvedFont {
    KitRenderResolvedTextRun text_run;
    char resolved_path[384];
    int used_shared_font;
} PhysicsSimResolvedFont;

CoreResult physics_sim_font_bridge_open(SDL_Renderer *renderer,
                                        const AppConfig *cfg,
                                        PhysicsSimFontSlot slot,
                                        TTF_Font **out_font,
                                        PhysicsSimResolvedFont *out_resolved);

void physics_sim_font_bridge_close(TTF_Font **font);

#endif
