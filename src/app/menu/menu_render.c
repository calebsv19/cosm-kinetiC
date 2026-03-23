#include "app/menu/menu_render.h"

#include <stdio.h>

#include "app/menu/menu_state.h"

static SDL_Color COLOR_BG       = {18, 18, 22, 255};
static SDL_Color COLOR_PANEL    = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT     = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM = {210, 216, 226, 255};
static SDL_Color COLOR_ACCENT   = {90, 170, 255, 255};
static SDL_Color COLOR_BUTTON_BG = {45, 50, 58, 255};
static SDL_Color COLOR_BUTTON_BG_ACTIVE = {60, 120, 200, 255};

static int color_luma(SDL_Color color) {
    return (299 * (int)color.r + 587 * (int)color.g + 114 * (int)color.b) / 1000;
}

static int color_contrast_gap(SDL_Color a, SDL_Color b) {
    int gap = color_luma(a) - color_luma(b);
    return gap < 0 ? -gap : gap;
}

static Uint8 mix_u8(Uint8 a, Uint8 b, int a_weight, int b_weight) {
    int total = a_weight + b_weight;
    if (total <= 0) return a;
    return (Uint8)(((int)a * a_weight + (int)b * b_weight) / total);
}

static SDL_Color mix_color(SDL_Color a, SDL_Color b, int a_weight, int b_weight) {
    return (SDL_Color){
        mix_u8(a.r, b.r, a_weight, b_weight),
        mix_u8(a.g, b.g, a_weight, b_weight),
        mix_u8(a.b, b.b, a_weight, b_weight),
        mix_u8(a.a, b.a, a_weight, b_weight)
    };
}

static SDL_Color ensure_fill_contrast(SDL_Color fill,
                                      SDL_Color preferred_text,
                                      SDL_Color darker_anchor) {
    if (color_contrast_gap(fill, preferred_text) >= 110) {
        return fill;
    }
    if (color_luma(preferred_text) >= 150) {
        return mix_color(fill, darker_anchor, 1, 2);
    }
    return mix_color(fill, (SDL_Color){240, 243, 247, fill.a}, 1, 2);
}

static SDL_Color choose_readable_text(SDL_Color background, SDL_Color preferred_text) {
    if (color_contrast_gap(background, preferred_text) >= 110) {
        return preferred_text;
    }
    if (color_luma(background) >= 150) {
        return (SDL_Color){24, 28, 34, preferred_text.a ? preferred_text.a : 255};
    }
    return (SDL_Color){245, 247, 250, preferred_text.a ? preferred_text.a : 255};
}

static SDL_Color border_for_background(SDL_Color background) {
    if (color_luma(background) >= 150) {
        return (SDL_Color){48, 54, 62, 180};
    }
    return (SDL_Color){0, 0, 0, 180};
}

void menu_set_theme_palette(const MenuThemePalette *palette) {
    if (!palette) return;
    COLOR_BG = palette->background;
    COLOR_PANEL = palette->panel;
    COLOR_TEXT = choose_readable_text(COLOR_PANEL, palette->text);
    COLOR_TEXT_DIM = choose_readable_text(COLOR_PANEL, palette->text_dim);
    COLOR_ACCENT = ensure_fill_contrast(palette->accent, COLOR_TEXT, COLOR_PANEL);
    COLOR_BUTTON_BG = ensure_fill_contrast(palette->button_bg, COLOR_TEXT, COLOR_PANEL);
    COLOR_BUTTON_BG_ACTIVE = ensure_fill_contrast(palette->button_bg_active, COLOR_TEXT, COLOR_PANEL);
}

SDL_Rect menu_preset_list_rect(void) {
    SDL_Rect rect = {
        .x = PRESET_LIST_MARGIN_X,
        .y = PRESET_LIST_MARGIN_Y,
        .w = PRESET_LIST_WIDTH,
        .h = PRESET_LIST_HEIGHT
    };
    return rect;
}

