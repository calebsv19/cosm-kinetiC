#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay.h"

#include <stdio.h>

#include "app/menu/menu_render.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.h"
#include "kit_workspace_authoring_ui.h"

enum {
    PHYSICS_SIM_AUTHORING_PANE_ROW_CAP = 3
};

static SDL_Rect physics_sim_authoring_sdl_rect(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Rect physics_sim_authoring_sdl_kit_rect(KitRenderRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Color physics_sim_authoring_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
}

static void physics_sim_authoring_draw_button(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const KitWorkspaceAuthoringOverlayButton *button) {
    SDL_Rect rect;
    SDL_Color fill;
    SDL_Color border;
    if (!renderer || !font || !button || !button->visible) return;
    rect = physics_sim_authoring_sdl_rect(button->rect);
    fill = physics_sim_authoring_alpha(menu_color_button_bg(), button->enabled ? 238u : 140u);
    border = physics_sim_authoring_alpha(menu_color_accent(), button->enabled ? 245u : 150u);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    menu_draw_text(renderer,
                   font,
                   button->label ? button->label : "",
                   rect.x + 8,
                   rect.y + 5,
                   menu_color_text());
}

static void physics_sim_authoring_draw_controls(SceneMenuInteraction *ctx, int width) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i = 0u;
    if (!ctx || !ctx->renderer || width <= 0) return;
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        width,
        physics_sim_workspace_authoring_host_active(&ctx->workspace_authoring),
        physics_sim_workspace_authoring_host_pane_overlay_active(&ctx->workspace_authoring),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        physics_sim_authoring_draw_button(ctx->renderer,
                                          ctx->font_small ? ctx->font_small : ctx->font,
                                          &buttons[i]);
    }
}

static void physics_sim_authoring_draw_pane_rows(SceneMenuInteraction *ctx,
                                                 int width,
                                                 int height) {
    PhysicsSimWorkspaceAuthoringPaneRow rows[PHYSICS_SIM_AUTHORING_PANE_ROW_CAP];
    uint32_t count = 0u;
    uint32_t i = 0u;
    SDL_Color pane_border = physics_sim_authoring_alpha(menu_color_accent(), 210u);
    SDL_Color label_fill = physics_sim_authoring_alpha(menu_color_panel(), 232u);
    SDL_Color text = menu_color_text();
    SDL_Color muted = menu_color_text_dim();
    TTF_Font *font = ctx->font_small ? ctx->font_small : ctx->font;
    if (!ctx || !ctx->renderer || width <= 0 || height <= 0) return;

    count = physics_sim_workspace_authoring_overlay_build_pane_rows(
        width,
        height,
        rows,
        (uint32_t)(sizeof(rows) / sizeof(rows[0])));
    for (i = 0u; i < count; ++i) {
        SDL_Rect pane_rect = physics_sim_authoring_sdl_rect(rows[i].pane_rect);
        SDL_Rect label_rect;
        char detail[192];
        if (pane_rect.w <= 0 || pane_rect.h <= 0) continue;

        SDL_SetRenderDrawColor(ctx->renderer,
                               pane_border.r,
                               pane_border.g,
                               pane_border.b,
                               pane_border.a);
        SDL_RenderDrawRect(ctx->renderer, &pane_rect);
        label_rect = (SDL_Rect){ pane_rect.x + 8, pane_rect.y + 8, 240, 24 };
        if (label_rect.x + label_rect.w > pane_rect.x + pane_rect.w - 8) {
            label_rect.w = (pane_rect.x + pane_rect.w - 8) - label_rect.x;
        }
        if (label_rect.w < 96) label_rect.w = pane_rect.w - 16;
        if (label_rect.w > 0) {
            SDL_SetRenderDrawColor(ctx->renderer,
                                   label_fill.r,
                                   label_fill.g,
                                   label_fill.b,
                                   label_fill.a);
            SDL_RenderFillRect(ctx->renderer, &label_rect);
            SDL_SetRenderDrawColor(ctx->renderer,
                                   pane_border.r,
                                   pane_border.g,
                                   pane_border.b,
                                   pane_border.a);
            SDL_RenderDrawRect(ctx->renderer, &label_rect);
            menu_draw_text(ctx->renderer,
                           font,
                           rows[i].pane_label ? rows[i].pane_label : "Pane",
                           label_rect.x + 6,
                           label_rect.y + 4,
                           text);
        }

        if (pane_rect.w > 260 && pane_rect.h > 96) {
            snprintf(detail,
                     sizeof(detail),
                     "%s / %s",
                     rows[i].module_key ? rows[i].module_key : "unbound",
                     rows[i].module_label ? rows[i].module_label : "Unbound");
            menu_draw_text(ctx->renderer,
                           font,
                           detail,
                           pane_rect.x + 14,
                           pane_rect.y + 42,
                           muted);
        }
    }
}

