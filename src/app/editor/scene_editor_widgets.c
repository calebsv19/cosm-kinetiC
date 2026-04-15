#include "app/editor/scene_editor_widgets.h"
#include "app/menu/menu_render.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "render/text_upload_policy.h"
#include "vk_renderer.h"

static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_BUTTON_BG = {45, 50, 58, 255};

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    return (SDL_Color){
        (Uint8)(color.r + (Uint8)((255 - color.r) * factor)),
        (Uint8)(color.g + (Uint8)((255 - color.g) * factor)),
        (Uint8)(color.b + (Uint8)((255 - color.b) * factor)),
        color.a
    };
}

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

void scene_editor_draw_button(SDL_Renderer *renderer,
                              const EditorButton *button,
                              TTF_Font *font) {
    char label_fit[128];
    const char *label = NULL;
    int label_w = 0;
    int label_h = 0;
    int label_x = 0;
    bool hovered = false;
    if (!renderer || !button || !font) return;
    refresh_widget_theme();
    hovered = button->enabled && rect_is_hovered(&button->rect);
    SDL_Color fill = button->enabled ? COLOR_BUTTON_BG : (SDL_Color){25, 28, 32, 255};
    if (hovered) {
        fill = lighten_color(fill, 0.18f);
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &button->rect);
    SDL_Color border = hovered ? menu_color_accent() : (SDL_Color){0, 0, 0, 200};
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &button->rect);
    SDL_Color text = button->enabled ? COLOR_TEXT : COLOR_TEXT_DIM;
    fit_text_to_width(renderer, font, button->label, button->rect.w - 16, label_fit, sizeof(label_fit));
    label = label_fit[0] ? label_fit : button->label;
    if (TTF_SizeUTF8(font, label, &label_w, &label_h) != 0) {
        label_w = 0;
        label_h = 0;
    } else {
        label_w = physics_sim_text_logical_pixels(renderer, label_w);
        label_h = physics_sim_text_logical_pixels(renderer, label_h);
    }
    label_x = button->rect.x + 12;
    if (label_w > 0 && label_w <= button->rect.w - 16) {
        label_x = button->rect.x + (button->rect.w - label_w) / 2;
        if (label_x < button->rect.x + 8) label_x = button->rect.x + 8;
    }
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, label, text);
    if (!surf) return;
    SDL_Rect dst = {
        .x = label_x,
        .y = button->rect.y + (button->rect.h / 2) -
             physics_sim_text_logical_pixels(renderer, surf->h) / 2,
        .w = physics_sim_text_logical_pixels(renderer, surf->w),
        .h = physics_sim_text_logical_pixels(renderer, surf->h)
    };
    VkRendererTexture tex = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surf,
                                                   &tex,
                                                   physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
    }
    SDL_FreeSurface(surf);
}

void scene_editor_draw_numeric_field(SDL_Renderer *renderer,
                                     TTF_Font *font,
                                     const NumericField *field,
                                     const FluidEmitter *selected_emitter) {
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
    SDL_Surface *label = TTF_RenderUTF8_Blended(font, label_text, COLOR_TEXT_DIM);
    if (label) {
        int label_w = physics_sim_text_logical_pixels(renderer, label->w);
        int label_h = physics_sim_text_logical_pixels(renderer, label->h);
        SDL_Rect dst = {field->rect.x, field->rect.y - label_h - 6, label_w, label_h};
        VkRendererTexture label_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       label,
                                                       &label_tex,
                                                       physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &label_tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &label_tex);
        }
        SDL_FreeSurface(label);
    }

    char display[32] = {0};
    if (field->editing) {
        snprintf(display, sizeof(display), "%s", field->buffer);
    } else if (selected_emitter) {
        float value = (field->target == FIELD_RADIUS) ? selected_emitter->radius : selected_emitter->strength;
        snprintf(display, sizeof(display), "%.3f", value);
    } else {
        snprintf(display, sizeof(display), "--");
    }
    fit_text_to_width(renderer, font, display, field->rect.w - 12, display_fit, sizeof(display_fit));
    value_text = display_fit[0] ? display_fit : display;
    SDL_Surface *value = TTF_RenderUTF8_Blended(font, value_text, COLOR_TEXT);
    if (value) {
        int value_w = physics_sim_text_logical_pixels(renderer, value->w);
        int value_h = physics_sim_text_logical_pixels(renderer, value->h);
        SDL_Rect dst = {field->rect.x + 8, field->rect.y + (field->rect.h - value_h) / 2, value_w, value_h};
        VkRendererTexture value_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       value,
                                                       &value_tex,
                                                       physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &value_tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &value_tex);
        }
        SDL_FreeSurface(value);
    }
}
