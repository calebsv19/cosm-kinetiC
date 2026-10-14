#ifndef PHYSICS_SIM_MENU_WINDOW_LIFECYCLE_H
#define PHYSICS_SIM_MENU_WINDOW_LIFECYCLE_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "kit_ui_window_sdl.h"

typedef struct PhysicsSimMenuWindowLifecycle {
    KitUiWindowState observed;
    bool release_blocked;
    bool pointer_down;
    bool geometry_dirty;
    Uint8 blocked_keys[SDL_NUM_SCANCODES];
} PhysicsSimMenuWindowLifecycle;

struct SceneMenuInteraction;
/* Transient input only: caller-owned text/settings/authoring drafts persist. */
void menu_window_lifecycle_invalidate(struct SceneMenuInteraction *ctx);
uint32_t menu_window_lifecycle_refresh(struct SceneMenuInteraction *ctx);
bool menu_window_lifecycle_filter(struct SceneMenuInteraction *ctx,
                                  const SDL_Event *event);
#endif