void menu_draw_text(SDL_Renderer *renderer,
                    TTF_Font *font,
                    const char *text,
                    int x,
                    int y,
                    SDL_Color color) {
    if (!renderer || !font || !text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    VkRendererTexture tex = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surf,
                                                   &tex,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
    }
    SDL_FreeSurface(surf);
}

void menu_draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect) {
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, COLOR_PANEL.a);
    SDL_RenderFillRect(renderer, rect);
}

void menu_update_scrollbar(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    ScrollBar *bar = &ctx->scrollbar;
    scrollbar_set_track(bar, (SDL_Rect){
        ctx->list_rect.x + ctx->list_rect.w - SCROLLBAR_WIDTH,
        ctx->list_rect.y,
        SCROLLBAR_WIDTH,
        ctx->list_rect.h
    });
    scrollbar_set_content(bar,
                          menu_preset_total_height(ctx),
                          (float)ctx->list_rect.h);
}

void menu_draw_button(SDL_Renderer *renderer,
                      const SDL_Rect *rect,
                      const char *label,
                      TTF_Font *font,
                      bool selected) {
    SDL_Color color = selected ? COLOR_BUTTON_BG_ACTIVE : COLOR_BUTTON_BG;
    SDL_Color border = border_for_background(color);
    SDL_Color text = choose_readable_text(color, COLOR_TEXT);
    if (selected) {
        color = ensure_fill_contrast(color, text, COLOR_PANEL);
        border = border_for_background(color);
        text = choose_readable_text(color, COLOR_TEXT);
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, rect);
    menu_draw_text(renderer, font, label, rect->x + 12, rect->y + 14, text);
}

void menu_draw_text_input(SDL_Renderer *renderer,
                          TTF_Font *font,
                          const SDL_Rect *rect,
                          const TextInputField *field) {
    SDL_Color fill = ensure_fill_contrast(COLOR_BUTTON_BG, COLOR_TEXT, COLOR_PANEL);
    SDL_Color border = ensure_fill_contrast(COLOR_ACCENT, COLOR_TEXT, COLOR_PANEL);
    SDL_Color text_color = choose_readable_text(fill, COLOR_TEXT);
    SDL_Color caret = choose_readable_text(fill, COLOR_TEXT);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 250);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(renderer, rect);

    const char *value = text_input_value(field);
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, value, text_color);
    int text_w = 0;
    if (surf) {
        text_w = surf->w;
        SDL_Rect dst = {rect->x + 8, rect->y + rect->h / 2 - surf->h / 2,
                        surf->w, surf->h};
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
    if (field->caret_visible) {
        int caret_x = rect->x + 8 + text_w + 2;
        SDL_SetRenderDrawColor(renderer, caret.r, caret.g, caret.b, 220);
        SDL_RenderDrawLine(renderer, caret_x, rect->y + 8,
                           caret_x, rect->y + rect->h - 8);
    }
}

void menu_draw_toggle(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const SDL_Rect *rect,
                      const char *label,
                      bool enabled) {
    if (!renderer || !rect || !label || !font) return;
    SDL_Color fill = enabled ? COLOR_ACCENT : COLOR_PANEL;
    SDL_Color text_color;
    if (enabled) {
        fill = ensure_fill_contrast(fill, COLOR_TEXT, COLOR_PANEL);
    }
    SDL_Color border = border_for_background(fill);
    text_color = enabled
                     ? choose_readable_text(fill, COLOR_TEXT)
                     : choose_readable_text(fill, COLOR_TEXT_DIM);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, rect);
    menu_draw_text(renderer, font, label, rect->x + 10, rect->y + 10, text_color);
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

