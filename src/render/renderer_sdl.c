#include "render/renderer_sdl.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>

#define OBJECT_BORDER_THICKNESS 2

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static SDL_PixelFormat *g_format = NULL;
static TTF_Font *g_hud_font = NULL;
static bool g_ttf_initialized = false;
static const float DENSITY_VISUAL_SCALE = 0.05f;

static int g_window_w = 0;
static int g_window_h = 0;
static int g_grid_w   = 0;
static int g_grid_h   = 0;

static TTF_Font *load_hud_font(void) {
    const char *paths[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], 14);
        if (font) return font;
    }
    fprintf(stderr, "[renderer] Failed to load HUD font: %s\n", TTF_GetError());
    return NULL;
}

static void render_hud_text_line(const char *text,
                                 SDL_Texture **texture,
                                 SDL_Surface **surface,
                                 int *w,
                                 int *h) {
    if (!text || !texture || !surface || !w || !h) return;
    if (!g_hud_font || !g_renderer) {
        *texture = NULL;
        *surface = NULL;
        *w = *h = 0;
        return;
    }
    SDL_Color color = {240, 240, 240, 255};
    *surface = TTF_RenderUTF8_Blended(g_hud_font, text, color);
    if (!*surface) {
        *texture = NULL;
        *w = *h = 0;
        return;
    }
    *texture = SDL_CreateTextureFromSurface(g_renderer, *surface);
    if (!*texture) {
        SDL_FreeSurface(*surface);
        *surface = NULL;
        *w = *h = 0;
        return;
    }
    *w = (*surface)->w;
    *h = (*surface)->h;
}

static void renderer_draw_hud(const RendererHudInfo *hud) {
    if (!hud || !g_renderer || !g_hud_font) return;

    const char *preset_name = (hud->preset_name && hud->preset_name[0])
                                  ? hud->preset_name
                                  : "Preset";
    char preset_line[128];
    snprintf(preset_line, sizeof(preset_line),
             "Preset: %s (%s)",
             preset_name,
             hud->preset_is_custom ? "custom" : "built-in");
    char grid_line[64];
    snprintf(grid_line, sizeof(grid_line), "Grid: %dx%d", hud->grid_w, hud->grid_h);
    char emitter_line[64];
    snprintf(emitter_line, sizeof(emitter_line), "Emitters: %zu", hud->emitter_count);
    char brush_line[64];
    snprintf(brush_line, sizeof(brush_line), "Brush Queue: %zu", hud->stroke_samples);
    char status_line[32];
    snprintf(status_line, sizeof(status_line), "Status: %s", hud->paused ? "Paused" : "Running");
    const char *hint_line = "Esc to exit";

    const char *lines[] = {
        preset_line,
        grid_line,
        emitter_line,
        brush_line,
        status_line,
        hint_line
    };

    SDL_Surface *surfaces[6] = {0};
    SDL_Texture *textures[6] = {0};
    int widths[6] = {0};
    int heights[6] = {0};
    int count = 0;
    int max_w = 0;
    int total_h = 0;

    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        SDL_Texture *tex = NULL;
        SDL_Surface *surf = NULL;
        int w = 0, h = 0;
        render_hud_text_line(lines[i], &tex, &surf, &w, &h);
        if (!tex || !surf) continue;
        textures[count] = tex;
        surfaces[count] = surf;
        widths[count] = w;
        heights[count] = h;
        if (w > max_w) max_w = w;
        total_h += h;
        ++count;
    }

    if (count == 0) {
        return;
    }

    const int padding = 8;
    const int spacing = 2;
    total_h += spacing * (count - 1);

    SDL_Rect panel = {
        .x = 12,
        .y = 12,
        .w = max_w + padding * 2,
        .h = total_h + padding * 2
    };

    SDL_SetRenderDrawColor(g_renderer, 30, 35, 40, 30);
    SDL_RenderFillRect(g_renderer, &panel);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 60);
    SDL_RenderDrawRect(g_renderer, &panel);

    int y = panel.y + padding;
    for (int i = 0; i < count; ++i) {
        SDL_Rect dst = {panel.x + padding, y, widths[i], heights[i]};
        SDL_RenderCopy(g_renderer, textures[i], NULL, &dst);
        y += heights[i] + spacing;
    }

    for (int i = 0; i < count; ++i) {
        if (textures[i]) SDL_DestroyTexture(textures[i]);
        if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
    }
}

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH) {
    g_window_w = windowW;
    g_window_h = windowH;
    g_grid_w   = gridW;
    g_grid_h   = gridH;

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    g_ttf_initialized = true;

    g_window = SDL_CreateWindow(
        "Physics Sim - Fluid2D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowW, windowH,
        SDL_WINDOW_SHOWN
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return false;
    }

    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        gridW, gridH
    );
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        g_renderer = NULL;
        g_window = NULL;
        return false;
    }

    g_format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    if (!g_format) {
        fprintf(stderr, "SDL_AllocFormat failed: %s\n", SDL_GetError());
        renderer_sdl_shutdown();
        return false;
    }

    g_hud_font = load_hud_font();
    if (!g_hud_font) {
        // degrade gracefully; HUD rendering will be disabled
        fprintf(stderr, "[renderer] HUD font unavailable, HUD disabled.\n");
    }

    return true;
}

