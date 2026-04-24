#include "app/menu/menu_render.h"

#include <stdio.h>
#include <string.h>

#include "app/menu/menu_state.h"
#include "render/text_draw.h"

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

static bool menu_rect_is_hovered(const SDL_Rect *rect) {
    int mouse_x = 0;
    int mouse_y = 0;
    if (!rect) return false;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    return mouse_x >= rect->x &&
           mouse_x < (rect->x + rect->w) &&
           mouse_y >= rect->y &&
           mouse_y < (rect->y + rect->h);
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
    (void)physics_sim_text_draw_utf8_at(renderer, font, text, x, y, color);
}

void menu_draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect) {
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, COLOR_PANEL.a);
    SDL_RenderFillRect(renderer, rect);
}

static int menu_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    if (!font) return fallback;
    {
        int h = TTF_FontHeight(font);
        if (h > 0) {
            return physics_sim_text_logical_pixels(renderer, h);
        }
    }
    return fallback;
}

static void menu_fit_text_to_width(SDL_Renderer *renderer,
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
    int text_w = 0;
    int text_h = 0;
    int inset_x = 12;
    char label_fit[256];
    const char *draw_label = label;
    if (!renderer || !rect || !label || !font) return;
    bool hovered = menu_rect_is_hovered(rect);
    SDL_Color color = selected ? COLOR_BUTTON_BG_ACTIVE : COLOR_BUTTON_BG;
    if (hovered) {
        color = selected ? lighten_color(color, 0.10f)
                         : lighten_color(color, 0.18f);
    }
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
    if (font && label) {
        int max_label_w = rect->w - 16;
        if (max_label_w < 8) max_label_w = 8;
        menu_fit_text_to_width(renderer, font, label, max_label_w, label_fit, sizeof(label_fit));
        draw_label = label_fit[0] ? label_fit : label;
        if (TTF_SizeUTF8(font, draw_label, &text_w, &text_h) != 0) {
            text_w = 0;
            text_h = menu_font_height(renderer, font, rect->h / 2);
        } else {
            text_w = physics_sim_text_logical_pixels(renderer, text_w);
            text_h = physics_sim_text_logical_pixels(renderer, text_h);
        }
        if (text_w > 0 && text_w <= rect->w - 16) {
            inset_x = (rect->w - text_w) / 2;
            if (inset_x < 8) inset_x = 8;
        }
        menu_draw_text(renderer,
                       font,
                       draw_label,
                       rect->x + inset_x,
                       rect->y + (rect->h - text_h) / 2,
                       text);
    }
}

void menu_draw_text_input(SDL_Renderer *renderer,
                          TTF_Font *font,
                          const SDL_Rect *rect,
                          const TextInputField *field) {
    int text_h = 12;
    if (!renderer || !font || !rect || !field) return;
    text_h = menu_font_height(renderer, font, rect->h - 12);
    SDL_Color fill = ensure_fill_contrast(COLOR_BUTTON_BG, COLOR_TEXT, COLOR_PANEL);
    SDL_Color border = ensure_fill_contrast(COLOR_ACCENT, COLOR_TEXT, COLOR_PANEL);
    SDL_Color text_color = choose_readable_text(fill, COLOR_TEXT);
    SDL_Color caret = choose_readable_text(fill, COLOR_TEXT);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 250);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(renderer, rect);

    const char *value = text_input_value(field);
    char value_fit[256];
    int max_value_w = rect->w - 16;
    if (max_value_w < 8) max_value_w = 8;
    menu_fit_text_to_width(renderer, font, value, max_value_w, value_fit, sizeof(value_fit));
    if (!value_fit[0]) snprintf(value_fit, sizeof(value_fit), "%s", value ? value : "");
    int text_w = 0;
    int text_h_logical = 0;
    if (physics_sim_text_measure_utf8(renderer, font, value_fit, &text_w, &text_h_logical)) {
        SDL_Rect dst = {rect->x + 8, rect->y + rect->h / 2 - text_h_logical / 2,
                        text_w, text_h_logical};
        (void)physics_sim_text_draw_utf8(renderer, font, value_fit, text_color, &dst);
    }
    if (field->caret_visible) {
        int caret_x = rect->x + 8 + text_w + 2;
        int caret_pad = (rect->h - text_h) / 2;
        if (caret_pad < 4) caret_pad = 4;
        SDL_SetRenderDrawColor(renderer, caret.r, caret.g, caret.b, 220);
        SDL_RenderDrawLine(renderer, caret_x, rect->y + caret_pad,
                           caret_x, rect->y + rect->h - caret_pad);
    }
}

void menu_draw_toggle(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const SDL_Rect *rect,
                      const char *label,
                      bool enabled) {
    int text_w = 0;
    int text_h = 12;
    int text_x = 0;
    char label_fit[256];
    const char *draw_label = label;
    if (!renderer || !rect || !label || !font) return;
    text_h = menu_font_height(renderer, font, rect->h / 2);
    text_x = rect->x + 10;
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
    {
        int max_label_w = rect->w - 20;
        if (max_label_w < 8) max_label_w = 8;
        menu_fit_text_to_width(renderer, font, label, max_label_w, label_fit, sizeof(label_fit));
        draw_label = label_fit[0] ? label_fit : label;
    }
    if (TTF_SizeUTF8(font, draw_label, &text_w, &text_h) == 0) {
        text_w = physics_sim_text_logical_pixels(renderer, text_w);
        text_h = physics_sim_text_logical_pixels(renderer, text_h);
    }
    if (text_w > 0 && text_w <= rect->w - 20) {
        text_x = rect->x + (rect->w - text_w) / 2;
    }
    menu_draw_text(renderer,
                   font,
                   draw_label,
                   text_x,
                   rect->y + (rect->h - text_h) / 2,
                   text_color);
}

