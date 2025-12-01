#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_model.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

static SDL_Color COLOR_BG        = {20, 22, 26, 255};
static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_FIELD_ACTIVE = {24, 26, 33, 255};
static SDL_Color COLOR_FIELD_BORDER = {90, 170, 255, 255};

static void draw_text(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const char *text,
                      int x,
                      int y,
                      SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_object_list(SceneEditorState *state) {
    if (!state) return;
    int row_h = state->list_view.row_height;
    int x = state->list_rect.x + 6;
    int y_start = state->list_rect.y - (int)editor_list_view_offset(&state->list_view);
    SDL_Color text = COLOR_TEXT;
    int total_rows = (int)state->working.import_shape_count;
    editor_list_view_set_rows(&state->list_view, total_rows);
    for (int i = 0; i < total_rows; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->list_rect.y || y > state->list_rect.y + state->list_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_row) {
            bg = (SDL_Color){70, 120, 200, 80};
        } else if (i == state->hover_row) {
            bg = (SDL_Color){90, 100, 120, 60};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->list_rect.x + 2, y + 2, state->list_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const ImportedShape *imp = &state->working.import_shapes[i];
        const char *label = imp->path[0] ? imp->path : "Unnamed";
        // Display basename for readability.
        const char *slash = strrchr(label, '/');
        if (slash && *(slash + 1)) {
            label = slash + 1;
        }
        SDL_Color col = imp->enabled ? text : COLOR_TEXT_DIM;
        draw_text(state->renderer, state->font_small, label, x, y + 6, col);
    }
    SDL_Color track = {20, 20, 22, 180};
    SDL_Color thumb = {90, 170, 255, 220};
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
    int row_h = state->import_view.row_height;
    int x = state->import_rect.x + 6;
    int y_start = state->import_rect.y - (int)editor_list_view_offset(&state->import_view);
    SDL_Color text = COLOR_TEXT;
    editor_list_view_set_rows(&state->import_view, state->import_file_count);
    for (int i = 0; i < state->import_file_count; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->import_rect.y || y > state->import_rect.y + state->import_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_import_row) {
            bg = (SDL_Color){70, 120, 200, 80};
        } else if (i == state->hover_import_row) {
            bg = (SDL_Color){90, 100, 120, 60};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->import_rect.x + 2, y + 2, state->import_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const char *label = state->import_files[i][0] ? state->import_files[i] : "(empty)";
        draw_text(state->renderer, state->font_small, label, x, y + 6, text);
    }
    SDL_Color track = {20, 20, 22, 180};
    SDL_Color thumb = {90, 170, 255, 220};
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
    if (label_y < 0) label_y = 0;
    draw_text(renderer, font, label, rect->x + 2, label_y, COLOR_TEXT_DIM);
    char value_buf[32];
    const char *value_text = value_buf;
    if (editing && input) {
        value_text = text_input_value(input);
        if (!value_text) value_text = "";
    } else {
        snprintf(value_buf, sizeof(value_buf), "%.2f", value);
    }
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, value_text, COLOR_TEXT);
    int text_w = 0;
    int text_h = 0;
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            text_w = surf->w;
            text_h = surf->h;
            int text_y = rect->y + rect->h / 2 - text_h / 2 + 2;
            SDL_Rect dst = {rect->x + 6, text_y, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
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
    SDL_Renderer *renderer = state->renderer;
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer);

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

    SDL_Rect canvas_rect = {
        .x = state->canvas_x,
        .y = state->canvas_y,
        .w = state->canvas_width,
        .h = state->canvas_height
    };
    SDL_RenderSetClipRect(renderer, &canvas_rect);

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
                                     state->hover_object);

    scene_editor_canvas_draw_emitters(renderer,
                                      state->canvas_x,
                                      state->canvas_y,
                                      state->canvas_width,
                                      state->canvas_height,
                                      &state->working,
                                      state->selected_emitter,
                                      state->hover_emitter,
                                      state->font_small,
                                      state->emitter_object_map);

    SDL_RenderSetClipRect(renderer, NULL);

    draw_hover_tooltip(state);

    int info_x = state->canvas_x;
    int info_y = state->canvas_y + state->canvas_height + 20;
    draw_text(renderer, state->font_small,
              "Shortcuts: Tab emitters, arrows nudge, +/- radius/size, Delete removes",
              info_x,
              info_y,
              COLOR_TEXT_DIM);
    draw_text(renderer, state->font_small,
              "Drag objects/emitters on canvas. Save applies changes. Esc cancels.",
              info_x,
              info_y + 26,
              COLOR_TEXT_DIM);
    const char *boundary_hint = state->boundary_mode
                                    ? "Air Flow Mode ON: Click edges, E=emit, R=recv, X=clear"
                                    : "Air Flow Mode OFF: Click button to edit edge flows";
    draw_text(renderer, state->font_small,
              boundary_hint,
              info_x,
              info_y + 52,
              COLOR_TEXT_DIM);

    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &state->panel_rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
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
    SDL_SetRenderDrawColor(renderer, 28, 30, 36, 255);
    SDL_RenderFillRect(renderer, &list_rect);
    SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
    SDL_RenderDrawRect(renderer, &list_rect);

    if (state->showing_import_picker) {
        SDL_SetRenderDrawColor(renderer, 34, 36, 44, 255);
        SDL_RenderFillRect(renderer, &import_rect);
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderDrawRect(renderer, &import_rect);
        draw_text(renderer, state->font_small, "Import files", import_rect.x, import_rect.y - 22, COLOR_TEXT_DIM);
        draw_import_list(state);
    } else {
        draw_text(renderer, state->font_small, "Objects", list_rect.x, list_rect.y - 22, COLOR_TEXT_DIM);
        draw_object_list(state);
    }

    scene_editor_draw_button(renderer, &state->btn_save, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_cancel, state->font_small);
}
