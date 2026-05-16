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

static bool provider_is_3d_like(MenuSettingsProviderId provider) {
    return provider == MENU_SETTINGS_PROVIDER_3D ||
           provider == MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D;
}

static int font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    int h = fallback;
    if (!font) return fallback;
    h = TTF_FontHeight(font);
    if (h <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, h);
}

static int text_width(SDL_Renderer *renderer, TTF_Font *font, const char *text) {
    int w = 0;
    if (!font || !text || text[0] == '\0') return 0;
    if (TTF_SizeUTF8(font, text, &w, NULL) != 0) return 0;
    return physics_sim_text_logical_pixels(renderer, w);
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
                            const char *value,
                            bool interactive) {
    char label_fit[96];
    char value_fit[128];
    TTF_Font *text_font = font_small ? font_small : font;
    int body_h = font_height(renderer, text_font, 16);
    int body_y = 0;
    int label_x = 0;
    int label_w = 0;
    int value_right = 0;
    int value_x = 0;
    int value_w = 0;
    int value_text_w = 0;
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
    body_y = layout->cell_rect.y + (layout->cell_rect.h - body_h) / 2;
    label_x = layout->cell_rect.x + 8;
    value_right = interactive ? (layout->dec_rect.x - 8)
                              : (layout->cell_rect.x + layout->cell_rect.w - 8);
    label_w = layout->cell_rect.w / 2 - 16;
    if (interactive) label_w -= 8;
    if (label_w < 56) label_w = 56;
    fit_text_to_width(renderer, text_font, label, label_w, label_fit, sizeof(label_fit));
    menu_draw_text(renderer,
                   text_font,
                   label_fit[0] ? label_fit : label,
                   label_x,
                   body_y,
                   menu_color_text_dim());

    value_x = layout->cell_rect.x + layout->cell_rect.w / 2;
    value_w = value_right - value_x;
    if (value_w < 48) value_w = 48;
    fit_text_to_width(renderer, text_font, value, value_w, value_fit, sizeof(value_fit));
    value_text_w = text_width(renderer, text_font, value_fit[0] ? value_fit : value);
    value_x = value_right - value_text_w;
    if (value_x < layout->cell_rect.x + layout->cell_rect.w / 2) {
        value_x = layout->cell_rect.x + layout->cell_rect.w / 2;
    }
    menu_draw_text(renderer,
                   text_font,
                   value_fit[0] ? value_fit : value,
                   value_x,
                   body_y,
                   menu_color_text());
    if (interactive) {
        menu_draw_button(renderer, &layout->dec_rect, "-", text_font, false);
        menu_draw_button(renderer, &layout->inc_rect, "+", text_font, false);
    }
}

static bool field_is_interactive(const MenuSettingsFieldDef *def) {
    return def && !def->runtime_display_only;
}

static const char *field_label_for_context(const SceneMenuInteraction *ctx,
                                           const MenuSettingsFieldDef *def) {
    if (!def) return "";
    if (ctx && !provider_is_3d_like(ctx->settings_shell.provider) &&
        def->id == MENU_SETTINGS_FIELD_GRID_X) {
        return "Grid";
    }
    return def->label;
}

static int draft_requested_3d_depth(const MenuSettingsDraft *draft) {
    if (!draft || draft->grid_z <= 0) return 0;
    return draft->grid_z;
}

static void render_field_value_label(const SceneMenuInteraction *ctx,
                                     MenuSettingsFieldId field,
                                     char *out,
                                     size_t out_size) {
    const MenuSettingsDraft *draft = draft_or_null(ctx ? &ctx->settings_shell : NULL);
    if (!out || out_size == 0u) return;
    out[0] = '\0';
    switch (field) {
    case MENU_SETTINGS_FIELD_GRID_X:
        if (ctx && provider_is_3d_like(ctx->settings_shell.provider)) {
            snprintf(out, out_size, "%d", draft ? draft->grid_x : 0);
        } else {
            snprintf(out, out_size, "%dx%d", draft ? draft->grid_x : 0, draft ? draft->grid_y : 0);
        }
        break;
    case MENU_SETTINGS_FIELD_GRID_Y:
        snprintf(out, out_size, "%d", draft ? draft->grid_y : 0);
        break;
    case MENU_SETTINGS_FIELD_GRID_Z: {
        int requested_depth = draft_requested_3d_depth(draft);
        if (requested_depth > 0) {
            snprintf(out, out_size, "%d", requested_depth);
        } else {
            snprintf(out, out_size, "Auto");
        }
        break;
    }
    case MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS:
        snprintf(out, out_size, "%d", draft ? draft->physics_substeps : 0);
        break;
    case MENU_SETTINGS_FIELD_SOLVER_ITERATIONS:
        snprintf(out, out_size, "%d", draft ? draft->fluid_solver_iterations : 0);
        break;
    case MENU_SETTINGS_FIELD_QUALITY_PRESET:
        snprintf(out, out_size, "%s",
                 menu_settings_render_quality_name(ctx ? &ctx->settings_shell : NULL));
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
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED:
        snprintf(out, out_size, "%u", draft ? draft->atmosphere.seed : 0u);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE:
        snprintf(out, out_size, "%.2f", draft ? draft->atmosphere.density_scale : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_THRESHOLD:
        snprintf(out, out_size, "%.2f", draft ? draft->atmosphere.density_threshold : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X:
        snprintf(out, out_size, "%.1f", draft ? draft->atmosphere.base_wind_x : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Y:
        snprintf(out, out_size, "%.1f", draft ? draft->atmosphere.base_wind_y : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z:
        snprintf(out, out_size, "%.1f", draft ? draft->atmosphere.base_wind_z : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_TURBULENCE:
        snprintf(out, out_size, "%.1f", draft ? draft->atmosphere.turbulence_strength : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_NOISE_SCALE:
        snprintf(out, out_size, "%.2f", draft ? draft->atmosphere.noise_scale : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN:
        snprintf(out, out_size, "%.2f", draft ? draft->atmosphere.band_min_y : 0.0f);
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX:
        snprintf(out, out_size, "%.2f", draft ? draft->atmosphere.band_max_y : 0.0f);
        break;
    default:
        snprintf(out, out_size, "--");
        break;
    }
}

const char *menu_settings_render_quality_name(const MenuSettingsShellState *state) {
    const MenuSettingsDraft *draft = draft_or_null(state);
    SpaceMode mode = (state && provider_is_3d_like(state->provider))
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
        render_field_value_label(ctx, layouts[i].field, value, sizeof(value));
        draw_field_cell(ctx->renderer,
                        ctx->font,
                        ctx->font_small,
                        &layouts[i],
                        field_label_for_context(ctx, def),
                        value,
                        field_is_interactive(def));
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