void menu_draw_preset_list(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer *renderer = ctx->renderer;
    TTF_Font *small_font = ctx->font_small ? ctx->font_small : ctx->font;
    SDL_Rect panel = ctx->list_rect;
    menu_draw_panel(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Rect clip = {
        panel.x + 2,
        panel.y + 2,
        panel.w - 4,
        panel.h - 4
    };
    SDL_RenderSetClipRect(renderer, &clip);

    bool retained_catalog = menu_showing_retained_catalog(ctx);
    int count = menu_visible_entry_count(ctx);
    int total_rows = count + (retained_catalog ? 0 : 1);

    for (int row = 0; row < total_rows; ++row) {
        bool is_add_entry = (!retained_catalog && row == count);
        SDL_Rect row_rect;
        if (!menu_preset_row_rect(ctx, row, is_add_entry, &row_rect)) continue;
        int slot_index = is_add_entry ? -1 : menu_slot_index_from_visible_row(ctx, row);
        int retained_scene_index = is_add_entry ? -1 : row;
        bool selected = false;
        bool hovered = false;
        if (!is_add_entry && retained_catalog) {
            selected = ctx->selection && ctx->selection->retained_scene_index == retained_scene_index;
            hovered = ctx->hover_retained_scene_index == retained_scene_index;
        } else if (!is_add_entry) {
            selected = slot_index >= 0 && ctx->selection->custom_slot_index == slot_index;
            hovered = slot_index >= 0 && ctx->hover_slot == slot_index;
        } else {
            hovered = ctx->hover_add_entry;
        }

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
                .y = row_rect.y + 8,
                .w = row_rect.w - 60,
                .h = row_rect.h - 16
            };
            if (label_rect.h < 16) label_rect.h = 16;

            if (ctx->rename_input.active && slot_index >= 0 && ctx->renaming_slot == slot_index) {
                menu_draw_text_input(renderer, ctx->font, &label_rect, &ctx->rename_input);
            } else {
                const char *label = "Untitled Preset";
                char label_buf[256];
                int label_h = menu_font_height(renderer, ctx->font, 16);
                if (retained_catalog) {
                    const PhysicsSimSceneLibraryEntry *entry =
                        (retained_scene_index >= 0 &&
                         retained_scene_index < ctx->scene_library.retained_scenes.count)
                            ? &ctx->scene_library.retained_scenes.entries[retained_scene_index]
                            : NULL;
                    label = (entry && entry->display_name[0] != '\0')
                                ? entry->display_name
                                : "Untitled 3D Scene";
                } else {
                    const CustomPresetSlot *slot =
                        preset_library_get_slot_const(ctx->library, slot_index);
                    label = (slot && slot->name[0] != '\0')
                                ? slot->name
                                : "Untitled Preset";
                }
                menu_fit_text_to_width(renderer,
                                       ctx->font,
                                       label,
                                       label_rect.w - 6,
                                       label_buf,
                                       sizeof(label_buf));
                menu_draw_text(renderer, ctx->font, label_buf,
                               label_rect.x,
                               label_rect.y + (label_rect.h - label_h) / 2,
                               text_color);
            }

            if (!retained_catalog) {
                SDL_Rect delete_rect = menu_preset_delete_button_rect(&row_rect);
                SDL_Color del_color = hovered ? (SDL_Color){200, 110, 110, 255}
                                              : COLOR_TEXT_DIM;
                SDL_SetRenderDrawColor(renderer, del_color.r, del_color.g, del_color.b, del_color.a);
                SDL_RenderDrawRect(renderer, &delete_rect);
                if (small_font) {
                    int x_w = 0;
                    int x_h = menu_font_height(renderer, small_font, delete_rect.h - 8);
                    if (TTF_SizeUTF8(small_font, "x", &x_w, &x_h) != 0) {
                        x_w = delete_rect.w / 2;
                    } else {
                        x_w = physics_sim_text_logical_pixels(renderer, x_w);
                        x_h = physics_sim_text_logical_pixels(renderer, x_h);
                    }
                    menu_draw_text(renderer,
                                   small_font,
                                   "x",
                                   delete_rect.x + (delete_rect.w - x_w) / 2,
                                   delete_rect.y + (delete_rect.h - x_h) / 2,
                                   del_color);
                }
            }
        } else {
            SDL_Color text_color = hovered ? COLOR_TEXT : COLOR_TEXT_DIM;
            char label[64];
            char add_buf[64];
            snprintf(label, sizeof(label), "+ Add %s preset", menu_mode_label(ctx->active_mode));
            if (small_font) {
                int add_h = menu_font_height(renderer, small_font, 16);
                int add_w = 0;
                int add_x = row_rect.x + 12;
                menu_fit_text_to_width(renderer, small_font, label, row_rect.w - 24, add_buf, sizeof(add_buf));
                if (TTF_SizeUTF8(small_font, add_buf, &add_w, &add_h) == 0) {
                    add_w = physics_sim_text_logical_pixels(renderer, add_w);
                    add_h = physics_sim_text_logical_pixels(renderer, add_h);
                }
                if (add_w > 0 && add_w < row_rect.w - 24) {
                    add_x = row_rect.x + (row_rect.w - add_w) / 2;
                }
                menu_draw_text(renderer,
                               small_font,
                               add_buf,
                               add_x,
                               row_rect.y + (row_rect.h - add_h) / 2,
                               text_color);
            }
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
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

SDL_Color menu_color_button_bg(void) {
    return COLOR_BUTTON_BG;
}

SDL_Color menu_color_button_bg_active(void) {
    return COLOR_BUTTON_BG_ACTIVE;
}
