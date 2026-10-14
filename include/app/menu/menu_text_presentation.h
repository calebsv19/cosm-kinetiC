#ifndef PHYSICS_SIM_MENU_TEXT_PRESENTATION_H
#define PHYSICS_SIM_MENU_TEXT_PRESENTATION_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "kit_ui_text_presentation.h"
struct SceneMenuInteraction;
void menu_text_copy_utf8(char *, size_t, const char *);
CoreResult menu_text_fit_measured(const char *, float, char *, size_t, KitUiTextMeasure, void *);
CoreResult menu_text_label_build(KitUiTextPresentation *, char *, size_t, const SDL_Rect *, float, int, KitUiTextMeasure, void *);
void menu_text_fit(SDL_Renderer *, TTF_Font *, const char *, int, char *, size_t);
/* alignment: 0 left, 1 center, 2 right. Logical coordinates; enclosing clip retained. */
void menu_draw_contained_text(SDL_Renderer *, TTF_Font *, const char *, const SDL_Rect *, SDL_Color, int);
SDL_Rect menu_status_text_rect(const struct SceneMenuInteraction *, int, int, int);
void menu_draw_status_text(struct SceneMenuInteraction *, int, int);
#endif
