#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_model.h"
#include "app/menu/menu_render.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "vk_renderer.h"

static SDL_Color COLOR_BG        = {20, 22, 26, 255};
static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_FIELD_ACTIVE = {24, 26, 33, 255};
static SDL_Color COLOR_FIELD_BORDER = {90, 170, 255, 255};

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

static void refresh_panel_theme(void) {
    COLOR_BG = menu_color_bg();
    COLOR_PANEL = menu_color_panel();
    COLOR_TEXT = menu_color_text();
    COLOR_TEXT_DIM = menu_color_text_dim();
    COLOR_FIELD_BORDER = menu_color_accent();
    COLOR_FIELD_ACTIVE = lighten_color(COLOR_PANEL, 0.08f);
}

static void draw_text(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const char *text,
                      int x,
                      int y,
                      SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Rect dst = {x, y, surf->w, surf->h};
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

static void draw_object_list(SceneEditorState *state) {
    if (!state) return;
    refresh_panel_theme();
    int row_h = state->list_view.row_height;
    int x = state->list_rect.x + 6;
    int y_start = state->list_rect.y - (int)editor_list_view_offset(&state->list_view);
    int text_h = state->font_small ? TTF_FontHeight(state->font_small) : 16;
    if (text_h < 12) text_h = 12;
    SDL_Color text = COLOR_TEXT;
    int total_rows = (int)state->working.object_count;
    editor_list_view_set_rows(&state->list_view, total_rows);
    for (int i = 0; i < total_rows; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->list_rect.y || y > state->list_rect.y + state->list_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_object) {
            SDL_Color accent = menu_color_accent();
            bg = (SDL_Color){accent.r, accent.g, accent.b, 80};
        } else if (i == state->hover_object) {
            SDL_Color hover = lighten_color(COLOR_PANEL, 0.12f);
            bg = (SDL_Color){hover.r, hover.g, hover.b, 90};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->list_rect.x + 2, y + 2, state->list_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const PresetObject *obj = &state->working.objects[i];
        const char *label = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
        int em_idx = emitter_index_for_object(state, i);
        SDL_Color col = text;
        char line[64];
        char line_fit[64];
        snprintf(line, sizeof(line), "%s %s%s",
                 label,
                 obj->gravity_enabled ? "[G]" : "[ ]",
                 (em_idx >= 0) ? " (Emitter)" : "");
        fit_text_to_width(state->font_small,
                          line,
                          state->list_rect.w - 26,
                          line_fit,
                          sizeof(line_fit));
        draw_text(state->renderer,
                  state->font_small,
                  line_fit[0] ? line_fit : line,
                  x,
                  y + (row_h - text_h) / 2,
                  col);
    }
    SDL_Color track = COLOR_BG;
    track.a = 180;
    SDL_Color thumb = menu_color_accent();
    thumb.a = 220;
    editor_list_view_draw(state->renderer, &state->list_view, track, thumb);
}

static void draw_hover_tooltip(SceneEditorState *state) {
    if (!state || !state->renderer || !state->font_small) return;
    if (state->pointer_x < 0 || state->pointer_y < 0) return;

    const char *lines[3] = {0};
    char buf1[64];
    char buf2[64];
    char buf3[64];
    int count = 0;

    if (state->hover_emitter >= 0 &&
        state->hover_emitter < (int)state->working.emitter_count) {
        const FluidEmitter *em = &state->working.emitters[state->hover_emitter];
        snprintf(buf1, sizeof(buf1), "%s emitter", emitter_type_name(em->type));
        snprintf(buf2, sizeof(buf2), "Radius %.2f  Strength %.1f", em->radius, em->strength);
        snprintf(buf3, sizeof(buf3), "Pos %.2f, %.2f", em->position_x, em->position_y);
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        count = 3;
    } else if (state->hover_object >= 0 &&
               state->hover_object < (int)state->working.object_count) {
        const PresetObject *obj = &state->working.objects[state->hover_object];
        const char *type = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
        snprintf(buf1, sizeof(buf1), "Object: %s", type);
        snprintf(buf2, sizeof(buf2), "Size %.2f x %.2f", obj->size_x, obj->size_y);
        snprintf(buf3, sizeof(buf3), "Pos %.2f, %.2f", obj->position_x, obj->position_y);
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        count = 3;
        int em_idx = emitter_index_for_object(state, state->hover_object);
        if (em_idx >= 0 && em_idx < (int)state->working.emitter_count) {
            const FluidEmitter *em = &state->working.emitters[em_idx];
            snprintf(buf3, sizeof(buf3), "Emitter: %s (r=%.2f, s=%.1f)",
                     emitter_type_name(em->type), em->radius, em->strength);
            lines[2] = buf3;
            count = 3;
        }
    } else if (state->boundary_mode && state->boundary_hover_edge >= 0 &&
               state->boundary_hover_edge < BOUNDARY_EDGE_COUNT) {
        int edge = state->boundary_hover_edge;
        const BoundaryFlow *flow = &state->working.boundary_flows[edge];
        snprintf(buf1, sizeof(buf1), "Edge: %s", boundary_edge_name(edge));
        snprintf(buf2, sizeof(buf2), "Mode: %s", boundary_mode_label(flow->mode));
        lines[0] = buf1;
        lines[1] = buf2;
        count = 2;
        if (flow->mode != BOUNDARY_FLOW_DISABLED) {
            snprintf(buf3, sizeof(buf3), "Strength %.1f", flow->strength);
            lines[2] = buf3;
            count = 3;
        }
    }

    if (count > 0) {
        scene_editor_canvas_draw_tooltip(state->renderer,
                                         state->font_small,
                                         state->pointer_x,
                                         state->pointer_y,
                                         lines,
                                         count);
    }
}

static void draw_import_list(SceneEditorState *state) {
    if (!state) return;
    refresh_panel_theme();
    int row_h = state->import_view.row_height;
    int x = state->import_rect.x + 6;
    int y_start = state->import_rect.y - (int)editor_list_view_offset(&state->import_view);
    int text_h = state->font_small ? TTF_FontHeight(state->font_small) : 16;
    if (text_h < 12) text_h = 12;
    SDL_Color text = COLOR_TEXT;
    editor_list_view_set_rows(&state->import_view, state->import_file_count);
    for (int i = 0; i < state->import_file_count; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->import_rect.y || y > state->import_rect.y + state->import_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_import_row) {
            SDL_Color accent = menu_color_accent();
            bg = (SDL_Color){accent.r, accent.g, accent.b, 80};
        } else if (i == state->hover_import_row) {
            SDL_Color hover = lighten_color(COLOR_PANEL, 0.12f);
            bg = (SDL_Color){hover.r, hover.g, hover.b, 90};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->import_rect.x + 2, y + 2, state->import_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const char *label = state->import_files[i][0] ? state->import_files[i] : "(empty)";
        char label_fit[256];
        fit_text_to_width(state->font_small,
                          label,
                          state->import_rect.w - 24,
                          label_fit,
                          sizeof(label_fit));
        draw_text(state->renderer,
                  state->font_small,
                  label_fit,
                  x,
                  y + (row_h - text_h) / 2,
                  text);
    }
    SDL_Color track = COLOR_BG;
    track.a = 180;
    SDL_Color thumb = menu_color_accent();
    thumb.a = 220;
    editor_list_view_draw(state->renderer, &state->import_view, track, thumb);
}

static void draw_dimension_field(SceneEditorState *state,
                                 const SDL_Rect *rect,
                                 const char *label,
                                 float value,
                                 bool editing,
                                 const TextInputField *input) {
    if (!state || !rect) return;
    SDL_Renderer *renderer = state->renderer;
    TTF_Font *font = state->font_small ? state->font_small : state->font_main;
    if (!renderer || !font) return;
    SDL_Color bg = editing ? COLOR_FIELD_ACTIVE : COLOR_PANEL;
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, COLOR_FIELD_BORDER.r, COLOR_FIELD_BORDER.g, COLOR_FIELD_BORDER.b,
                           editing ? 255 : 140);
    SDL_RenderDrawRect(renderer, rect);
    int label_height = TTF_FontHeight(font);
    int label_y = rect->y - label_height - 2;
    char label_fit[64];
    char value_fit[64];
    const char *draw_label = label ? label : "";
    const char *draw_value = NULL;
    int max_text_w = rect->w - 8;
    if (max_text_w < 8) max_text_w = 8;
    if (label_y < 0) label_y = 0;
    fit_text_to_width(font, draw_label, max_text_w, label_fit, sizeof(label_fit));
    draw_text(renderer,
              font,
              label_fit[0] ? label_fit : draw_label,
              rect->x + 2,
              label_y,
              COLOR_TEXT_DIM);
    char value_buf[32];
    const char *value_text = value_buf;
    if (editing && input) {
        value_text = text_input_value(input);
        if (!value_text) value_text = "";
    } else {
        snprintf(value_buf, sizeof(value_buf), "%.2f", value);
    }
    fit_text_to_width(font, value_text, rect->w - 12, value_fit, sizeof(value_fit));
    draw_value = value_fit[0] ? value_fit : value_text;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, draw_value, COLOR_TEXT);
    int text_w = 0;
    int text_h = 0;
    if (surf) {
        text_w = surf->w;
        text_h = surf->h;
        int text_y = rect->y + rect->h / 2 - text_h / 2 + 2;
        SDL_Rect dst = {rect->x + 6, text_y, surf->w, surf->h};
        VkRendererTexture vk_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       surf,
                                                       &vk_tex,
                                                       VK_FILTER_LINEAR) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &vk_tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &vk_tex);
        }
        SDL_FreeSurface(surf);
    }
    if (editing && input && input->caret_visible) {
        int caret_x = rect->x + 6 + text_w + 2;
        int caret_y0 = rect->y + 4;
        int caret_y1 = rect->y + rect->h - 4;
        SDL_SetRenderDrawColor(renderer, COLOR_FIELD_BORDER.r, COLOR_FIELD_BORDER.g, COLOR_FIELD_BORDER.b, 255);
        SDL_RenderDrawLine(renderer, caret_x, caret_y0, caret_x, caret_y1);
    }
}

