#include "app/menu/menu_settings_render.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "app/menu/menu_settings_layout.h"
#include "app/quality_profiles.h"
#include "app/menu/menu_settings_input.h"
#include "app/menu/menu_render.h"
#include "app/menu/menu_settings_schema.h"
#include "render/text_upload_policy.h"

static const MenuSettingsDraft *draft_or_null(const MenuSettingsShellState *state) {
    return state ? &state->draft : NULL;
}

static int font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    int h = fallback;
    if (!font) return fallback;
    h = TTF_FontHeight(font);
    if (h <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, h);
}

static void fit_text_to_width(SDL_Renderer *renderer,
                              TTF_Font *font,
                              const char *text,
                              int max_width,
                              char *out,
                              size_t out_size) {
    int w = 0;
    size_t len = 0;
    if (!out || out_size == 0u) return;
    out[0] = '\0';
    if (!text) return;
    snprintf(out, out_size, "%s", text);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0) {
        w = physics_sim_text_logical_pixels(renderer, w);
        if (w <= max_width) return;
    }

    len = strlen(out);
    while (len > 0u) {
        char candidate[256];
        --len;
        out[len] = '\0';
        snprintf(candidate, sizeof(candidate), "%s...", out);
        if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0) {
            w = physics_sim_text_logical_pixels(renderer, w);
            if (w <= max_width) {
                snprintf(out, out_size, "%s", candidate);
                return;
            }
        }
    }
    snprintf(out, out_size, "...");
}

static void draw_field_cell(SDL_Renderer *renderer,
                            TTF_Font *font,
                            TTF_Font *font_small,
                            const MenuSettingsFieldLayout *layout,
                            const char *label,
                            const char *value) {
    char value_fit[128];
    TTF_Font *value_font = font_small ? font_small : font;
    int body_h = font_height(renderer, value_font, 16);
    int value_x = 0;
    int value_w = 0;
    if (!renderer || !font || !layout || !label || !value) return;

    SDL_SetRenderDrawColor(renderer,
                           menu_color_panel().r,
                           menu_color_panel().g,
                           menu_color_panel().b,
                           255);
    SDL_RenderFillRect(renderer, &layout->cell_rect);
    SDL_SetRenderDrawColor(renderer,
                           menu_color_accent().r,
                           menu_color_accent().g,
                           menu_color_accent().b,
                           120);
    SDL_RenderDrawRect(renderer, &layout->cell_rect);
    menu_draw_text(renderer,
                   font_small ? font_small : font,
                   label,
                   layout->cell_rect.x + 8,
                   layout->cell_rect.y + 4,
                   menu_color_text_dim());

    value_x = layout->cell_rect.x + 8;
    value_w = layout->dec_rect.x - value_x - 8;
    if (value_w < 48) value_w = 48;
    fit_text_to_width(renderer, value_font, value, value_w, value_fit, sizeof(value_fit));
    menu_draw_text(renderer,
                   value_font,
                   value_fit[0] ? value_fit : value,
                   value_x,
                   layout->cell_rect.y + layout->cell_rect.h - body_h - 4,
                   menu_color_text());
    menu_draw_button(renderer, &layout->dec_rect, "-", font_small ? font_small : font, false);
    menu_draw_button(renderer, &layout->inc_rect, "+", font_small ? font_small : font, false);
}

