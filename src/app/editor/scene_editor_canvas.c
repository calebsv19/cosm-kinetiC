#include "app/editor/scene_editor_canvas.h"

#include <math.h>
#include <string.h>

static SDL_Color COLOR_CANVAS    = {12, 14, 18, 255};
static SDL_Color COLOR_SOURCE    = {252, 163, 17, 255};
static SDL_Color COLOR_JET       = {64, 201, 255, 255};
static SDL_Color COLOR_SINK      = {200, 80, 255, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_SELECTED  = {255, 255, 255, 255};

static SDL_Color emitter_color(const FluidEmitter *em) {
    switch (em->type) {
    case EMITTER_DENSITY_SOURCE: return COLOR_SOURCE;
    case EMITTER_VELOCITY_JET:   return COLOR_JET;
    case EMITTER_SINK:           return COLOR_SINK;
    default:                     return COLOR_JET;
    }
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

void scene_editor_canvas_project(int canvas_x,
                                 int canvas_y,
                                 int canvas_size,
                                 float px,
                                 float py,
                                 int *out_x,
                                 int *out_y) {
    if (!out_x || !out_y) return;
    *out_x = canvas_x + (int)lroundf(px * (float)canvas_size);
    *out_y = canvas_y + (int)lroundf(py * (float)canvas_size);
}

void scene_editor_canvas_to_normalized(int canvas_x,
                                       int canvas_y,
                                       int canvas_size,
                                       int sx,
                                       int sy,
                                       float *out_x,
                                       float *out_y) {
    if (!out_x || !out_y) return;
    float nx = (float)(sx - canvas_x) / (float)canvas_size;
    float ny = (float)(sy - canvas_y) / (float)canvas_size;
    if (nx < 0.0f) nx = 0.0f;
    if (nx > 1.0f) nx = 1.0f;
    if (ny < 0.0f) ny = 0.0f;
    if (ny > 1.0f) ny = 1.0f;
    *out_x = nx;
    *out_y = ny;
}

int scene_editor_canvas_hit_test(const FluidScenePreset *preset,
                                 int canvas_x,
                                 int canvas_y,
                                 int canvas_size,
                                 int px,
                                 int py,
                                 EditorDragMode *mode) {
    if (!preset) return -1;
    int closest = -1;
    float best_dist = 1e9f;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_size,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(em->radius * canvas_size);
        if (radius_px < 4) radius_px = 4;
        float dx = (float)px - (float)cx;
        float dy = (float)py - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= (float)radius_px) {
            if (dist < best_dist) {
                closest = (int)i;
                best_dist = dist;
                if (mode) *mode = DRAG_POSITION;
            }
        } else if (em->type != EMITTER_DENSITY_SOURCE) {
            int arrow_len = radius_px + 40;
            int hx = cx + (int)(em->dir_x * arrow_len);
            int hy = cy + (int)(em->dir_y * arrow_len);
            float adx = (float)px - (float)hx;
            float ady = (float)py - (float)hy;
            float adist = sqrtf(adx * adx + ady * ady);
            if (adist <= 18.0f && adist < best_dist) {
                closest = (int)i;
                best_dist = adist;
                if (mode) *mode = DRAG_DIRECTION;
            }
        }
    }
    return closest;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

void scene_editor_canvas_draw_emitters(SDL_Renderer *renderer,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_size,
                                       const FluidScenePreset *preset,
                                       int selected_emitter,
                                       int hover_emitter,
                                       TTF_Font *font_small) {
    if (!renderer || !preset) return;

    SDL_Rect canvas_rect = {canvas_x, canvas_y, canvas_size, canvas_size};
    SDL_SetRenderDrawColor(renderer, COLOR_CANVAS.r, COLOR_CANVAS.g, COLOR_CANVAS.b, 255);
    SDL_RenderFillRect(renderer, &canvas_rect);

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_size,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(em->radius * canvas_size);
        if (radius_px < 4) radius_px = 4;

        SDL_Color color = emitter_color(em);
        if ((int)i == selected_emitter) {
            SDL_Color outline = lighten_color(color, SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR);
            draw_circle(renderer, cx, cy, radius_px + 3, outline);
            color = outline;
        }
        draw_circle(renderer, cx, cy, radius_px, color);

        if (em->type != EMITTER_DENSITY_SOURCE) {
            int arrow_len = radius_px + 40;
            int hx = cx + (int)(em->dir_x * arrow_len);
            int hy = cy + (int)(em->dir_y * arrow_len);
            draw_line(renderer, cx, cy, hx, hy, COLOR_SELECTED);
            draw_circle(renderer, hx, hy, 5, COLOR_SELECTED);
        }

        if ((int)i == hover_emitter && font_small) {
            char tooltip_pos[64];
            snprintf(tooltip_pos, sizeof(tooltip_pos), "Radius: %.2f", em->radius);
            (void)tooltip_pos;
            // Canvas-specific tooltip drawing can be added later if needed.
        }
    }
}

void scene_editor_canvas_draw_name(SDL_Renderer *renderer,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_size,
                                   TTF_Font *font_main,
                                   TTF_Font *font_small,
                                   const char *name,
                                   bool renaming,
                                   const TextInputField *input) {
    if (!renderer) return;
    SDL_Rect rect = {
        .x = canvas_x,
        .y = canvas_y - 50,
        .w = canvas_size,
        .h = 36
    };
    if (rect.y < 20) rect.y = 20;

    if (renaming && input && font_main) {
        SDL_SetRenderDrawColor(renderer, COLOR_CANVAS.r, COLOR_CANVAS.g, COLOR_CANVAS.b, 240);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 255);
        SDL_RenderDrawRect(renderer, &rect);
        const char *text = text_input_value(input);
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_main, text, COLOR_TEXT);
        int text_w = 0;
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            text_w = surf->w;
            SDL_Rect dst = {rect.x + 8, rect.y + rect.h / 2 - surf->h / 2,
                            surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        if (input->caret_visible) {
            int caret_x = rect.x + 8 + text_w + 2;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, caret_x, rect.y + 6,
                               caret_x, rect.y + rect.h - 6);
        }
    } else if (font_main) {
        const char *title = (name && name[0]) ? name : "Untitled Preset";
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_main, title, COLOR_TEXT);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {rect.x, rect.y, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }
    (void)font_small;
}
