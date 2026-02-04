#include "app/structural/structural_render.h"

#include <math.h>

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static SDL_Color lerp_color(SDL_Color a, SDL_Color b, float t) {
    SDL_Color out;
    out.r = (Uint8)((float)a.r + ((float)b.r - (float)a.r) * t);
    out.g = (Uint8)((float)a.g + ((float)b.g - (float)a.g) * t);
    out.b = (Uint8)((float)a.b + ((float)b.b - (float)a.b) * t);
    out.a = (Uint8)((float)a.a + ((float)b.a - (float)a.a) * t);
    return out;
}

SDL_Color structural_render_color_bipolar(float value, float max_abs) {
    value = -value;
    SDL_Color neg = {220, 80, 60, 255};
    SDL_Color mid = {70, 200, 100, 255};
    SDL_Color pos = {60, 120, 220, 255};
    if (max_abs < 1e-6f) max_abs = 1.0f;
    float t = clampf(value / max_abs, -1.0f, 1.0f);
    if (t >= 0.0f) {
        return lerp_color(mid, pos, t);
    }
    return lerp_color(mid, neg, -t);
}

static float apply_gamma(float t, float gamma) {
    if (gamma <= 0.0f) return t;
    return powf(t, gamma);
}

SDL_Color structural_render_color_diverging(float value, float max_abs, float gamma) {
    SDL_Color neg = {59, 76, 192, 255};
    SDL_Color mid = {245, 245, 245, 255};
    SDL_Color pos = {180, 4, 38, 255};
    if (max_abs < 1e-6f) max_abs = 1.0f;
    float t = clampf(value / max_abs, -1.0f, 1.0f);
    float mag = apply_gamma(fabsf(t), gamma);
    if (t >= 0.0f) {
        return lerp_color(mid, pos, mag);
    }
    return lerp_color(mid, neg, mag);
}

SDL_Color structural_render_color_heat(float value, float max_value, float gamma) {
    SDL_Color c0 = {25, 25, 40, 255};
    SDL_Color c1 = {64, 120, 200, 255};
    SDL_Color c2 = {240, 210, 80, 255};
    SDL_Color c3 = {220, 60, 40, 255};
    if (max_value < 1e-6f) max_value = 1.0f;
    float t = clampf(value / max_value, 0.0f, 1.0f);
    t = apply_gamma(t, gamma);
    if (t <= 0.33f) {
        return lerp_color(c0, c1, t / 0.33f);
    }
    if (t <= 0.66f) {
        return lerp_color(c1, c2, (t - 0.33f) / 0.33f);
    }
    return lerp_color(c2, c3, (t - 0.66f) / 0.34f);
}

void structural_render_draw_thick_line(SDL_Renderer *renderer,
                                       float x0, float y0,
                                       float x1, float y1,
                                       float width) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-3f) {
        SDL_Rect dot = {(int)x0, (int)y0, 1, 1};
        SDL_RenderFillRect(renderer, &dot);
        return;
    }
    float nx = -dy / len;
    float ny = dx / len;
    int half = (int)(width * 0.5f);
    for (int i = -half; i <= half; ++i) {
        float ox = nx * (float)i;
        float oy = ny * (float)i;
        SDL_RenderDrawLine(renderer,
                           (int)(x0 + ox), (int)(y0 + oy),
                           (int)(x1 + ox), (int)(y1 + oy));
    }
}

void structural_render_draw_endcap(SDL_Renderer *renderer,
                                   float cx, float cy,
                                   float radius) {
    int r = (int)radius;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r * r) {
                SDL_Rect dot = {(int)cx + dx, (int)cy + dy, 1, 1};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}

void structural_render_draw_beam(SDL_Renderer *renderer,
                                 float x0, float y0,
                                 float x1, float y1,
                                 float width,
                                 SDL_Color c0,
                                 SDL_Color c1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    int segments = (int)fmaxf(1.0f, len / 6.0f);
    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i / (float)segments;
        float t1 = (float)(i + 1) / (float)segments;
        float ax = x0 + dx * t0;
        float ay = y0 + dy * t0;
        float bx = x0 + dx * t1;
        float by = y0 + dy * t1;
        SDL_Color c = lerp_color(c0, c1, (t0 + t1) * 0.5f);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        structural_render_draw_thick_line(renderer, ax, ay, bx, by, width);
    }

    SDL_SetRenderDrawColor(renderer, c0.r, c0.g, c0.b, c0.a);
    structural_render_draw_endcap(renderer, x0, y0, width * 0.6f);
    SDL_SetRenderDrawColor(renderer, c1.r, c1.g, c1.b, c1.a);
    structural_render_draw_endcap(renderer, x1, y1, width * 0.6f);
}
