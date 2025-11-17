#include "render/renderer_sdl.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <string.h>
#include "render/TimerHUD/src/api/time_scope.h"

#define OBJECT_BORDER_THICKNESS 2

// Toggle smoothing of the fluid texture before it is upscaled to the window.
// Set to 0 to disable the blur pass when visualizing the grid.
#define RENDERER_ENABLE_SMOOTHING 1

// Toggle bilinear filtering when SDL scales the fluid texture to the window.
// Disabling this reverts to nearest-neighbor pixel doubling.
#define RENDERER_ENABLE_LINEAR_FILTER 1

#if RENDERER_ENABLE_SMOOTHING
static const float RENDERER_SMOOTH_KERNEL_1D[3] = {1.0f, 2.0f, 1.0f};
static const float RENDERER_SMOOTH_KERNEL_1D_SUM = 4.0f;
#endif

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

static float *g_density_tmp = NULL;
static float *g_density_blur = NULL;
static size_t g_density_capacity = 0;
static float *g_vorticity_tmp = NULL;
static float *g_vorticity_blur = NULL;
static size_t g_vorticity_capacity = 0;
static float *g_pressure_tmp = NULL;
static float *g_pressure_blur = NULL;
static size_t g_pressure_capacity = 0;
static bool g_draw_vorticity = false;
static bool g_draw_pressure = false;

static void renderer_apply_overlays(const SceneState *scene);
static void renderer_apply_vorticity_overlay(const SceneState *scene,
                                             Uint8 *pixels,
                                             int pitch);
static void renderer_apply_pressure_overlay(const SceneState *scene,
                                            Uint8 *pixels,
                                            int pitch);
static inline void blend_pixel(Uint32 *dst,
                               Uint8 r,
                               Uint8 g,
                               Uint8 b,
                               Uint8 alpha);

