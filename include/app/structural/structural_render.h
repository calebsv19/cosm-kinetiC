#ifndef STRUCTURAL_RENDER_H
#define STRUCTURAL_RENDER_H

#include <SDL2/SDL.h>

SDL_Color structural_render_color_bipolar(float value, float max_abs);
SDL_Color structural_render_color_diverging(float value, float max_abs, float gamma);
SDL_Color structural_render_color_heat(float value, float max_value, float gamma);

void structural_render_draw_thick_line(SDL_Renderer *renderer,
                                       float x0, float y0,
                                       float x1, float y1,
                                       float width);

void structural_render_draw_endcap(SDL_Renderer *renderer,
                                   float cx, float cy,
                                   float radius);

void structural_render_draw_beam(SDL_Renderer *renderer,
                                 float x0, float y0,
                                 float x1, float y1,
                                 float width,
                                 SDL_Color c0,
                                 SDL_Color c1);

#endif