void renderer_sdl_shutdown(void) {
    if (g_hud_font) {
        TTF_CloseFont(g_hud_font);
        g_hud_font = NULL;
    }
    if (g_format) {
        SDL_FreeFormat(g_format);
        g_format = NULL;
    }
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    if (g_ttf_initialized) {
        TTF_Quit();
        g_ttf_initialized = false;
    }
}

static void renderer_draw_object_borders(const SceneState *scene);

static bool renderer_upload_scene(const SceneState *scene) {
    if (!scene || !scene->smoke || !g_renderer || !g_texture) return false;
    int tex_pitch = 0;
    void *pixels = NULL;
    if (SDL_LockTexture(g_texture, NULL, &pixels, &tex_pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return false;
    }

    // tex_pitch is in bytes per row
    int w = scene->smoke->w;
    int h = scene->smoke->h;

    for (int y = 0; y < h; ++y) {
        Uint32 *dst = (Uint32 *)((Uint8 *)pixels + y * tex_pitch);
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * (size_t)w + (size_t)x;
            float d = scene->smoke->density[i];
            float norm = d * DENSITY_VISUAL_SCALE;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;

            Uint8 c = (Uint8)(norm * 255.0f);
            Uint32 pixel = g_format
                ? SDL_MapRGBA(g_format, c, c, c, 255)
                : (Uint32)(0xFFu << 24 | (c << 16) | (c << 8) | c);
            dst[x] = pixel;
        }
    }

    SDL_UnlockTexture(g_texture);

    return true;
}

bool renderer_sdl_render_scene(const SceneState *scene) {
    if (!renderer_upload_scene(scene)) return false;

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    SDL_Rect dst_rect = {0, 0, g_window_w, g_window_h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst_rect);
    renderer_draw_object_borders(scene);
    return true;
}

void renderer_sdl_present_with_hud(const RendererHudInfo *hud) {
    renderer_draw_hud(hud);
    SDL_RenderPresent(g_renderer);
}

bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch) {
    if (!out_rgba || !g_renderer) return false;
    int pitch = g_window_w * 4;
    uint8_t *buffer = (uint8_t *)malloc((size_t)pitch * (size_t)g_window_h);
    if (!buffer) return false;
    if (SDL_RenderReadPixels(g_renderer,
                             NULL,
                             SDL_PIXELFORMAT_ABGR8888,
                             buffer,
                             pitch) != 0) {
        fprintf(stderr, "[renderer] SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        free(buffer);
        return false;
    }
    *out_rgba = buffer;
    if (out_pitch) *out_pitch = pitch;
    return true;
}

void renderer_sdl_free_capture(uint8_t *pixels) {
    free(pixels);
}

int renderer_sdl_output_width(void) {
    return g_window_w;
}

int renderer_sdl_output_height(void) {
    return g_window_h;
}
static void renderer_draw_object_borders(const SceneState *scene) {
#if OBJECT_BORDER_THICKNESS <= 0
    (void)scene;
    return;
#else
    if (!scene || !scene->config || !g_renderer) return;
    const ObjectManager *objects = &scene->objects;
    if (!objects || objects->count == 0) return;

    float scale_x = (scene->config->window_w > 0)
                        ? (float)g_window_w / (float)scene->config->window_w
                        : 1.0f;
    float scale_y = (scene->config->window_h > 0)
                        ? (float)g_window_h / (float)scene->config->window_h
                        : 1.0f;

    SDL_Color circle_color = {255, 80, 80, 255};
    SDL_Color box_color    = {170, 120, 80, 255};
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    const int segments = 48;
    for (int i = 0; i < objects->count; ++i) {
        const SceneObject *obj = &objects->objects[i];
        int cx = (int)lroundf(obj->body.position.x * scale_x);
        int cy = (int)lroundf(obj->body.position.y * scale_y);
        if (obj->type == SCENE_OBJECT_CIRCLE) {
            int radius = (int)lroundf(obj->body.radius * scale_x);
            if (radius < 2) radius = 2;
            SDL_SetRenderDrawColor(g_renderer,
                                   circle_color.r,
                                   circle_color.g,
                                   circle_color.b,
                                   circle_color.a);
            for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
                int r = radius - t;
                if (r <= 0) break;
                float prev_x = (float)(cx + r);
                float prev_y = (float)cy;
                for (int seg = 1; seg <= segments; ++seg) {
                    float theta = (float)seg / (float)segments * 2.0f * (float)M_PI;
                    float cur_x = (float)cx + cosf(theta) * (float)r;
                    float cur_y = (float)cy + sinf(theta) * (float)r;
                    SDL_RenderDrawLine(g_renderer,
                                       (int)prev_x,
                                       (int)prev_y,
                                       (int)cur_x,
                                       (int)cur_y);
                    prev_x = cur_x;
                    prev_y = cur_y;
                }
            }
        } else {
            int half_w = (int)lroundf(obj->body.half_extents.x * scale_x);
            int half_h = (int)lroundf(obj->body.half_extents.y * scale_y);
            if (half_w < 2) half_w = 2;
            if (half_h < 2) half_h = 2;
            SDL_SetRenderDrawColor(g_renderer,
                                   box_color.r,
                                   box_color.g,
                                   box_color.b,
                                   box_color.a);
            for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
                SDL_Rect rect = {
                    cx - half_w + t,
                    cy - half_h + t,
                    (half_w - t) * 2,
                    (half_h - t) * 2
                };
                if (rect.w <= 0 || rect.h <= 0) break;
                SDL_RenderDrawRect(g_renderer, &rect);
            }
        }
    }
#endif
}
