#include "app/editor/scene_editor_widgets.h"
#include "app/menu/menu_render.h"
#include "app/ui/physics_sim_ui_button.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "render/text_draw.h"

static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_BUTTON_BG = {45, 50, 58, 255};

static bool rect_is_hovered(const SDL_Rect *rect) {
    int mouse_x = 0;
    int mouse_y = 0;
    if (!rect) return false;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    return mouse_x >= rect->x &&
           mouse_x < (rect->x + rect->w) &&
           mouse_y >= rect->y &&
           mouse_y < (rect->y + rect->h);
}

static void refresh_widget_theme(void) {
    COLOR_PANEL = menu_color_panel();
    COLOR_TEXT = menu_color_text();
    COLOR_TEXT_DIM = menu_color_text_dim();
    COLOR_BUTTON_BG = menu_color_button_bg();
}

static void fit_text_to_width(SDL_Renderer *renderer,
                              TTF_Font *font,
                              const char *text,
                              int max_width,
                              char *out,
                              size_t out_size) {
    int w = 0;
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;
    snprintf(out, out_size, "%s", text);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0) {
        w = physics_sim_text_logical_pixels(renderer, w);
        if (w <= max_width) return;
    }

    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[256];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0) {
                w = physics_sim_text_logical_pixels(renderer, w);
                if (w <= max_width) {
                    snprintf(out, out_size, "%s", candidate);
                    return;
                }
            }
        }
    }
    snprintf(out, out_size, "...");
}

static PhysicsSimUIButtonPalette scene_editor_button_palette(void) {
    PhysicsSimUIButtonPalette palette;
    palette.idle_fill = COLOR_BUTTON_BG;
    palette.selected_fill = menu_color_button_bg_active();
    palette.hover_fill = menu_color_accent();
    palette.positive_fill = (SDL_Color){36, 172, 86, 255};
    palette.outline_idle = (SDL_Color){0, 0, 0, 200};
    palette.outline_highlight = menu_color_accent();
    palette.text_primary = COLOR_TEXT;
    palette.text_muted = COLOR_TEXT_DIM;
    return palette;
}

void scene_editor_draw_button(SDL_Renderer *renderer,
                              const EditorButton *button,
                              TTF_Font *font) {
    PhysicsSimUIButtonParams params;
    PhysicsSimUIButtonResolvedStyle style;
    bool hovered = false;
    char label_fit[256];
    const char *draw_label = NULL;
    int text_w = 0;
    int text_h = 0;
    int text_x = 0;
    int text_y = 0;
    if (!renderer || !button || !font) return;
    refresh_widget_theme();
    hovered = button->enabled && rect_is_hovered(&button->rect);
    memset(&params, 0, sizeof(params));
    params.rect = &button->rect;
    params.label = button->label;
    params.font = font;
    params.palette = scene_editor_button_palette();
    params.variant = PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT;
    params.enabled = button->enabled;
    params.hovered = hovered;
    params.use_explicit_hover = true;
    params.selected = false;
    params.pressed = false;
    params.min_left_pad = 8;
    params.min_top_pad = 4;
    if (!physics_sim_ui_button_resolve_style(&params, &style)) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, style.fill.r, style.fill.g, style.fill.b, 255);
    SDL_RenderFillRect(renderer, &button->rect);
    SDL_SetRenderDrawColor(renderer, style.outline.r, style.outline.g, style.outline.b, style.outline.a);
    SDL_RenderDrawRect(renderer, &button->rect);

    draw_label = button->label;
    fit_text_to_width(renderer, font, button->label, button->rect.w - 12, label_fit, sizeof(label_fit));
    if (label_fit[0]) {
        draw_label = label_fit;
    }
    if (TTF_SizeUTF8(font, draw_label, &text_w, &text_h) == 0) {
        text_w = physics_sim_text_logical_pixels(renderer, text_w);
        text_h = physics_sim_text_logical_pixels(renderer, text_h);
    }
    text_x = button->rect.x + 8;
    if (text_w > 0 && text_w <= button->rect.w - 12) {
        text_x = button->rect.x + (button->rect.w - text_w) / 2;
    }
    text_y = button->rect.y + (button->rect.h - text_h) / 2;
    menu_draw_text(renderer, font, draw_label, text_x, text_y, style.text);
}

void scene_editor_draw_numeric_field(SDL_Renderer *renderer,
                                     TTF_Font *font,
                                     const NumericField *field,
                                     const FluidEmitter *selected_emitter,
                                     const PhysicsSimEmitterOverlay *selected_overlay_emitter) {
    char label_fit[128];
    char display_fit[64];
    const char *label_text = NULL;
    const char *value_text = NULL;
    if (!renderer || !font || !field) return;
    refresh_widget_theme();
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &field->rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &field->rect);
    fit_text_to_width(renderer, font, field->label, field->rect.w - 4, label_fit, sizeof(label_fit));
    label_text = label_fit[0] ? label_fit : field->label;
    {
        int label_w = 0;
        int label_h = 0;
        if (physics_sim_text_measure_utf8(renderer, font, label_text, &label_w, &label_h)) {
            SDL_Rect dst = {field->rect.x, field->rect.y - label_h - 6, label_w, label_h};
            (void)physics_sim_text_draw_utf8(renderer, font, label_text, COLOR_TEXT_DIM, &dst);
        }
    }

    char display[32] = {0};
    if (field->editing) {
        snprintf(display, sizeof(display), "%s", field->buffer);
    } else if (selected_overlay_emitter) {
        float value = selected_overlay_emitter->strength;
        if (field->target == FIELD_RADIUS) {
            value = selected_overlay_emitter->radius;
        } else if (field->target == FIELD_THERMAL_BUOYANCY_3D) {
            value = selected_overlay_emitter->thermal_buoyancy_3d;
        }
        snprintf(display, sizeof(display), "%.3f", value);
    } else if (selected_emitter) {
        float value = (field->target == FIELD_RADIUS) ? selected_emitter->radius : selected_emitter->strength;
        snprintf(display, sizeof(display), "%.3f", value);
    } else {
        snprintf(display, sizeof(display), "--");
    }
    fit_text_to_width(renderer, font, display, field->rect.w - 12, display_fit, sizeof(display_fit));
    value_text = display_fit[0] ? display_fit : display;
    {
        int value_w = 0;
        int value_h = 0;
        if (physics_sim_text_measure_utf8(renderer, font, value_text, &value_w, &value_h)) {
            SDL_Rect dst = {field->rect.x + 8,
                            field->rect.y + (field->rect.h - value_h) / 2,
                            value_w,
                            value_h};
            (void)physics_sim_text_draw_utf8(renderer, font, value_text, COLOR_TEXT, &dst);
        }
    }
}
