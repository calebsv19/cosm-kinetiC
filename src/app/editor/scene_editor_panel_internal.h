#ifndef SCENE_EDITOR_PANEL_INTERNAL_H
#define SCENE_EDITOR_PANEL_INTERNAL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "app/editor/scene_editor_internal.h"

extern SDL_Color COLOR_BG;
extern SDL_Color COLOR_PANEL;
extern SDL_Color COLOR_TEXT;
extern SDL_Color COLOR_TEXT_DIM;
extern SDL_Color COLOR_FIELD_ACTIVE;
extern SDL_Color COLOR_FIELD_BORDER;
extern SDL_Color COLOR_STATUS_OK;
extern SDL_Color COLOR_STATUS_WARN;
extern SDL_Color COLOR_STATUS_ERR;

SDL_Color lighten_color(SDL_Color color, float factor);
void refresh_panel_theme(void);
void draw_text(SDL_Renderer *renderer,
               TTF_Font *font,
               const char *text,
               int x,
               int y,
               SDL_Color color);
void fit_text_to_width(SDL_Renderer *renderer,
                       TTF_Font *font,
                       const char *text,
                       int max_width,
                       char *out,
                       size_t out_size);
void wrap_text_lines(SDL_Renderer *renderer,
                     TTF_Font *font,
                     const char *text,
                     int max_width,
                     int max_lines,
                     char out[][192],
                     int *out_count);
int panel_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback);

void draw_center_pane_summary(SceneEditorState *state);
void draw_right_panel_summary(SceneEditorState *state);

#endif // SCENE_EDITOR_PANEL_INTERNAL_H