static bool ensure_density_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_density_capacity) return true;
    float *tmp = (float *)realloc(g_density_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_density_tmp = tmp;
    float *blur = (float *)realloc(g_density_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_density_blur = blur;
    g_density_capacity = cell_count;
    return true;
}

static bool ensure_vorticity_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_vorticity_capacity) return true;
    float *tmp = (float *)realloc(g_vorticity_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_vorticity_tmp = tmp;
    float *blur = (float *)realloc(g_vorticity_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_vorticity_blur = blur;
    g_vorticity_capacity = cell_count;
    return true;
}

static bool ensure_pressure_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_pressure_capacity) return true;
    float *tmp = (float *)realloc(g_pressure_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_pressure_tmp = tmp;
    float *blur = (float *)realloc(g_pressure_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_pressure_blur = blur;
    g_pressure_capacity = cell_count;
    return true;
}

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline void blend_pixel(Uint32 *dst,
                               Uint8 r,
                               Uint8 g,
                               Uint8 b,
                               Uint8 alpha) {
    if (!dst || alpha == 0) return;
    Uint8 base_r = 0, base_g = 0, base_b = 0, base_a = 0;
    SDL_GetRGBA(*dst, g_format, &base_r, &base_g, &base_b, &base_a);
    base_r = (Uint8)((r * alpha + base_r * (255 - alpha)) / 255);
    base_g = (Uint8)((g * alpha + base_g * (255 - alpha)) / 255);
    base_b = (Uint8)((b * alpha + base_b * (255 - alpha)) / 255);
    *dst = SDL_MapRGBA(g_format, base_r, base_g, base_b, base_a ? base_a : 255);
}

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
    char window_line[64];
    snprintf(window_line, sizeof(window_line), "Window: %dx%d", hud->window_w, hud->window_h);
    char mode_line[64];
    if (hud->sim_mode == SIM_MODE_WIND_TUNNEL) {
        snprintf(mode_line, sizeof(mode_line),
                 "Mode: Wind (inflow %.1f)",
                 hud->tunnel_inflow_speed);
    } else {
        snprintf(mode_line, sizeof(mode_line), "Mode: Box");
    }
    char emitter_line[64];
    snprintf(emitter_line, sizeof(emitter_line), "Emitters: %zu", hud->emitter_count);
    char brush_line[64];
    snprintf(brush_line, sizeof(brush_line), "Brush Queue: %zu", hud->stroke_samples);
    char quality_line[64];
    snprintf(quality_line, sizeof(quality_line), "Quality: %s",
             (hud->quality_name && hud->quality_name[0]) ? hud->quality_name : "Custom");
    char solver_line[64];
    snprintf(solver_line, sizeof(solver_line),
             "Solver: iter %d, substeps %d",
             hud->solver_iterations,
             hud->physics_substeps);
    char vorticity_line[48];
    snprintf(vorticity_line, sizeof(vorticity_line),
             "Vorticity Overlay: %s",
             hud->vorticity_enabled ? "On" : "Off");
    char pressure_line[48];
    snprintf(pressure_line, sizeof(pressure_line),
             "Pressure Overlay: %s",
             hud->pressure_enabled ? "On" : "Off");
    char status_line[32];
    snprintf(status_line, sizeof(status_line), "Status: %s", hud->paused ? "Paused" : "Running");
    const char *hint_line = "Esc exit | P pause | C clear | E snapshot | V vorticity | B pressure";

    enum { MAX_HUD_LINES = 10 };
    const char *lines[MAX_HUD_LINES];
    size_t line_count = 0;
    lines[line_count++] = preset_line;
    lines[line_count++] = grid_line;
    lines[line_count++] = window_line;
    lines[line_count++] = mode_line;
    lines[line_count++] = quality_line;
    lines[line_count++] = solver_line;
    lines[line_count++] = emitter_line;
    lines[line_count++] = brush_line;
    lines[line_count++] = vorticity_line;
    lines[line_count++] = pressure_line;
    lines[line_count++] = status_line;
    lines[line_count++] = hint_line;

    SDL_Surface *surfaces[MAX_HUD_LINES];
    SDL_Texture *textures[MAX_HUD_LINES];
    int widths[MAX_HUD_LINES];
    int heights[MAX_HUD_LINES];
    for (int i = 0; i < MAX_HUD_LINES; ++i) {
        surfaces[i] = NULL;
        textures[i] = NULL;
        widths[i] = 0;
        heights[i] = 0;
    }
    int count = 0;
    int max_w = 0;
    int total_h = 0;

    for (size_t i = 0; i < line_count; ++i) {
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

#if RENDERER_ENABLE_LINEAR_FILTER
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#endif

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
    free(g_density_tmp);
    g_density_tmp = NULL;
    free(g_density_blur);
    g_density_blur = NULL;
    g_density_capacity = 0;
    free(g_vorticity_tmp);
    g_vorticity_tmp = NULL;
    free(g_vorticity_blur);
    g_vorticity_blur = NULL;
    g_vorticity_capacity = 0;
    free(g_pressure_tmp);
    g_pressure_tmp = NULL;
    free(g_pressure_blur);
    g_pressure_blur = NULL;
    g_pressure_capacity = 0;
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

static float renderer_safe_scale(int target, int source) {
    if (source <= 0) return 1.0f;
    if (target <= 0) return 1.0f;
    return (float)target / (float)source;
}

static void renderer_draw_rotated_box_outline(const SceneObject *obj,
                                              float scale_x,
                                              float scale_y,
                                              SDL_Color color) {
    if (!obj || !g_renderer) return;
    float cos_a = cosf(obj->body.angle);
    float sin_a = sinf(obj->body.angle);
    SDL_SetRenderDrawColor(g_renderer,
                           color.r,
                           color.g,
                           color.b,
                           color.a);

    for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
        float shrink_x = (scale_x > 0.0f) ? (float)t / scale_x : 0.0f;
        float shrink_y = (scale_y > 0.0f) ? (float)t / scale_y : 0.0f;
        float hx = obj->body.half_extents.x - shrink_x;
        float hy = obj->body.half_extents.y - shrink_y;
        if (hx <= 0.0f || hy <= 0.0f) break;

        SDL_Point pts[5];
        const float corner_x[4] = {-hx,  hx,  hx, -hx};
        const float corner_y[4] = {-hy, -hy,  hy,  hy};
        for (int i = 0; i < 4; ++i) {
            float lx = corner_x[i];
            float ly = corner_y[i];
            float rx = lx * cos_a - ly * sin_a;
            float ry = lx * sin_a + ly * cos_a;
            float world_x = obj->body.position.x + rx;
            float world_y = obj->body.position.y + ry;
            pts[i].x = (int)lroundf(world_x * scale_x);
            pts[i].y = (int)lroundf(world_y * scale_y);
        }
        pts[4] = pts[0];
        SDL_RenderDrawLines(g_renderer, pts, 5);
    }
}

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
    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_density_buffers(cell_count)) {
        SDL_UnlockTexture(g_texture);
        return false;
    }

    for (size_t i = 0; i < cell_count; ++i) {
        float norm = scene->smoke->density[i] * DENSITY_VISUAL_SCALE;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        g_density_tmp[i] = norm;
    }

    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    accum += g_density_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_density_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    accum += g_density_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_density_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    for (int y = 0; y < h; ++y) {
        Uint32 *dst = (Uint32 *)((Uint8 *)pixels + y * tex_pitch);
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * (size_t)w + (size_t)x;
            float norm = g_density_tmp[i];
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
    renderer_apply_overlays(scene);

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    SDL_Rect dst_rect = {0, 0, g_window_w, g_window_h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst_rect);
    renderer_draw_object_borders(scene);
    return true;
}

