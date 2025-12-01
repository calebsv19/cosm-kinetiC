#include "app/editor/scene_editor_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EDITOR_PANEL_WIDTH 280
#define EDITOR_CANVAS_MARGIN 40
#define EDITOR_CANVAS_TOP 90

void set_dirty(SceneEditorState *state) {
    if (state) state->dirty = true;
}

float sanitize_domain_dimension(float value) {
    if (!isfinite(value) || value <= 0.1f) {
        return 0.1f;
    }
    if (value > 64.0f) {
        return 64.0f;
    }
    return value;
}

void editor_update_dimension_rects(SceneEditorState *state) {
    if (!state) return;
    int rect_h = 26;
    int gap = 12;
    int inset = 16;
    int available_w = state->panel_rect.w - inset * 2 - gap;
    if (available_w < 80) available_w = 80;
    int field_w = available_w / 2;
    if (field_w < 60) field_w = 60;
    int rect_y = state->panel_rect.y - rect_h - 8;
    if (rect_y < 20) rect_y = 20;
    int start_x = state->panel_rect.x + inset;
    state->width_rect = (SDL_Rect){start_x, rect_y, field_w, rect_h};
    state->height_rect = (SDL_Rect){start_x + field_w + gap, rect_y, field_w, rect_h};
}

void editor_update_canvas_layout(SceneEditorState *state) {
    if (!state || !state->window) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(state->window, &winW, &winH);
    int panel_width = EDITOR_PANEL_WIDTH;
    int canvas_margin = EDITOR_CANVAS_MARGIN;
    int canvas_top = EDITOR_CANVAS_TOP;
    int max_canvas_h = winH - (canvas_top + 20);
    if (max_canvas_h < 260) max_canvas_h = 260;
    int max_canvas_w = winW - panel_width - (canvas_margin * 2) - 24;
    if (max_canvas_w < 220) max_canvas_w = 220;
    float w_units = sanitize_domain_dimension(state->working.domain_width);
    float h_units = sanitize_domain_dimension(state->working.domain_height);
    float aspect = w_units / h_units;
    if (aspect < 0.2f) aspect = 0.2f;
    if (aspect > 10.0f) aspect = 10.0f;

    int canvas_w = max_canvas_w;
    int canvas_h = (int)lroundf((float)canvas_w / aspect);
    if (canvas_h > max_canvas_h) {
        canvas_h = max_canvas_h;
        canvas_w = (int)lroundf((float)canvas_h * aspect);
    }

    const int min_canvas_w = 220;
    const int min_canvas_h = 180;
    if (canvas_w < min_canvas_w) {
        canvas_w = min_canvas_w;
        canvas_h = (int)lroundf((float)canvas_w / aspect);
    }
    if (canvas_h < min_canvas_h) {
        canvas_h = min_canvas_h;
        canvas_w = (int)lroundf((float)canvas_h * aspect);
        if (canvas_w > max_canvas_w) {
            canvas_w = max_canvas_w;
            canvas_h = (int)lroundf((float)canvas_w / aspect);
        }
    }

    state->canvas_y = canvas_top + (max_canvas_h - canvas_h) / 2;
    state->canvas_x = canvas_margin + (max_canvas_w - canvas_w) / 2;
    state->canvas_width = canvas_w;
    state->canvas_height = canvas_h;
    int panel_x = canvas_margin + max_canvas_w + 24;
    state->panel_rect.x = panel_x;
    state->panel_rect.y = canvas_top;
    state->panel_rect.w = panel_width;
    state->panel_rect.h = max_canvas_h;
    editor_update_dimension_rects(state);
}

SDL_Rect editor_name_rect(const SceneEditorState *state) {
    SDL_Rect rect = {
        .x = state->canvas_x,
        .y = state->canvas_y - 50,
        .w = state->canvas_width,
        .h = 36
    };
    if (rect.y < 20) rect.y = 20;
    return rect;
}

void editor_begin_name_edit(SceneEditorState *state) {
    if (!state || !state->name_edit_ptr || state->name_edit_capacity == 0) return;
    text_input_begin(&state->name_input,
                     state->name_edit_ptr,
                     state->name_edit_capacity - 1);
    state->renaming_name = true;
}

void editor_finish_name_edit(SceneEditorState *state, bool apply) {
    if (!state || !state->renaming_name) return;
    if (apply && state->name_edit_ptr) {
        const char *value = text_input_value(&state->name_input);
        if (!value || value[0] == '\0') {
            state->name_edit_ptr[0] = '\0';
        } else {
            snprintf(state->name_edit_ptr, state->name_edit_capacity, "%s", value);
        }
        state->working.name = state->name_edit_ptr;
        if (state->name_buffer && state->name_buffer != state->name_edit_ptr) {
            snprintf(state->name_buffer, state->name_capacity, "%s", state->name_edit_ptr);
        }
    }
    text_input_end(&state->name_input);
    state->renaming_name = false;
}

void editor_begin_dimension_edit(SceneEditorState *state, bool editing_width) {
    if (!state) return;
    if (editing_width) {
        if (state->editing_height) {
            editor_finish_dimension_edit(state, false, false);
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", state->working.domain_width);
        text_input_begin(&state->width_input, buffer, 10);
        state->editing_width = true;
    } else {
        if (state->editing_width) {
            editor_finish_dimension_edit(state, true, false);
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", state->working.domain_height);
        text_input_begin(&state->height_input, buffer, 10);
        state->editing_height = true;
    }
}

void editor_finish_dimension_edit(SceneEditorState *state,
                                  bool editing_width,
                                  bool apply) {
    if (!state) return;
    TextInputField *field = editing_width ? &state->width_input : &state->height_input;
    if (!field->active) {
        if (editing_width) state->editing_width = false;
        else state->editing_height = false;
        return;
    }
    if (apply) {
        const char *text = text_input_value(field);
        if (text && text[0]) {
            float value = (float)atof(text);
            value = sanitize_domain_dimension(value);
            if (editing_width) {
                if (fabsf(state->working.domain_width - value) > 1e-3f) {
                    state->working.domain_width = value;
                    set_dirty(state);
                }
            } else {
                if (fabsf(state->working.domain_height - value) > 1e-3f) {
                    state->working.domain_height = value;
                    set_dirty(state);
                }
            }
            editor_update_canvas_layout(state);
        }
    }
    text_input_end(field);
    if (editing_width) state->editing_width = false;
    else state->editing_height = false;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void canvas_to_normalized_unclamped(const SceneEditorState *state,
                                    int sx,
                                    int sy,
                                    float *out_x,
                                    float *out_y) {
    if (!state || !out_x || !out_y ||
        state->canvas_width <= 0 || state->canvas_height <= 0) return;
    *out_x = (float)(sx - state->canvas_x) / (float)state->canvas_width;
    *out_y = (float)(sy - state->canvas_y) / (float)state->canvas_height;
}