static void draw_dimension_fields(SceneEditorState *state) {
    if (!state) return;
    draw_dimension_field(state,
                         &state->width_rect,
                         "Width",
                         state->working.domain_width,
                         state->editing_width,
                         &state->width_input);
    draw_dimension_field(state,
                         &state->height_rect,
                         "Height",
                         state->working.domain_height,
                         state->editing_height,
                         &state->height_input);
}

void scene_editor_panel_draw(SceneEditorState *state) {
    if (!state || !state->renderer) return;
    refresh_panel_theme();
    SDL_Renderer *renderer = state->renderer;
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    int win_w = 0;
    int win_h = 0;
    if (state->window) {
        SDL_GetWindowSize(state->window, &win_w, &win_h);
    }
    if (win_w <= 0 || win_h <= 0) {
        win_w = state->panel_rect.x + state->panel_rect.w;
        win_h = state->panel_rect.y + state->panel_rect.h;
    }
    SDL_Rect clear_rect = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &clear_rect);

    scene_editor_canvas_draw_background(renderer,
                                        state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        false,
                                        0.0f,
                                        0.0f);

    const char *title = state->name_edit_ptr
                            ? state->name_edit_ptr
                            : (state->working.name ? state->working.name : "Untitled Preset");
    scene_editor_canvas_draw_name(renderer,
                                  state->canvas_x,
                                  state->canvas_y,
                                  state->canvas_width,
                                  state->canvas_height,
                                  state->font_main ? state->font_main : state->font_small,
                                  state->font_small,
                                  title,
                                  state->renaming_name,
                                  &state->name_input);