static void physics_sim_authoring_draw_font_theme_button(
    SceneMenuInteraction *ctx,
    const KitRenderRect *rect,
    const char *label,
    int selected,
    int enabled) {
    SDL_Rect sdl_rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    TTF_Font *font;
    if (!ctx || !ctx->renderer || !rect) return;
    sdl_rect = physics_sim_authoring_sdl_kit_rect(*rect);
    if (sdl_rect.w <= 0 || sdl_rect.h <= 0) return;
    fill = physics_sim_authoring_alpha(selected ? menu_color_button_bg_active()
                                                : menu_color_button_bg(),
                                       enabled ? 238u : 128u);
    border = physics_sim_authoring_alpha(selected ? menu_color_text()
                                                  : menu_color_accent(),
                                         enabled ? 245u : 130u);
    text = enabled ? menu_color_text() : menu_color_text_dim();
    font = ctx->font_small ? ctx->font_small : ctx->font;
    SDL_SetRenderDrawColor(ctx->renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(ctx->renderer, &sdl_rect);
    SDL_SetRenderDrawColor(ctx->renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(ctx->renderer, &sdl_rect);
    menu_draw_text(ctx->renderer,
                   font,
                   label ? label : "",
                   sdl_rect.x + 8,
                   sdl_rect.y + 5,
                   text);
}

static void physics_sim_authoring_draw_section(SceneMenuInteraction *ctx,
                                               const KitRenderRect *rect,
                                               const char *title,
                                               const char *subtitle) {
    SDL_Rect sdl_rect;
    TTF_Font *font;
    if (!ctx || !ctx->renderer || !rect) return;
    sdl_rect = physics_sim_authoring_sdl_kit_rect(*rect);
    if (sdl_rect.w <= 0 || sdl_rect.h <= 0) return;
    font = ctx->font_small ? ctx->font_small : ctx->font;
    SDL_SetRenderDrawColor(ctx->renderer,
                           menu_color_panel().r,
                           menu_color_panel().g,
                           menu_color_panel().b,
                           238);
    SDL_RenderFillRect(ctx->renderer, &sdl_rect);
    SDL_SetRenderDrawColor(ctx->renderer,
                           menu_color_accent().r,
                           menu_color_accent().g,
                           menu_color_accent().b,
                           210);
    SDL_RenderDrawRect(ctx->renderer, &sdl_rect);
    menu_draw_text(ctx->renderer,
                   ctx->font ? ctx->font : font,
                   title ? title : "",
                   sdl_rect.x + 14,
                   sdl_rect.y + 12,
                   menu_color_text());
    if (subtitle && subtitle[0]) {
        menu_draw_text(ctx->renderer,
                       font,
                       subtitle,
                       sdl_rect.x + 14,
                       sdl_rect.y + 40,
                       menu_color_text_dim());
    }
}

static void physics_sim_authoring_draw_font_theme_overlay(SceneMenuInteraction *ctx,
                                                          int width,
                                                          int height) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    char font_preset[64] = "daw_default";
    char theme_preset[64] = "greyscale";
    char text_size[96];
    char status[192];
    uint32_t i;
    if (!ctx || !ctx->renderer || width <= 0 || height <= 0) return;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL, width, height, &layout)) {
        return;
    }

    (void)physics_sim_shared_font_current_preset(font_preset, sizeof(font_preset));
    (void)physics_sim_shared_theme_current_preset(theme_preset, sizeof(theme_preset));
    snprintf(text_size,
             sizeof(text_size),
             "Text Size step:%d (%d%%)",
             ctx->cfg ? ctx->cfg->text_zoom_step : 0,
             app_config_text_zoom_percent(ctx->cfg));
    snprintf(status,
             sizeof(status),
             "%s",
             ctx->workspace_authoring.font_theme_status_active
                 ? ctx->workspace_authoring.font_theme_status
                 : "Click a preset to preview live; Apply accepts, Cancel restores baseline.");

    SDL_SetRenderDrawColor(ctx->renderer,
                           menu_color_bg().r,
                           menu_color_bg().g,
                           menu_color_bg().b,
                           255);
    SDL_RenderFillRect(ctx->renderer, NULL);

    physics_sim_authoring_draw_section(ctx,
                                       &layout.font_preset_section,
                                       "Font Preset",
                                       font_preset);
    for (i = 0u; i < layout.font_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT + i);
        CoreFontPresetId preset_id;
        int selected = 0;
        if (kit_workspace_authoring_ui_font_theme_button_font_preset_id(button_id, &preset_id)) {
            const char *name = core_font_preset_name(preset_id);
            selected = name && strcmp(name, font_preset) == 0;
        }
        physics_sim_authoring_draw_font_theme_button(
            ctx,
            &layout.font_preset_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            selected,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id));
    }

    physics_sim_authoring_draw_section(ctx,
                                       &layout.text_size_section,
                                       "Text Size",
                                       text_size);
    physics_sim_authoring_draw_font_theme_button(
        ctx,
        &layout.text_size_dec_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC),
        0,
        1);
    physics_sim_authoring_draw_font_theme_button(
        ctx,
        &layout.text_size_value_chip,
        text_size,
        1,
        1);
    physics_sim_authoring_draw_font_theme_button(
        ctx,
        &layout.text_size_inc_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC),
        0,
        1);
    physics_sim_authoring_draw_font_theme_button(
        ctx,
        &layout.text_size_reset_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET),
        0,
        1);

    physics_sim_authoring_draw_section(ctx,
                                       &layout.theme_preset_section,
                                       "Theme Preset",
                                       theme_preset);
    for (i = 0u; i < layout.theme_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT + i);
        CoreThemePresetId preset_id;
        int selected = 0;
        if (kit_workspace_authoring_ui_font_theme_button_theme_preset_id(button_id, &preset_id)) {
            const char *name = core_theme_preset_name(preset_id);
            selected = name && strcmp(name, theme_preset) == 0;
        }
        physics_sim_authoring_draw_font_theme_button(
            ctx,
            &layout.theme_preset_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            selected,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id));
    }

    physics_sim_authoring_draw_section(ctx,
                                       &layout.custom_theme_section,
                                       "Custom Theme Slots",
                                       status);
    for (i = 0u; i < layout.custom_theme_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB + i);
        physics_sim_authoring_draw_font_theme_button(
            ctx,
            &layout.custom_theme_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            0,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id));
    }
}

void physics_sim_workspace_authoring_overlay_draw(SceneMenuInteraction *ctx,
                                                  int width,
                                                  int height) {
    if (!ctx || !ctx->renderer ||
        !physics_sim_workspace_authoring_host_active(&ctx->workspace_authoring)) {
        return;
    }

    if (physics_sim_workspace_authoring_host_pane_overlay_active(&ctx->workspace_authoring)) {
        physics_sim_authoring_draw_pane_rows(ctx, width, height);
    } else if (physics_sim_workspace_authoring_host_font_theme_overlay_active(&ctx->workspace_authoring)) {
        physics_sim_authoring_draw_font_theme_overlay(ctx, width, height);
    }
    physics_sim_authoring_draw_controls(ctx, width);
}
