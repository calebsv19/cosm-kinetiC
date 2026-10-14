#include "app/editor/scene_editor_widgets.h"
#include "app/menu/menu_render.h"
#include "app/ui/physics_sim_ui_button.h"
#include "app/editor/scene_editor_internal.h"
#include "app/menu/menu_text_presentation.h"

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
    menu_text_fit(renderer, font, text, max_width, out, out_size);
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

static void draw_button(const KitUiButtonState *shared, SDL_Renderer *renderer,
                              const EditorButton *button,
                              TTF_Font *font) {
    PhysicsSimUIButtonParams params;
    PhysicsSimUIButtonResolvedStyle style;
    bool hovered = false;
    bool focused = false;
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
    if (shared) {
        params.hovered = shared->hovered;
        params.pressed = shared->pressed;
        focused = shared->focused;
    }
    params.min_left_pad = 8;
    params.min_top_pad = 4;
    if (!physics_sim_ui_button_resolve_style(&params, &style)) {
        return;
    }

    if (focused) style.outline = menu_color_accent();
    SDL_SetRenderDrawColor(renderer, style.fill.r, style.fill.g, style.fill.b, 255);
    SDL_RenderFillRect(renderer, &button->rect);
    SDL_SetRenderDrawColor(renderer, style.outline.r, style.outline.g, style.outline.b, style.outline.a);
    SDL_RenderDrawRect(renderer, &button->rect);

    SDL_Rect label_rect = {button->rect.x+8, button->rect.y+4,
        button->rect.w-16, button->rect.h-8};
    menu_draw_contained_text(renderer, font, button->label, &label_rect, style.text, 1);
}

void scene_editor_draw_button(SDL_Renderer *renderer, const EditorButton *button, TTF_Font *font) {
    draw_button(NULL, renderer, button, font);
}
void scene_editor_draw_owned_button(SceneEditorState *state, const EditorButton *button, TTF_Font *font) {
    KitUiButtonState shared = scene_editor_shared_button(state, button, &button->rect, button->enabled);
    draw_button(&shared, state->renderer, button, font);
}

void scene_editor_draw_shared_button(SDL_Renderer *renderer, const EditorButton *button,
    TTF_Font *font, const KitUiButtonState *shared) {
    draw_button(shared, renderer, button, font);
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

void scene_editor_draw_owned_numeric_field(SceneEditorState *state, TTF_Font *font,
    const NumericField *field, const FluidEmitter *emitter, const PhysicsSimEmitterOverlay *overlay) {
    NumericField display = *field;
    display.editing = false;
    scene_editor_draw_numeric_field(state->renderer, font, &display, emitter, overlay);
    if (field->editing) scene_editor_shared_text_draw(state, font, &field->rect,
        (char *)field->buffer, menu_color_panel(), menu_color_accent(), menu_color_text());
}