#if !USE_VULKAN
    SDL_Rect canvas_rect = {
        .x = state->canvas_x,
        .y = state->canvas_y,
        .w = state->canvas_width,
        .h = state->canvas_height
    };
    SDL_RenderSetClipRect(renderer, &canvas_rect);
#endif

    scene_editor_canvas_draw_boundary_flows(renderer,
                                            state->canvas_x,
                                            state->canvas_y,
                                            state->canvas_width,
                                            state->canvas_height,
                                            state->working.boundary_flows,
                                            state->boundary_hover_edge,
                                            state->boundary_selected_edge,
                                            state->boundary_mode);

    scene_editor_canvas_draw_imports(renderer, state);

    scene_editor_canvas_draw_objects(renderer,
                                     state->canvas_x,
                                     state->canvas_y,
                                     state->canvas_width,
                                     state->canvas_height,
                                     &state->working,
                                     state->selected_object,
                                     state->hover_object,
                                     state->emitter_object_map);

    scene_editor_canvas_draw_emitters(renderer,
                                      state->canvas_x,
                                      state->canvas_y,
                                      state->canvas_width,
                                      state->canvas_height,
                                      &state->working,
                                      state->selected_emitter,
                                      state->hover_emitter,
                                      state->font_small,
                                      state->emitter_object_map,
                                      state->emitter_import_map);