void renderer_sdl_present_with_hud(const RendererHudInfo *hud) {
    renderer_draw_hud(hud);
    ts_render(g_renderer);
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

static void renderer_apply_vorticity_overlay(const SceneState *scene,
                                             Uint8 *pixels,
                                             int pitch) {
    if (!scene || !scene->smoke || !pixels) return;
    const Fluid2D *grid = scene->smoke;
    int w = grid->w;
    int h = grid->h;
    if (w < 3 || h < 3) return;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_vorticity_buffers(cell_count)) return;

    // 1) Compute raw vorticity (centered differences) in interior
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;

            float dvy_dx = (grid->velY[id + 1]     - grid->velY[id - 1]) * 0.5f;
            float dvx_dy = (grid->velX[id + w]     - grid->velX[id - w]) * 0.5f;
            float vort   = dvy_dx - dvx_dy;

            g_vorticity_tmp[id] = vort;
        }
    }

    // Zero borders (we don't have valid centered differences there)
    for (int x = 0; x < w; ++x) {
        g_vorticity_tmp[x] = 0.0f;
        g_vorticity_tmp[(size_t)(h - 1) * (size_t)w + (size_t)x] = 0.0f;
    }
    for (int y = 0; y < h; ++y) {
        g_vorticity_tmp[(size_t)y * (size_t)w]               = 0.0f;
        g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)(w - 1)] = 0.0f;
    }

    // 2) Optional blur to make vort filaments softer
    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        // Horizontal blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0)    sx = 0;
                    if (sx >= w)   sx = w - 1;
                    accum += g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_vorticity_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        // Vertical blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0)    sy = 0;
                    if (sy >= h)   sy = h - 1;
                    accum += g_vorticity_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    // 3) Find symmetric range for vorticity
    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;
    for (size_t i = 0; i < cell_count; ++i) {
        float v = g_vorticity_tmp[i];
        if (!isfinite(v)) continue;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }
    if (!isfinite(min_v) || !isfinite(max_v)) {
        min_v = -1.0f;
        max_v =  1.0f;
    }
    float v_range = fmaxf(fabsf(min_v), fabsf(max_v));
    if (v_range < 1e-6f) v_range = 1.0f;
    v_range *= 1.5f;

    for (int y = 0; y < h; ++y) {
        Uint32 *row = (Uint32 *)(pixels + y * pitch);
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float vort = g_vorticity_tmp[id];
            float norm = vort / v_range;
            if (!isfinite(norm)) norm = 0.0f;
            if (norm >  1.0f) norm =  1.0f;
            if (norm < -1.0f) norm = -1.0f;

            float magnitude = fabsf(norm);
            magnitude = powf(magnitude, 1.1f);
            if (magnitude > 1.0f) magnitude = 1.0f;

            const float base = 0.12f;
            float r = base;
            float g = base;
            float b = base;
            if (norm >= 0.0f) {
                // Make positive vorticity a vivid orange/yellow
                r += 0.95f * magnitude;
                g += 0.40f * magnitude;
                b *= (1.0f - 0.8f * magnitude);
            } else {
                // Negative vorticity becomes a saturated purple
                r += 0.55f * magnitude;
                b += 1.05f * magnitude;
                g *= (1.0f - 0.75f * magnitude);
            }

            r = clamp01(r);
            g = clamp01(g);
            b = clamp01(b);

            Uint8 overlay_r = (Uint8)lroundf(r * 255.0f);
            Uint8 overlay_g = (Uint8)lroundf(g * 255.0f);
            Uint8 overlay_b = (Uint8)lroundf(b * 255.0f);
            Uint8 alpha = (Uint8)lroundf(60.0f + 195.0f * magnitude);
            blend_pixel(&row[x], overlay_r, overlay_g, overlay_b, alpha);
        }
    }
}


