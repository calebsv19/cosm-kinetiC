#ifndef PHYSICS_MENU_SHARED_CONTROLS_H
#define PHYSICS_MENU_SHARED_CONTROLS_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "kit_ui_surface.h"
struct SceneMenuInteraction;
void menu_shared_controls_sync(struct SceneMenuInteraction *ctx);
void menu_shared_controls_cancel(struct SceneMenuInteraction *ctx);
bool menu_shared_controls_event(struct SceneMenuInteraction *ctx, const SDL_Event *event);
bool menu_shared_controls_contains(struct SceneMenuInteraction *ctx, int x, int y);
KitUiButtonState menu_shared_controls_state(const struct SceneMenuInteraction *ctx,
                                           const SDL_Rect *rect, bool selected);
#endif
