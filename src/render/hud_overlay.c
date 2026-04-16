#include "render/hud_overlay.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "font_paths.h"
#include "render/text_upload_policy.h"
#include "vk_renderer.h"

static SDL_Renderer *g_hud_renderer = NULL;
static TTF_Font     *g_hud_font     = NULL;

static TTF_Font *load_hud_font(void) {
    const char *paths[] = {
        FONT_BODY_PATH_1,
        FONT_BODY_PATH_2,
        FONT_TITLE_PATH_1,  // extra fallbacks if body fonts fail
        FONT_TITLE_PATH_2
    };
    int point_size = physics_sim_text_raster_point_size(g_hud_renderer, 12, 8);

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], point_size);
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
                                 SDL_Surface **surface,
                                 int *w,
                                 int *h) {
    if (!text || !surface || !w || !h) return;
    if (!g_hud_font || !g_hud_renderer) {
        *surface = NULL;
        *w = *h = 0;
        return;
    }
    SDL_Color color = {240, 240, 240, 255};
    *surface = TTF_RenderUTF8_Blended(g_hud_font, text, color);
    if (!*surface) {
        *w = *h = 0;
        return;
    }
    *w = physics_sim_text_logical_pixels(g_hud_renderer, (*surface)->w);
    *h = physics_sim_text_logical_pixels(g_hud_renderer, (*surface)->h);
}

static void append_token(char *dst, size_t dst_size, const char *token) {
    if (!dst || dst_size == 0 || !token || !token[0]) return;
    size_t len = strlen(dst);
    if (len >= dst_size - 1) return;
    if (len > 0) {
        snprintf(dst + len, dst_size - len, " %s", token);
    } else {
        snprintf(dst + len, dst_size - len, "%s", token);
    }
}

static const char *hud_space_mode_label(SpaceMode mode) {
    switch (mode) {
    case SPACE_MODE_3D:
        return "3D";
    case SPACE_MODE_2D:
    default:
        return "2D";
    }
}

