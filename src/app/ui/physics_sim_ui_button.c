#include "app/ui/physics_sim_ui_button.h"

#include <stdio.h>
#include <string.h>

#include "render/text_draw.h"

static CoreThemeColor physics_sim_ui_button_core_color(SDL_Color color) {
    CoreThemeColor out;
    out.r = color.r;
    out.g = color.g;
    out.b = color.b;
    out.a = color.a;
    return out;
}

static SDL_Color physics_sim_ui_button_sdl_color(CoreThemeColor color) {
    return (SDL_Color){color.r, color.g, color.b, color.a};
}

static bool physics_sim_ui_button_fill_edge(SDL_Renderer *renderer,
                                            int x,
                                            int y,
                                            int w,
                                            int h,
                                            SDL_Color color) {
    SDL_Rect rect = {x, y, w, h};
    if (!renderer || w <= 0 || h <= 0) {
        return false;
    }
    if (SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) != 0) {
        return false;
    }
    return SDL_RenderFillRect(renderer, &rect) == 0;
}

static bool physics_sim_ui_button_draw_outline(SDL_Renderer *renderer,
                                               const SDL_Rect *rect,
                                               SDL_Color color) {
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) {
        return false;
    }
    if (!physics_sim_ui_button_fill_edge(renderer, rect->x, rect->y, rect->w, 1, color)) {
        return false;
    }
    if (rect->h > 1 &&
        !physics_sim_ui_button_fill_edge(renderer, rect->x, rect->y + rect->h - 1, rect->w, 1, color)) {
        return false;
    }
    if (rect->h > 2 &&
        !physics_sim_ui_button_fill_edge(renderer, rect->x, rect->y + 1, 1, rect->h - 2, color)) {
        return false;
    }
    if (rect->w > 1 && rect->h > 2 &&
        !physics_sim_ui_button_fill_edge(renderer, rect->x + rect->w - 1, rect->y + 1, 1, rect->h - 2, color)) {
        return false;
    }
    return true;
}

static void physics_sim_ui_button_fit_text_to_width(SDL_Renderer *renderer,
                                                    TTF_Font *font,
                                                    const char *text,
                                                    int max_width,
                                                    char *out,
                                                    size_t out_size) {
    int width = 0;
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;
    snprintf(out, out_size, "%s", text);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &width, NULL) == 0) {
        width = physics_sim_text_logical_pixels(renderer, width);
        if (width <= max_width) return;
    }
    len = strlen(out);
    while (len > 0) {
        char candidate[256];
        --len;
        out[len] = '\0';
        snprintf(candidate, sizeof(candidate), "%s...", out);
        if (TTF_SizeUTF8(font, candidate, &width, NULL) == 0) {
            width = physics_sim_text_logical_pixels(renderer, width);
            if (width <= max_width) {
                snprintf(out, out_size, "%s", candidate);
                return;
            }
        }
    }
    snprintf(out, out_size, "...");
}

static int physics_sim_ui_button_font_height(SDL_Renderer *renderer,
                                             TTF_Font *font,
                                             int fallback) {
    int height = 0;
    if (!font) return fallback;
    height = TTF_FontHeight(font);
    if (height <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, height);
}

bool physics_sim_ui_button_draw_frame(SDL_Renderer *renderer,
                                      const SDL_Rect *rect,
                                      SDL_Color fill,
                                      SDL_Color outline) {
    SDL_Rect fill_rect = {0};
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) {
        return false;
    }
    fill_rect = *rect;
    if (SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a) != 0 ||
        SDL_RenderFillRect(renderer, &fill_rect) != 0 ||
        !physics_sim_ui_button_draw_outline(renderer, rect, outline)) {
        return false;
    }
    return true;
}