#if !USE_VULKAN
    SDL_RenderSetClipRect(renderer, NULL);
#endif

    draw_hover_tooltip(state);

    int info_x = state->canvas_x;
    int info_y = state->canvas_y + state->canvas_height + 20;
    int info_step = state->font_small ? TTF_FontHeight(state->font_small) + 8 : 26;
    if (info_step < 24) info_step = 24;
    if (win_h > 0) {
        int max_info_y = win_h - info_step * 3 - 8;
        if (info_y > max_info_y) info_y = max_info_y;
        if (info_y < state->canvas_y + state->canvas_height + 6) {
            info_y = state->canvas_y + state->canvas_height + 6;
        }
    }
    draw_text(renderer, state->font_small,
              "Shortcuts: Tab emitters, arrows nudge, +/- radius/size, Delete removes",
              info_x,
              info_y,
              COLOR_TEXT_DIM);
    draw_text(renderer, state->font_small,
              "Drag objects/emitters on canvas. Save applies changes. Esc cancels.",
              info_x,
              info_y + info_step,
              COLOR_TEXT_DIM);
    const char *boundary_hint = state->boundary_mode
                                    ? "Air Flow Mode ON: Click edges, E=emit, R=recv, X=clear"
                                    : "Air Flow Mode OFF: Click button to edit edge flows";
    draw_text(renderer, state->font_small,
              boundary_hint,
              info_x,
              info_y + info_step * 2,
              COLOR_TEXT_DIM);

    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &state->panel_rect);
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 200);
    SDL_RenderDrawRect(renderer, &state->panel_rect);
    draw_dimension_fields(state);

    draw_text(renderer, state->font_main,
              "Emitter Details",
              state->panel_rect.x + 12,
              state->panel_rect.y + 12,
              COLOR_TEXT);

    const FluidEmitter *selected_em = NULL;
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        selected_em = &state->working.emitters[state->selected_emitter];
    }
    scene_editor_draw_numeric_field(renderer, state->font_small,
                                    &state->radius_field, selected_em);
    scene_editor_draw_numeric_field(renderer, state->font_small,
                                    &state->strength_field, selected_em);

    scene_editor_draw_button(renderer, &state->btn_add_source, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_add_jet, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_add_sink, state->font_small);
    if (state->showing_import_picker) {
        scene_editor_draw_button(renderer, &state->btn_import_back, state->font_small);
    } else {
        scene_editor_draw_button(renderer, &state->btn_add_import, state->font_small);
    }
    scene_editor_draw_button(renderer, &state->btn_boundary, state->font_small);

    SDL_Rect list_rect = state->list_rect;
    SDL_Rect import_rect = state->import_rect;
    SDL_Color list_fill = lighten_color(COLOR_PANEL, 0.04f);
    SDL_SetRenderDrawColor(renderer, list_fill.r, list_fill.g, list_fill.b, 255);
    SDL_RenderFillRect(renderer, &list_rect);
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderDrawRect(renderer, &list_rect);

    if (state->showing_import_picker) {
        SDL_Color import_fill = lighten_color(COLOR_PANEL, 0.08f);
        SDL_SetRenderDrawColor(renderer, import_fill.r, import_fill.g, import_fill.b, 255);
        SDL_RenderFillRect(renderer, &import_rect);
        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderDrawRect(renderer, &import_rect);
        {
            int label_h = state->font_small ? TTF_FontHeight(state->font_small) : 16;
            draw_text(renderer, state->font_small, "Import files", import_rect.x, import_rect.y - label_h - 6, COLOR_TEXT_DIM);
        }
        draw_import_list(state);
    } else {
        {
            int label_h = state->font_small ? TTF_FontHeight(state->font_small) : 16;
            draw_text(renderer, state->font_small, "Objects", list_rect.x, list_rect.y - label_h - 6, COLOR_TEXT_DIM);
        }
        draw_object_list(state);
    }

    scene_editor_draw_button(renderer, &state->btn_save, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_cancel, state->font_small);
}
