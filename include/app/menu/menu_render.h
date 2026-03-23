#ifndef MENU_RENDER_H
#define MENU_RENDER_H

#include "app/menu/menu_types.h"

typedef struct MenuThemePalette {
    SDL_Color background;
    SDL_Color panel;
    SDL_Color text;
    SDL_Color text_dim;
    SDL_Color accent;
    SDL_Color button_bg;
    SDL_Color button_bg_active;
} MenuThemePalette;

void menu_set_theme_palette(const MenuThemePalette *palette);

SDL_Rect menu_preset_list_rect(void);
void menu_update_scrollbar(SceneMenuInteraction *ctx);

void menu_draw_text(SDL_Renderer *renderer,
                    TTF_Font *font,
                    const char *text,
                    int x,
                    int y,
                    SDL_Color color);

void menu_draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect);

void menu_draw_button(SDL_Renderer *renderer,
                      const SDL_Rect *rect,
                      const char *label,
                      TTF_Font *font,
                      bool selected);

void menu_draw_text_input(SDL_Renderer *renderer,
                          TTF_Font *font,
                          const SDL_Rect *rect,
                          const TextInputField *field);

void menu_draw_toggle(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const SDL_Rect *rect,
                      const char *label,
                      bool enabled);

void menu_draw_preset_list(SceneMenuInteraction *ctx);

SDL_Color menu_color_bg(void);
SDL_Color menu_color_panel(void);
SDL_Color menu_color_text(void);
SDL_Color menu_color_text_dim(void);
SDL_Color menu_color_accent(void);

#endif