static void renderer_apply_pressure_overlay(const SceneState *scene,
                                            Uint8 *pixels,
                                            int pitch) {
    if (!scene || !scene->smoke || !scene->smoke->pressure || !pixels) return;
    const Fluid2D *grid = scene->smoke;
    int w = grid->w;
    int h = grid->h;
    if (w < 2 || h < 2) return;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_pressure_buffers(cell_count)) return;

    // 1) Find symmetric min/max so 0 = mid, +/-max = full red/blue.
    float min_p = FLT_MAX;
    float max_p = -FLT_MAX;
    for (size_t i = 0; i < cell_count; ++i) {
        float p = grid->pressure[i];
        if (!isfinite(p)) continue;
        if (p < min_p) min_p = p;
        if (p > max_p) max_p = p;
    }
    if (!isfinite(min_p) || !isfinite(max_p)) {
        min_p = -1.0f;
        max_p =  1.0f;
    }
    float range = fmaxf(fabsf(min_p), fabsf(max_p));
    if (range < 1e-6f) range = 1.0f;
    const float exaggerate = 1.1f;
    range *= exaggerate;

    // Normalize to [-1,1]
    for (size_t i = 0; i < cell_count; ++i) {
        float norm = grid->pressure[i] / range;
        if (!isfinite(norm)) norm = 0.0f;
        if (norm >  1.0f) norm =  1.0f;
        if (norm < -1.0f) norm = -1.0f;
        g_pressure_tmp[i] = norm;
    }

    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        // Horizontal blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0)    sx = 0;
                    if (sx >= w)   sx = w - 1;
                    accum += g_pressure_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_pressure_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        // Vertical blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0)    sy = 0;
                    if (sy >= h)   sy = h - 1;
                    accum += g_pressure_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_pressure_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    const Uint8 base_gray = 200;
    for (int y = 0; y < h; ++y) {
        Uint32 *row = (Uint32 *)(pixels + y * pitch);
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float value = g_pressure_tmp[id];
            float magnitude = fabsf(value);
            magnitude = powf(magnitude, 1.05f);
            if (magnitude > 1.0f) magnitude = 1.0f;

            Uint8 overlay_r = base_gray;
            Uint8 overlay_g = base_gray;
            Uint8 overlay_b = base_gray;
            if (value >= 0.0f) {
                overlay_r = (Uint8)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_g = (Uint8)lroundf((float)base_gray * (1.0f - magnitude));
                overlay_b = overlay_g;
            } else {
                overlay_b = (Uint8)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_r = (Uint8)lroundf((float)base_gray * (1.0f - magnitude));
                overlay_g = overlay_r;
            }
            Uint8 alpha = (Uint8)lroundf(40.0f + 140.0f * magnitude);
            blend_pixel(&row[x], overlay_r, overlay_g, overlay_b, alpha);
        }
    }
}


bool renderer_sdl_toggle_vorticity(void) {
    g_draw_vorticity = !g_draw_vorticity;
    return g_draw_vorticity;
}

bool renderer_sdl_vorticity_enabled(void) {
    return g_draw_vorticity;
}

bool renderer_sdl_toggle_pressure(void) {
    g_draw_pressure = !g_draw_pressure;
    return g_draw_pressure;
}

bool renderer_sdl_pressure_enabled(void) {
    return g_draw_pressure;
}

static void renderer_apply_overlays(const SceneState *scene) {
    if (!scene || !scene->smoke || !g_texture) return;
    if (!g_draw_pressure && !g_draw_vorticity) return;
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(g_texture, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "[renderer] Failed to lock scene texture: %s\n", SDL_GetError());
        return;
    }
    Uint8 *base = (Uint8 *)pixels;
    if (g_draw_pressure) {
        renderer_apply_pressure_overlay(scene, base, pitch);
    }
    if (g_draw_vorticity) {
        renderer_apply_vorticity_overlay(scene, base, pitch);
    }
    SDL_UnlockTexture(g_texture);
}
static void renderer_draw_object_borders(const SceneState *scene) {
#if OBJECT_BORDER_THICKNESS <= 0
    (void)scene;
    return;
#else
    if (!scene || !scene->config || !g_renderer) return;
    const ObjectManager *objects = &scene->objects;
    if (!objects || objects->count == 0) return;

    float scale_x = renderer_safe_scale(g_window_w, scene->config->window_w);
    float scale_y = renderer_safe_scale(g_window_h, scene->config->window_h);

    SDL_Color circle_color = {255, 80, 80, 255};
    SDL_Color box_color    = {170, 120, 80, 255};
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    const int segments = 48;
    for (int i = 0; i < objects->count; ++i) {
        const SceneObject *obj = &objects->objects[i];
        int cx = (int)lroundf(obj->body.position.x * scale_x);
        int cy = (int)lroundf(obj->body.position.y * scale_y);
        if (obj->type == SCENE_OBJECT_CIRCLE) {
            float radius_scale = (scale_x + scale_y) * 0.5f;
            if (radius_scale <= 0.0f) radius_scale = scale_x > 0.0f ? scale_x : 1.0f;
            int radius = (int)lroundf(obj->body.radius * radius_scale);
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
            renderer_draw_rotated_box_outline(obj, scale_x, scale_y, box_color);
        }
    }
#endif
}