static void render_field_value_label(const MenuSettingsShellState *state,
                                     MenuSettingsFieldId field,
                                     char *out,
                                     size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    out[0] = '\0';
    switch (field) {
    case MENU_SETTINGS_FIELD_GRID_X:
        snprintf(out, out_size, "%dx%d", draft ? draft->grid_x : 0, draft ? draft->grid_y : 0);
        break;
    case MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS:
        snprintf(out, out_size, "%d", draft ? draft->physics_substeps : 0);
        break;
    case MENU_SETTINGS_FIELD_SOLVER_ITERATIONS:
        snprintf(out, out_size, "%d", draft ? draft->fluid_solver_iterations : 0);
        break;
    case MENU_SETTINGS_FIELD_QUALITY_PRESET:
        snprintf(out, out_size, "%s", menu_settings_render_quality_name(state));
        break;
    case MENU_SETTINGS_FIELD_DENSITY_DIFFUSION:
        snprintf(out, out_size, "%.5g", draft ? draft->density_diffusion : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_DENSITY_DECAY:
        snprintf(out, out_size, "%.3f", draft ? draft->density_decay : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_BUOYANCY:
        snprintf(out, out_size, "%.2f", draft ? draft->fluid_buoyancy_force : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER:
        snprintf(out, out_size, "%.2f", draft ? draft->emitter_density_multiplier : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER:
        snprintf(out, out_size, "%.2f", draft ? draft->emitter_velocity_multiplier : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER:
        snprintf(out, out_size, "%.2f", draft ? draft->emitter_sink_multiplier : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED:
        snprintf(out, out_size, "%.1f", draft ? draft->tunnel_inflow_speed : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY:
        snprintf(out, out_size, "%.1f", draft ? draft->tunnel_inflow_density : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE:
        snprintf(out, out_size, "%.2f", draft ? draft->tunnel_viscosity_scale : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_VELOCITY_DAMPING:
        snprintf(out, out_size, "%.6g", draft ? draft->velocity_damping : 0.0f);
        break;
    default:
        snprintf(out, out_size, "--");
        break;
    }
}

const char *menu_settings_render_quality_name(const MenuSettingsShellState *state) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    SpaceMode mode = (state && state->provider == MENU_SETTINGS_PROVIDER_3D)
                         ? SPACE_MODE_3D
                         : SPACE_MODE_2D;
    return quality_profile_name_for_space_mode(mode, draft ? draft->quality_index : -1);
}

void menu_settings_render_grid_label(const MenuSettingsShellState *state,
                                     char *out,
                                     size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "%dx%d cells",
             draft ? draft->grid_x : 0,
             draft ? draft->grid_y : 0);
}

void menu_settings_render_substeps_label(const MenuSettingsShellState *state,
                                         char *out,
                                         size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "%d steps",
             draft ? draft->physics_substeps : 0);
}

void menu_settings_render_solver_label(const MenuSettingsShellState *state,
                                       char *out,
                                       size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "%d iterations",
             draft ? draft->fluid_solver_iterations : 0);
}

void menu_settings_render_headless_frames_label(const MenuSettingsShellState *state,
                                                char *out,
                                                size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "Frames: %d",
             draft ? draft->headless_frame_count : 0);
}

void menu_settings_render_inflow_label(const MenuSettingsShellState *state,
                                       char *out,
                                       size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "Inflow: %.3f",
             draft ? draft->tunnel_inflow_speed : 0.0f);
}

void menu_settings_render_viscosity_label(const MenuSettingsShellState *state,
                                          char *out,
                                          size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "Viscosity: %.6g",
             draft ? draft->velocity_damping : 0.0f);
}

void menu_settings_draw_simulation_panel(SceneMenuInteraction *ctx) {
    SDL_Color accent_color = menu_color_accent();
    MenuSettingsFieldLayout layouts[MENU_SETTINGS_FIELD_COUNT];
    SDL_Rect config_panel;
    char value[128];
    size_t layout_count = 0;
    size_t i = 0;
    bool dirty = false;
    bool saved_differs = false;
    const char *status_text = NULL;
    SDL_Color status_color;
    if (!ctx || !ctx->renderer || !ctx->font || !ctx->cfg) return;

    config_panel = ctx->config_panel_rect;
    dirty = menu_settings_has_pending_changes(ctx);
    saved_differs = menu_settings_saved_differs_from_current(ctx);
    status_color = menu_color_text_dim();
    menu_draw_panel(ctx->renderer, &config_panel);
    SDL_SetRenderDrawColor(ctx->renderer, accent_color.r, accent_color.g, accent_color.b, 120);
    SDL_RenderDrawRect(ctx->renderer, &config_panel);
    menu_draw_text(ctx->renderer,
                   ctx->font_small ? ctx->font_small : ctx->font,
                   "Simulation Settings",
                   config_panel.x + 10,
                   config_panel.y + 8,
                   menu_color_text_dim());

    if (ctx->settings_shell.provider == MENU_SETTINGS_PROVIDER_STRUCTURAL) {
        menu_draw_text(ctx->renderer,
                       ctx->font_small ? ctx->font_small : ctx->font,
                       "Structural mode keeps solver-specific controls in its editor lane.",
                       config_panel.x + 12,
                       config_panel.y + 42,
                       menu_color_text());
        return;
    }
    layout_count = menu_settings_layout_build_field_layouts(ctx,
                                                            layouts,
                                                            MENU_SETTINGS_FIELD_COUNT);
    for (i = 0; i < layout_count; ++i) {
        const MenuSettingsFieldDef *def = menu_settings_schema_field(layouts[i].field);
        if (!def) continue;
        render_field_value_label(&ctx->settings_shell, layouts[i].field, value, sizeof(value));
        draw_field_cell(ctx->renderer,
                        ctx->font,
                        ctx->font_small,
                        &layouts[i],
                        def->label,
                        value);
    }

    menu_draw_toggle(ctx->renderer,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     &ctx->volume_toggle_rect,
                     "Volume Frames",
                     ctx->settings_shell.draft.save_volume_frames);
    menu_draw_toggle(ctx->renderer,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     &ctx->render_toggle_rect,
                     "Render Frames",
                     ctx->settings_shell.draft.save_render_frames);
    menu_draw_toggle(ctx->renderer,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     &ctx->blur_toggle_rect,
                     "Render Blur",
                     ctx->settings_shell.draft.enable_render_blur);
    menu_draw_button(ctx->renderer,
                     &ctx->settings_apply_button.rect,
                     ctx->settings_apply_button.label,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     dirty);
    menu_draw_button(ctx->renderer,
                     &ctx->settings_save_button.rect,
                     ctx->settings_save_button.label,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     dirty || saved_differs);
    menu_draw_button(ctx->renderer,
                     &ctx->settings_reset_button.rect,
                     ctx->settings_reset_button.label,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     false);
    menu_draw_button(ctx->renderer,
                     &ctx->settings_restore_saved_button.rect,
                     ctx->settings_restore_saved_button.label,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     saved_differs);
    menu_draw_button(ctx->renderer,
                     &ctx->settings_defaults_button.rect,
                     ctx->settings_defaults_button.label,
                     ctx->font_small ? ctx->font_small : ctx->font,
                     false);

    if (dirty) {
        status_text = "Pending draft differs from current runtime";
        status_color = menu_color_accent();
    } else if (saved_differs) {
        status_text = "Current runtime differs from saved settings";
        status_color = menu_color_text();
    } else {
        status_text = "Current runtime matches saved settings";
    }
    menu_draw_text(ctx->renderer,
                   ctx->font_small ? ctx->font_small : ctx->font,
                   status_text,
                   config_panel.x + 12,
                   menu_settings_layout_status_y(ctx),
                   status_color);
}
