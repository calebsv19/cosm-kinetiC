#include "render/hud_overlay.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "font_paths.h"

static SDL_Renderer *g_hud_renderer = NULL;
static TTF_Font     *g_hud_font     = NULL;

static TTF_Font *load_hud_font(void) {
    const char *paths[] = {
        FONT_BODY_PATH_1,
        FONT_BODY_PATH_2,
        FONT_TITLE_PATH_1,  // extra fallbacks if body fonts fail
        FONT_TITLE_PATH_2
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], 14);
        if (font) {
            return font;
        }
    }

    fprintf(stderr, "[hud] Failed to load HUD font: %s\n", TTF_GetError());
    return NULL;
}

bool hud_overlay_init(SDL_Renderer *renderer) {
    g_hud_renderer = renderer;
    g_hud_font = load_hud_font();
    if (!g_hud_font) {
        fprintf(stderr, "[hud] HUD font unavailable, HUD disabled.\n");
        return false;
    }
    return true;
}

void hud_overlay_shutdown(void) {
    if (g_hud_font) {
        TTF_CloseFont(g_hud_font);
        g_hud_font = NULL;
    }
    g_hud_renderer = NULL;
}

static void render_hud_text_line(const char *text,
                                 SDL_Texture **texture,
                                 SDL_Surface **surface,
                                 int *w,
                                 int *h) {
    if (!text || !texture || !surface || !w || !h) return;
    if (!g_hud_font || !g_hud_renderer) {
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
    *texture = SDL_CreateTextureFromSurface(g_hud_renderer, *surface);
    if (!*texture) {
        SDL_FreeSurface(*surface);
        *surface = NULL;
        *w = *h = 0;
        return;
    }
    *w = (*surface)->w;
    *h = (*surface)->h;
}

void hud_overlay_draw(const RendererHudInfo *hud) {
    if (!hud || !g_hud_renderer || !g_hud_font) return;

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
    char quality_line[64];
    snprintf(quality_line, sizeof(quality_line), "Quality: %s",
             (hud->quality_name && hud->quality_name[0]) ? hud->quality_name : "Custom");
    char solver_line[64];
    snprintf(solver_line, sizeof(solver_line),
             "Solver: iter %d, substeps %d",
             hud->solver_iterations,
             hud->physics_substeps);
    char vorticity_line[64];
    snprintf(vorticity_line, sizeof(vorticity_line),
             "Vorticity (V): %s",
             hud->vorticity_enabled ? "On" : "Off");
    char pressure_line[64];
    snprintf(pressure_line, sizeof(pressure_line),
             "Pressure (B): %s",
             hud->pressure_enabled ? "On" : "Off");
    char velocity_line[64];
    snprintf(velocity_line, sizeof(velocity_line),
             "Velocity (S / Shift+S mode): %s (%s)",
             hud->velocity_overlay_enabled ? "On" : "Off",
             hud->velocity_fixed_length ? "Fixed" : "Magnitude");
    char particles_line[64];
    snprintf(particles_line, sizeof(particles_line),
             "Particles (L): %s",
             hud->particle_overlay_enabled ? "On" : "Off");
    char gravity_line[64];
    snprintf(gravity_line, sizeof(gravity_line),
             "Gravity (G): %s",
             hud->objects_gravity_enabled ? "On" : "Off");
    char status_line[32];
    snprintf(status_line, sizeof(status_line),
             "Status: %s", hud->paused ? "Paused" : "Running");
    const char *hint_line =
        "P pause | C clear | E snapshot";

    enum { MAX_HUD_LINES = 32 };
    const char *lines[MAX_HUD_LINES];
    size_t line_count = 0;
    lines[line_count++] = preset_line;
    lines[line_count++] = grid_line;
    lines[line_count++] = window_line;
    lines[line_count++] = mode_line;
    lines[line_count++] = quality_line;
    lines[line_count++] = solver_line;
    lines[line_count++] = vorticity_line;
    lines[line_count++] = pressure_line;
    lines[line_count++] = velocity_line;
    lines[line_count++] = particles_line;
    lines[line_count++] = gravity_line;
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

    SDL_SetRenderDrawColor(g_hud_renderer, 30, 35, 40, 16);
    SDL_RenderFillRect(g_hud_renderer, &panel);
    SDL_SetRenderDrawColor(g_hud_renderer, 255, 255, 255, 60);
    SDL_RenderDrawRect(g_hud_renderer, &panel);

    int y = panel.y + padding;
    for (int i = 0; i < count; ++i) {
        SDL_Rect dst = {panel.x + padding, y, widths[i], heights[i]};
        SDL_RenderCopy(g_hud_renderer, textures[i], NULL, &dst);
        y += heights[i] + spacing;
    }

    for (int i = 0; i < count; ++i) {
        if (textures[i]) SDL_DestroyTexture(textures[i]);
        if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
    }
}