bool physics_sim_ui_button_resolve_style(const PhysicsSimUIButtonParams *params,
                                         PhysicsSimUIButtonResolvedStyle *out_style) {
    KitUiButtonSpec spec;
    KitUiButtonStyle style;
    KitUiButtonTheme theme;
    int mouse_x = 0;
    int mouse_y = 0;
    bool hovered = false;

    if (!params || !params->rect || !params->label || !out_style ||
        params->rect->w <= 0 || params->rect->h <= 0) {
        return false;
    }

    kit_ui_button_spec_init(&spec, params->label);
    switch (params->variant) {
        case PHYSICS_SIM_UI_BUTTON_VARIANT_PRIMARY:
            spec.variant = KIT_UI_BUTTON_VARIANT_PRIMARY;
            break;
        case PHYSICS_SIM_UI_BUTTON_VARIANT_POSITIVE:
            spec.variant = KIT_UI_BUTTON_VARIANT_POSITIVE;
            break;
        case PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT:
        default:
            spec.variant = KIT_UI_BUTTON_VARIANT_DEFAULT;
            break;
    }

    if (params->use_explicit_hover) {
        hovered = params->hovered;
    } else {
        SDL_GetMouseState(&mouse_x, &mouse_y);
        hovered = mouse_x >= params->rect->x &&
                  mouse_x < (params->rect->x + params->rect->w) &&
                  mouse_y >= params->rect->y &&
                  mouse_y < (params->rect->y + params->rect->h);
    }
    spec.state.hovered = params->enabled && hovered;
    spec.state.selected = params->selected ? 1 : 0;
    spec.state.pressed = params->pressed ? 1 : 0;
    spec.state.disabled = params->enabled ? 0 : 1;
    spec.state.focused = 0;

    theme.idle_fill = physics_sim_ui_button_core_color(params->palette.idle_fill);
    theme.selected_fill = physics_sim_ui_button_core_color(params->palette.selected_fill);
    theme.hover_fill = physics_sim_ui_button_core_color(params->palette.hover_fill);
    theme.positive_fill = physics_sim_ui_button_core_color(params->palette.positive_fill);
    theme.outline_idle = physics_sim_ui_button_core_color(params->palette.outline_idle);
    theme.outline_highlight = physics_sim_ui_button_core_color(params->palette.outline_highlight);
    theme.text_primary = physics_sim_ui_button_core_color(params->palette.text_primary);
    theme.text_muted = physics_sim_ui_button_core_color(params->palette.text_muted);
    if (!kit_ui_button_style_resolve(&theme, &spec, &style)) {
        return false;
    }
    out_style->fill = physics_sim_ui_button_sdl_color(style.fill);
    out_style->outline = physics_sim_ui_button_sdl_color(style.outline);
    out_style->text = physics_sim_ui_button_sdl_color(style.text);
    return true;
}

bool physics_sim_ui_button_draw_resolved(SDL_Renderer *renderer,
                                         const PhysicsSimUIButtonResolvedDrawParams *params) {
    SDL_Rect text_dst = {0};
    char label_fit[256];
    const char *draw_label = NULL;
    int text_width_px = 0;
    int text_height_px = 0;
    int inset_x = 12;
    int min_left_pad = 8;
    int min_top_pad = 4;
    int max_label_width = 0;

    if (!renderer || !params || !params->rect || !params->font || !params->label ||
        params->rect->w <= 0 || params->rect->h <= 0) {
        return false;
    }

    draw_label = params->label;

    if (!physics_sim_ui_button_draw_frame(renderer, params->rect, params->fill, params->outline)) {
        return false;
    }

    if (params->min_left_pad > 0) {
        min_left_pad = params->min_left_pad;
    }
    if (params->min_top_pad > 0) {
        min_top_pad = params->min_top_pad;
    }
    max_label_width = params->rect->w - 16;
    if (max_label_width < 8) {
        max_label_width = 8;
    }
    physics_sim_ui_button_fit_text_to_width(renderer,
                                            params->font,
                                            params->label,
                                            max_label_width,
                                            label_fit,
                                            sizeof(label_fit));
    if (label_fit[0]) {
        draw_label = label_fit;
    }
    if (TTF_SizeUTF8(params->font, draw_label, &text_width_px, &text_height_px) != 0) {
        text_width_px = 0;
        text_height_px = physics_sim_ui_button_font_height(renderer,
                                                           params->font,
                                                           params->rect->h / 2);
    } else {
        text_width_px = physics_sim_text_logical_pixels(renderer, text_width_px);
        text_height_px = physics_sim_text_logical_pixels(renderer, text_height_px);
    }

    if (text_width_px > 0 && text_width_px <= params->rect->w - 16) {
        inset_x = (params->rect->w - text_width_px) / 2;
        if (inset_x < min_left_pad) {
            inset_x = min_left_pad;
        }
    }
    text_dst.x = params->rect->x + inset_x;
    text_dst.y = params->rect->y + (params->rect->h - text_height_px) / 2;
    text_dst.w = text_width_px;
    text_dst.h = text_height_px;
    if (text_dst.y < params->rect->y + min_top_pad) {
        text_dst.y = params->rect->y + min_top_pad;
    }

    (void)physics_sim_text_draw_utf8(renderer,
                                     params->font,
                                     draw_label,
                                     params->text,
                                     &text_dst);
    return true;
}

bool physics_sim_ui_button_draw(SDL_Renderer *renderer,
                                const PhysicsSimUIButtonParams *params) {
    PhysicsSimUIButtonResolvedStyle style;
    PhysicsSimUIButtonResolvedDrawParams draw_params;
    if (!physics_sim_ui_button_resolve_style(params, &style)) {
        return false;
    }
    draw_params.rect = params->rect;
    draw_params.label = params->label;
    draw_params.font = params->font;
    draw_params.fill = style.fill;
    draw_params.outline = style.outline;
    draw_params.text = style.text;
    draw_params.min_left_pad = params->min_left_pad;
    draw_params.min_top_pad = params->min_top_pad;
    return physics_sim_ui_button_draw_resolved(renderer, &draw_params);
}