void hud_overlay_draw(const RendererHudInfo *hud) {
    if (!hud || !g_hud_renderer || !g_hud_font) return;

    const char *mode_name = "Box";
    if (hud->sim_mode == SIM_MODE_WIND_TUNNEL) {
        mode_name = "Wind";
    } else if (hud->sim_mode == SIM_MODE_STRUCTURAL) {
        mode_name = "Structural";
    }

    char status_line[96];
    snprintf(status_line, sizeof(status_line), "%s | %s | Grid %dx%d",
             hud->paused ? "Paused" : "Run",
             mode_name,
             hud->grid_w, hud->grid_h);

    char space_line[120];
    snprintf(space_line,
             sizeof(space_line),
             "Space: %s%s%s",
             hud_space_mode_label(hud->requested_space_mode),
             (hud->requested_space_mode != hud->projection_space_mode) ? " -> " : "",
             (hud->requested_space_mode != hud->projection_space_mode)
                 ? hud_space_mode_label(hud->projection_space_mode)
                 : "");

    char backend_line[160];
    snprintf(backend_line,
             sizeof(backend_line),
             "Backend: %s%s",
             (hud->backend_lane == SIM_BACKEND_CONTROLLED_3D)
                 ? "Controlled 3D lane"
                 : "Canonical 2D lane",
             hud->backend_uses_canonical_2d_solver
                 ? " (2D solver scaffold)"
                 : "");

    char preset_line[128];
    snprintf(preset_line, sizeof(preset_line), "Preset: %s",
             (hud->preset_name && hud->preset_name[0]) ? hud->preset_name : "Preset");

    char wind_line[64];
    if (hud->sim_mode == SIM_MODE_WIND_TUNNEL) {
        snprintf(wind_line, sizeof(wind_line), "Inflow %.1f", hud->tunnel_inflow_speed);
    } else {
        wind_line[0] = '\0';
    }

    char quality_line[64];
    snprintf(quality_line, sizeof(quality_line), "Quality: %s",
             (hud->quality_name && hud->quality_name[0]) ? hud->quality_name : "Custom");

    char solver_line[64];
    snprintf(solver_line, sizeof(solver_line),
             "Solver: iter %d, substeps %d",
             hud->solver_iterations,
             hud->physics_substeps);

    char overlays_line[160] = {0};
    if (hud->vorticity_enabled) append_token(overlays_line, sizeof(overlays_line), "Vort(V)");
    if (hud->pressure_enabled) append_token(overlays_line, sizeof(overlays_line), "Press(B)");
    if (hud->velocity_overlay_enabled) append_token(overlays_line, sizeof(overlays_line), "Vel(S)");
    if (hud->particle_overlay_enabled) append_token(overlays_line, sizeof(overlays_line), "Flow(L)");

    char velocity_mode_line[72];
    if (hud->velocity_overlay_enabled) {
        snprintf(velocity_mode_line, sizeof(velocity_mode_line),
                 "Vel mode: %s (Shift+S)",
                 hud->velocity_fixed_length ? "Fixed" : "Magnitude");
    } else {
        velocity_mode_line[0] = '\0';
    }

    char path_warn_line[160] = {0};
    if (!hud->kit_viz_density_enabled || !hud->kit_viz_density_active) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_density_enabled ? "Dens:fb(K)" : "Dens:legacy(K)");
    }
    if (hud->velocity_overlay_enabled &&
        (!hud->kit_viz_velocity_enabled || !hud->kit_viz_velocity_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_velocity_enabled ? "Vel:fb(J)" : "Vel:legacy(J)");
    }
    if (hud->pressure_enabled &&
        (!hud->kit_viz_pressure_enabled || !hud->kit_viz_pressure_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_pressure_enabled ? "P:fb(S+B)" : "P:legacy(S+B)");
    }
    if (hud->vorticity_enabled &&
        (!hud->kit_viz_vorticity_enabled || !hud->kit_viz_vorticity_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_vorticity_enabled ? "V:fb(S+V)" : "V:legacy(S+V)");
    }
    if (hud->particle_overlay_enabled &&
        (!hud->kit_viz_particles_enabled || !hud->kit_viz_particles_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_particles_enabled ? "L:fb(S+L)" : "L:legacy(S+L)");
    }

    char gravity_line[48];
    if (!hud->objects_gravity_enabled) {
        snprintf(gravity_line, sizeof(gravity_line), "Obj gravity: Off (G)");
    } else {
        gravity_line[0] = '\0';
    }

    char retained_runtime_line[112];
    char retained_runtime_mode_line[96];
    if (hud->retained_runtime_visual_active) {
        snprintf(retained_runtime_line,
                 sizeof(retained_runtime_line),
                 "Retained 3D view: Alt+LMB orbit  MMB pan  Wheel zoom  F frame");
        snprintf(retained_runtime_mode_line,
                 sizeof(retained_runtime_mode_line),
                 "Runtime fluid: XY slice only; Z remains scaffold-only");
    } else {
        retained_runtime_line[0] = '\0';
        retained_runtime_mode_line[0] = '\0';
    }

    const char *hint_line_a = "Keys: P/C/E Esc 1/2  V/B/S/L  Shift+S";
    const char *hint_line_b = "Paths: K/J  Shift+V/B/L  G grav  H elastic";

    enum { MAX_HUD_LINES = 32 };
    const char *lines[MAX_HUD_LINES];
    size_t line_count = 0;
    lines[line_count++] = status_line;
    lines[line_count++] = space_line;
    lines[line_count++] = backend_line;
    lines[line_count++] = preset_line;
    if (wind_line[0]) lines[line_count++] = wind_line;
    lines[line_count++] = quality_line;
    if (hud->paused) lines[line_count++] = solver_line;
    if (overlays_line[0]) {
        static char overlays_buf[176];
        snprintf(overlays_buf, sizeof(overlays_buf), "Overlays:%s", overlays_line);
        lines[line_count++] = overlays_buf;
    }
    if (velocity_mode_line[0]) lines[line_count++] = velocity_mode_line;
    if (path_warn_line[0]) {
        static char path_warn_buf[176];
        snprintf(path_warn_buf, sizeof(path_warn_buf), "Path alerts:%s", path_warn_line);
        lines[line_count++] = path_warn_buf;
    }
    if (gravity_line[0]) lines[line_count++] = gravity_line;
    if (retained_runtime_line[0]) lines[line_count++] = retained_runtime_line;
    if (retained_runtime_mode_line[0]) lines[line_count++] = retained_runtime_mode_line;
    if (!hud->retained_runtime_visual_active) {
        lines[line_count++] = hint_line_a;
        if (hud->paused || path_warn_line[0] || overlays_line[0]) {
            lines[line_count++] = hint_line_b;
        }
    }

    SDL_Surface *surfaces[MAX_HUD_LINES];
    int widths[MAX_HUD_LINES];
    int heights[MAX_HUD_LINES];
    for (int i = 0; i < MAX_HUD_LINES; ++i) {
        surfaces[i] = NULL;
        widths[i] = 0;
        heights[i] = 0;
    }
    int count = 0;
    int max_w = 0;
    int total_h = 0;

    for (size_t i = 0; i < line_count; ++i) {
        SDL_Surface *surf = NULL;
        int w = 0, h = 0;
        render_hud_text_line(lines[i], &surf, &w, &h);
        if (!surf) continue;
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

    const int padding = 6;
    const int spacing = 1;
    total_h += spacing * (count - 1);

    SDL_Rect panel = {
        .x = 12,
        .y = 12,
        .w = max_w + padding * 2,
        .h = total_h + padding * 2
    };

    SDL_SetRenderDrawColor(g_hud_renderer, 20, 24, 28, 12);
    SDL_RenderFillRect(g_hud_renderer, &panel);
    SDL_SetRenderDrawColor(g_hud_renderer, 255, 255, 255, 60);
    SDL_RenderDrawRect(g_hud_renderer, &panel);

    int y = panel.y + padding;
    for (int i = 0; i < count; ++i) {
        SDL_Rect dst = {panel.x + padding, y, widths[i], heights[i]};
        VkRendererTexture texture = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)g_hud_renderer,
                                                       surfaces[i],
                                                       &texture,
                                                       physics_sim_text_upload_filter(g_hud_renderer)) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer*)g_hud_renderer, &texture, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer*)g_hud_renderer, &texture);
        }
        y += heights[i] + spacing;
    }

    for (int i = 0; i < count; ++i) {
        if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
    }
}