void menu_draw_preset_list(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer *renderer = ctx->renderer;
    SDL_Rect panel = ctx->list_rect;
    menu_draw_panel(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &panel);

#if !USE_VULKAN
    SDL_Rect clip = {
        panel.x + 2,
        panel.y + 2,
        panel.w - 4,
        panel.h - 4
    };
    SDL_RenderSetClipRect(renderer, &clip);
#endif

    int count = menu_visible_slot_count(ctx);
    int total_rows = count + 1;

    for (int row = 0; row < total_rows; ++row) {
        bool is_add_entry = (row == count);
        SDL_Rect row_rect;
        if (!menu_preset_row_rect(ctx, row, is_add_entry, &row_rect)) continue;
        int slot_index = is_add_entry ? -1 : menu_slot_index_from_visible_row(ctx, row);
        bool selected = (!is_add_entry &&
                         slot_index >= 0 &&
                         ctx->selection->custom_slot_index == slot_index);
        bool hovered = (!is_add_entry && slot_index >= 0 && ctx->hover_slot == slot_index) ||
                       (is_add_entry && ctx->hover_add_entry);

        if (!is_add_entry) {
            SDL_Color row_color = COLOR_PANEL;
            SDL_Color border = border_for_background(row_color);
            SDL_Color text_color = choose_readable_text(row_color, COLOR_TEXT);
            if (selected) row_color = COLOR_ACCENT;
            else if (hovered) row_color = lighten_color(COLOR_PANEL, 0.25f);
            if (selected) {
                row_color = ensure_fill_contrast(row_color, COLOR_TEXT, COLOR_PANEL);
            }
            border = border_for_background(row_color);
            text_color = choose_readable_text(row_color, COLOR_TEXT);

            SDL_SetRenderDrawColor(renderer, row_color.r, row_color.g, row_color.b, 255);
            SDL_RenderFillRect(renderer, &row_rect);
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRect(renderer, &row_rect);

            SDL_Rect label_rect = {
                .x = row_rect.x + 12,
                .y = row_rect.y + 10,
                .w = row_rect.w - 60,
                .h = row_rect.h - 20
            };

            if (ctx->rename_input.active && slot_index >= 0 && ctx->renaming_slot == slot_index) {
                menu_draw_text_input(renderer, ctx->font, &label_rect, &ctx->rename_input);
            } else {
                const CustomPresetSlot *slot =
                    preset_library_get_slot_const(ctx->library, slot_index);
                const char *label = (slot && slot->name[0] != '\0')
                                        ? slot->name
                                        : "Untitled Preset";
                menu_draw_text(renderer, ctx->font, label,
                               label_rect.x, label_rect.y, text_color);
            }

            SDL_Rect delete_rect = menu_preset_delete_button_rect(&row_rect);
            SDL_Color del_color = hovered ? (SDL_Color){200, 110, 110, 255}
                                          : COLOR_TEXT_DIM;
            SDL_SetRenderDrawColor(renderer, del_color.r, del_color.g, del_color.b, del_color.a);
            SDL_RenderDrawRect(renderer, &delete_rect);
            menu_draw_text(renderer, ctx->font_small, "x",
                           delete_rect.x + 6,
                           delete_rect.y + 4,
                           del_color);
        } else {
            SDL_Color text_color = hovered ? COLOR_TEXT : COLOR_TEXT_DIM;
            char label[64];
            snprintf(label, sizeof(label), "+ Add %s preset", menu_mode_label(ctx->active_mode));
            menu_draw_text(renderer,
                           ctx->font_small,
                           label,
                           row_rect.x + 12,
                           row_rect.y + (row_rect.h / 2) - 8,
                           text_color);
        }
    }

#if !USE_VULKAN
    SDL_RenderSetClipRect(renderer, NULL);
#endif
    scrollbar_draw(renderer, &ctx->scrollbar);
}

SDL_Color menu_color_bg(void) {
    return COLOR_BG;
}

SDL_Color menu_color_panel(void) {
    return COLOR_PANEL;
}

SDL_Color menu_color_text(void) {
    return COLOR_TEXT;
}

SDL_Color menu_color_text_dim(void) {
    return COLOR_TEXT_DIM;
}

SDL_Color menu_color_accent(void) {
    return COLOR_ACCENT;
}
