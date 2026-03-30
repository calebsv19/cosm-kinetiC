#include "app/editor/scene_editor_widgets.h"
#include "app/menu/menu_render.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "vk_renderer.h"

static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};

static void refresh_widget_theme(void) {
    COLOR_PANEL = menu_color_panel();
    COLOR_TEXT = menu_color_text();
    COLOR_TEXT_DIM = menu_color_text_dim();
}

static void fit_text_to_width(TTF_Font *font,
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
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0 && w <= max_width) return;

    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[256];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0 && w <= max_width) {
                snprintf(out, out_size, "%s", candidate);
                return;
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
    if (!renderer || !button || !font) return;
    refresh_widget_theme();
    SDL_Color fill = button->enabled ? COLOR_PANEL : (SDL_Color){25, 28, 32, 255};
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &button->rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &button->rect);
    SDL_Color text = button->enabled ? COLOR_TEXT : COLOR_TEXT_DIM;
    fit_text_to_width(font, button->label, button->rect.w - 16, label_fit, sizeof(label_fit));
    label = label_fit[0] ? label_fit : button->label;
    if (TTF_SizeUTF8(font, label, &label_w, &label_h) != 0) {
        label_w = 0;
        label_h = 0;
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
        .y = button->rect.y + (button->rect.h / 2) - surf->h / 2,
        .w = surf->w,
        .h = surf->h
    };
    VkRendererTexture tex = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surf,
                                                   &tex,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
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
    fit_text_to_width(font, field->label, field->rect.w - 4, label_fit, sizeof(label_fit));
    label_text = label_fit[0] ? label_fit : field->label;
    SDL_Surface *label = TTF_RenderUTF8_Blended(font, label_text, COLOR_TEXT_DIM);
    if (label) {
        SDL_Rect dst = {field->rect.x, field->rect.y - label->h - 6, label->w, label->h};
        VkRendererTexture label_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       label,
                                                       &label_tex,
                                                       VK_FILTER_LINEAR) == VK_SUCCESS) {
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
    fit_text_to_width(font, display, field->rect.w - 12, display_fit, sizeof(display_fit));
    value_text = display_fit[0] ? display_fit : display;
    SDL_Surface *value = TTF_RenderUTF8_Blended(font, value_text, COLOR_TEXT);
    if (value) {
        SDL_Rect dst = {field->rect.x + 8, field->rect.y + (field->rect.h - value->h) / 2, value->w, value->h};
        VkRendererTexture value_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       value,
                                                       &value_tex,
                                                       VK_FILTER_LINEAR) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &value_tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &value_tex);
        }
        SDL_FreeSurface(value);
    }
}
