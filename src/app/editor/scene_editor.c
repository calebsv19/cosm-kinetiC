#include "app/editor/scene_editor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input/input.h"
#include "ui/text_input.h"
#include "app/preset_io.h"

#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_widgets.h"

#define DOUBLE_CLICK_MS 350
#define OBJECT_DELETE_MARGIN 0.15f
#define DEFAULT_BOUNDARY_STRENGTH 25.0f

typedef enum EditorSelectionKind {
    SELECTION_NONE = 0,
    SELECTION_EMITTER,
    SELECTION_OBJECT
} EditorSelectionKind;

typedef struct SceneEditorState {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    AppConfig     cfg;
    FluidScenePreset working;
    FluidScenePreset *target;
    InputContextManager *context_mgr;
    bool owns_context_mgr;
    char *name_buffer;        // external storage (optional)
    size_t name_capacity;
    char   local_name[CUSTOM_PRESET_NAME_MAX];
    char  *name_edit_ptr;     // points to name_buffer or local_name
    size_t name_edit_capacity;
    TextInputField name_input;
    bool renaming_name;
    Uint32 last_name_click;
    bool editing_width;
    bool editing_height;
    Uint32 last_width_click;
    Uint32 last_height_click;
    TextInputField width_input;
    TextInputField height_input;

    int canvas_x;
    int canvas_y;
    int canvas_width;
    int canvas_height;
    SDL_Rect panel_rect;
    SDL_Rect width_rect;
    SDL_Rect height_rect;

    int  selected_emitter;
    int  hover_emitter;
    EditorDragMode drag_mode;
    bool dragging;
    float drag_offset_x;
    float drag_offset_y;
    int   selected_object;
    int   hover_object;
    bool  dragging_object;
    float object_drag_offset_x;
    float object_drag_offset_y;
    bool  dragging_object_handle;
    float object_handle_ratio;
    float handle_initial_length;
    bool  handle_resize_started;
    EditorSelectionKind selection_kind;

    EditorButton btn_save;
    EditorButton btn_cancel;
    EditorButton btn_add_source;
    EditorButton btn_add_jet;
    EditorButton btn_add_sink;
    EditorButton btn_add_circle;
    EditorButton btn_add_box;
    EditorButton btn_boundary;

    NumericField radius_field;
    NumericField strength_field;
    NumericField *active_field;

    bool boundary_mode;
    int  boundary_hover_edge;
    int  boundary_selected_edge;

    int pointer_x;
    int pointer_y;

    bool running;
    bool applied;
    bool dirty;
} SceneEditorState;

static SDL_Color COLOR_BG        = {20, 22, 26, 255};
static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_FIELD_ACTIVE = {24, 26, 33, 255};
static SDL_Color COLOR_FIELD_BORDER = {90, 170, 255, 255};

#define EDITOR_PANEL_WIDTH 280
#define EDITOR_CANVAS_MARGIN 40
#define EDITOR_CANVAS_TOP 90

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static void set_dirty(SceneEditorState *state);

static SDL_Rect editor_name_rect(const SceneEditorState *state) {
    SDL_Rect rect = {
        .x = state->canvas_x,
        .y = state->canvas_y - 50,
        .w = state->canvas_width,
        .h = 36
    };
    if (rect.y < 20) rect.y = 20;
    return rect;
}

static void editor_update_dimension_rects(SceneEditorState *state) {
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

static float sanitize_domain_dimension(float value) {
    if (!isfinite(value) || value <= 0.1f) {
        return 0.1f;
    }
    if (value > 64.0f) {
        return 64.0f;
    }
    return value;
}

static void editor_update_canvas_layout(SceneEditorState *state) {
    if (!state || !state->window) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(state->window, &winW, &winH);
    int panel_width = EDITOR_PANEL_WIDTH;
    int canvas_margin = EDITOR_CANVAS_MARGIN;
    int canvas_top = EDITOR_CANVAS_TOP;
    int max_canvas_h = winH - 220;
    if (max_canvas_h < 240) max_canvas_h = 240;
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
    if (canvas_w < 220) canvas_w = 220;
    if (canvas_h < 180) canvas_h = 180;

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

static void editor_begin_name_edit(SceneEditorState *state) {
    if (!state || !state->name_edit_ptr || state->name_edit_capacity == 0) return;
    text_input_begin(&state->name_input,
                     state->name_edit_ptr,
                     state->name_edit_capacity - 1);
    state->renaming_name = true;
}

static void editor_finish_name_edit(SceneEditorState *state, bool apply) {
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

static void editor_finish_dimension_edit(SceneEditorState *state,
                                         bool editing_width,
                                         bool apply);

static void editor_begin_dimension_edit(SceneEditorState *state, bool editing_width) {
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

static void editor_finish_dimension_edit(SceneEditorState *state,
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

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void canvas_to_normalized_unclamped(const SceneEditorState *state,
                                           int sx,
                                           int sy,
                                           float *out_x,
                                           float *out_y) {
    if (!state || !out_x || !out_y ||
        state->canvas_width <= 0 || state->canvas_height <= 0) return;
    *out_x = (float)(sx - state->canvas_x) / (float)state->canvas_width;
    *out_y = (float)(sy - state->canvas_y) / (float)state->canvas_height;
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

static void set_dirty(SceneEditorState *state) {
    if (state) state->dirty = true;
}

static void adjust_emitter_radius(FluidEmitter *em, float scale) {
    float new_radius = em->radius * scale;
    if (new_radius < 0.02f) new_radius = 0.02f;
    if (new_radius > 0.6f) new_radius = 0.6f;
    float ratio = (em->radius > 0.0f) ? (new_radius / em->radius) : 1.0f;
    em->radius = new_radius;
    em->strength *= ratio;
}

static NumericField *current_field(SceneEditorState *state) {
    return state ? state->active_field : NULL;
}

static void clamp_object(PresetObject *obj);

static const char *emitter_type_name(FluidEmitterType type) {
    switch (type) {
    case EMITTER_DENSITY_SOURCE: return "Density";
    case EMITTER_VELOCITY_JET:   return "Jet";
    case EMITTER_SINK:           return "Sink";
    default:                     return "Emitter";
    }
}

static const char *boundary_edge_name(int edge) {
    switch (edge) {
    case BOUNDARY_EDGE_TOP:    return "Top";
    case BOUNDARY_EDGE_RIGHT:  return "Right";
    case BOUNDARY_EDGE_BOTTOM: return "Bottom";
    case BOUNDARY_EDGE_LEFT:   return "Left";
    default:                   return "Edge";
    }
}

static void ensure_boundary_strength(BoundaryFlow *flow) {
    if (!flow) return;
    if (flow->strength <= 0.0f) {
        flow->strength = DEFAULT_BOUNDARY_STRENGTH;
    }
}

static void set_boundary_mode(SceneEditorState *state,
                              int edge,
                              BoundaryFlowMode mode) {
    if (!state || edge < 0 || edge >= BOUNDARY_EDGE_COUNT) return;
    BoundaryFlow *flow = &state->working.boundary_flows[edge];
    flow->mode = mode;
    if (mode != BOUNDARY_FLOW_DISABLED) {
        ensure_boundary_strength(flow);
    }
    state->boundary_selected_edge = edge;
    set_dirty(state);
}

static void cycle_boundary_emitter(SceneEditorState *state, int edge) {
    if (!state) return;
    set_boundary_mode(state, edge, BOUNDARY_FLOW_EMIT);
}

static void set_boundary_receiver(SceneEditorState *state, int edge) {
    if (!state) return;
    set_boundary_mode(state, edge, BOUNDARY_FLOW_RECEIVE);
}

static void clear_boundary(SceneEditorState *state, int edge) {
    if (!state || edge < 0 || edge >= BOUNDARY_EDGE_COUNT) return;
    BoundaryFlow *flow = &state->working.boundary_flows[edge];
    flow->mode = BOUNDARY_FLOW_DISABLED;
    flow->strength = 0.0f;
    state->boundary_selected_edge = edge;
    set_dirty(state);
}

static const char *boundary_mode_label(BoundaryFlowMode mode) {
    switch (mode) {
    case BOUNDARY_FLOW_EMIT:    return "Emit";
    case BOUNDARY_FLOW_RECEIVE: return "Receive";
    default:                    return "Disabled";
    }
}

static void begin_field_edit(SceneEditorState *state, NumericField *field) {
    if (!state || !field) return;
    if (state->active_field && state->active_field != field) {
        state->active_field->editing = false;
    }
    memset(field->buffer, 0, sizeof(field->buffer));
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        const FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        float value = (field->target == FIELD_RADIUS) ? em->radius : em->strength;
        snprintf(field->buffer, sizeof(field->buffer), "%.3f", value);
    }
    field->editing = true;
    state->active_field = field;
}

static void cancel_field_edit(SceneEditorState *state) {
    NumericField *field = current_field(state);
    if (!field) return;
    field->editing = false;
    memset(field->buffer, 0, sizeof(field->buffer));
    state->active_field = NULL;
}

static void commit_field_edit(SceneEditorState *state) {
    NumericField *field = current_field(state);
    if (!state || !field || !field->editing) return;
    if (state->selected_emitter < 0 ||
        state->selected_emitter >= (int)state->working.emitter_count) {
        cancel_field_edit(state);
        return;
    }
    char *endptr = NULL;
    float value = strtof(field->buffer, &endptr);
    if (field->buffer[0] == '\0' || endptr == field->buffer) {
        cancel_field_edit(state);
        return;
    }
    FluidEmitter *em = &state->working.emitters[state->selected_emitter];
    if (field->target == FIELD_RADIUS) {
        if (value < 0.01f) value = 0.01f;
        if (value > 0.6f) value = 0.6f;
        em->radius = value;
    } else if (field->target == FIELD_STRENGTH) {
        if (value < 0.1f) value = 0.1f;
        em->strength = value;
    }
    set_dirty(state);
    field->editing = false;
    state->active_field = NULL;
}

static bool field_handle_key(SceneEditorState *state, SDL_Keycode key) {
    NumericField *field = current_field(state);
    if (!field || !field->editing) return false;
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        commit_field_edit(state);
        return true;
    case SDLK_ESCAPE:
        cancel_field_edit(state);
        return true;
    case SDLK_BACKSPACE: {
        size_t len = strlen(field->buffer);
        if (len > 0) {
            field->buffer[len - 1] = '\0';
        }
        return true;
    }
    default:
        break;
    }
    if ((key >= SDLK_0 && key <= SDLK_9) || key == SDLK_PERIOD || key == SDLK_MINUS) {
        size_t len = strlen(field->buffer);
        if (len + 1 < sizeof(field->buffer)) {
            char ch = (char)key;
            field->buffer[len] = ch;
            field->buffer[len + 1] = '\0';
        }
        return true;
    }
    return false;
}

static void nudge_selected(SceneEditorState *state, float dx, float dy) {
    if (!state) return;
    if (state->selection_kind == SELECTION_OBJECT &&
        state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        obj->position_x = clamp01(obj->position_x + dx);
        obj->position_y = clamp01(obj->position_y + dy);
        clamp_object(obj);
        set_dirty(state);
        return;
    }
    if (state->selected_emitter < 0 ||
        state->selected_emitter >= (int)state->working.emitter_count) {
        return;
    }
    FluidEmitter *em = &state->working.emitters[state->selected_emitter];
    em->position_x = clamp01(em->position_x + dx);
    em->position_y = clamp01(em->position_y + dy);
    set_dirty(state);
}

static void normalize_direction(FluidEmitter *em) {
    float len = sqrtf(em->dir_x * em->dir_x + em->dir_y * em->dir_y);
    if (len < 0.001f) {
        em->dir_x = 0.0f;
        em->dir_y = -1.0f;
    } else {
        em->dir_x /= len;
        em->dir_y /= len;
    }
}

static void add_emitter(SceneEditorState *state, FluidEmitterType type) {
    if (!state) return;
    if (state->working.emitter_count >= MAX_FLUID_EMITTERS) return;
    FluidEmitter emitter = {
        .type = type,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .radius = 0.08f,
        .dir_x = 0.0f,
        .dir_y = -1.0f
    };
    switch (type) {
    case EMITTER_DENSITY_SOURCE:
        emitter.strength = 8.0f;
        break;
    case EMITTER_VELOCITY_JET:
        emitter.strength = 40.0f;
        break;
    case EMITTER_SINK:
        emitter.strength = 25.0f;
        break;
    }
    state->working.emitters[state->working.emitter_count] = emitter;
    state->selected_emitter = (int)state->working.emitter_count;
    state->working.emitter_count++;
    normalize_direction(&state->working.emitters[state->selected_emitter]);
    state->selection_kind = SELECTION_EMITTER;
    state->selected_object = -1;
    set_dirty(state);
}

static void remove_selected(SceneEditorState *state) {
    if (!state) return;
    size_t count = state->working.emitter_count;
    if (count == 0 || state->selected_emitter < 0 ||
        state->selected_emitter >= (int)count) {
        return;
    }
    for (size_t i = (size_t)state->selected_emitter; i + 1 < count; ++i) {
        state->working.emitters[i] = state->working.emitters[i + 1];
    }
    state->working.emitter_count--;
    if (state->working.emitter_count == 0) {
        state->selected_emitter = -1;
    } else if (state->selected_emitter >= (int)state->working.emitter_count) {
        state->selected_emitter = (int)state->working.emitter_count - 1;
    }
    cancel_field_edit(state);
    set_dirty(state);
    if (state->working.emitter_count > 0) {
        state->selection_kind = SELECTION_EMITTER;
    } else if (state->working.object_count > 0) {
        state->selection_kind = SELECTION_OBJECT;
        if (state->selected_object < 0) {
            state->selected_object = 0;
        }
    } else {
        state->selection_kind = SELECTION_NONE;
    }
}

static void clamp_object(PresetObject *obj) {
    if (!obj) return;
    if (obj->position_x < 0.0f) obj->position_x = 0.0f;
    if (obj->position_x > 1.0f) obj->position_x = 1.0f;
    if (obj->position_y < 0.0f) obj->position_y = 0.0f;
    if (obj->position_y > 1.0f) obj->position_y = 1.0f;
    if (obj->size_x < 0.02f) obj->size_x = 0.02f;
    if (obj->size_y < 0.02f) obj->size_y = 0.02f;
}

static bool object_is_outside(float x, float y) {
    return (x < -OBJECT_DELETE_MARGIN) || (x > 1.0f + OBJECT_DELETE_MARGIN) ||
           (y < -OBJECT_DELETE_MARGIN) || (y > 1.0f + OBJECT_DELETE_MARGIN);
}

static void add_object(SceneEditorState *state, PresetObjectType type) {
    if (!state) return;
    if (state->working.object_count >= MAX_PRESET_OBJECTS) return;
    PresetObject obj = {
        .type = type,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .size_x = 0.1f,
        .size_y = (type == PRESET_OBJECT_BOX) ? 0.08f : 0.1f,
        .angle = 0.0f,
        .is_static = true
    };
    clamp_object(&obj);
    state->working.objects[state->working.object_count++] = obj;
    state->selected_object = (int)state->working.object_count - 1;
    state->selected_emitter = -1;
    state->selection_kind = SELECTION_OBJECT;
    set_dirty(state);
}

static void remove_selected_object(SceneEditorState *state) {
    if (!state || state->selected_object < 0) return;
    int index = state->selected_object;
    if (index >= (int)state->working.object_count) return;
    for (int i = index; i < (int)state->working.object_count - 1; ++i) {
        state->working.objects[i] = state->working.objects[i + 1];
    }
    if (state->working.object_count > 0) {
        state->working.object_count--;
    }
    if (state->working.object_count == 0) {
        state->selected_object = -1;
        state->selection_kind = SELECTION_NONE;
        if (state->working.emitter_count > 0) {
            state->selection_kind = SELECTION_EMITTER;
            if (state->selected_emitter < 0) state->selected_emitter = 0;
        }
    } else if (state->selected_object >= (int)state->working.object_count) {
        state->selected_object = (int)state->working.object_count - 1;
    }
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
    set_dirty(state);
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

static void draw_editor(SceneEditorState *state) {
    SDL_Renderer *renderer = state->renderer;
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer);

    scene_editor_canvas_draw_background(renderer,
                                        state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height);

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
                                      state->font_small);

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
    scene_editor_draw_button(renderer, &state->btn_add_circle, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_add_box, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_boundary, state->font_small);

    scene_editor_draw_button(renderer, &state->btn_save, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_cancel, state->font_small);

}

static void finish_and_apply(SceneEditorState *state) {
    if (!state) return;
    commit_field_edit(state);
    if (state->editing_width) {
        editor_finish_dimension_edit(state, true, true);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, false, true);
    }
    state->applied = true;
    state->running = false;
}

static void cancel_and_close(SceneEditorState *state) {
    if (!state) return;
    if (state->editing_width) {
        editor_finish_dimension_edit(state, true, false);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, false, false);
    }
    state->applied = false;
    state->running = false;
}

static void editor_pointer_down(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    int x = ptr->x;
    int y = ptr->y;

    state->pointer_x = x;
    state->pointer_y = y;

    if (state->name_edit_ptr) {
        SDL_Rect name_rect = editor_name_rect(state);
        if (point_in_rect(&name_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click =
                (now - state->last_name_click) <= DOUBLE_CLICK_MS;
            state->last_name_click = now;
            if (double_click) {
                editor_begin_name_edit(state);
                return;
            }
        } else if (state->renaming_name) {
            editor_finish_name_edit(state, false);
        }
    }

    if (state->active_field &&
        !point_in_rect(&state->active_field->rect, x, y)) {
        commit_field_edit(state);
    }

    if (state->width_rect.w > 0 && state->width_rect.h > 0) {
        if (point_in_rect(&state->width_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_width_click) <= DOUBLE_CLICK_MS;
            state->last_width_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, true);
                return;
            }
        } else if (state->editing_width) {
            editor_finish_dimension_edit(state, true, false);
        }
    }

    if (state->height_rect.w > 0 && state->height_rect.h > 0) {
        if (point_in_rect(&state->height_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_height_click) <= DOUBLE_CLICK_MS;
            state->last_height_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, false);
                return;
            }
        } else if (state->editing_height) {
            editor_finish_dimension_edit(state, false, false);
        }
    }

    EditorButton *buttons[] = {
        &state->btn_save,
        &state->btn_cancel,
        &state->btn_add_source,
        &state->btn_add_jet,
        &state->btn_add_sink,
        &state->btn_add_circle,
        &state->btn_add_box,
        &state->btn_boundary
    };
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        EditorButton *btn = buttons[i];
        if (!btn->enabled) continue;
        if (point_in_rect(&btn->rect, x, y)) {
            if (btn == &state->btn_save) {
                finish_and_apply(state);
            } else if (btn == &state->btn_cancel) {
                cancel_and_close(state);
            } else if (btn == &state->btn_add_source) {
                add_emitter(state, EMITTER_DENSITY_SOURCE);
            } else if (btn == &state->btn_add_jet) {
                add_emitter(state, EMITTER_VELOCITY_JET);
            } else if (btn == &state->btn_add_sink) {
                add_emitter(state, EMITTER_SINK);
            } else if (btn == &state->btn_add_circle) {
                add_object(state, PRESET_OBJECT_CIRCLE);
            } else if (btn == &state->btn_add_box) {
                add_object(state, PRESET_OBJECT_BOX);
            } else if (btn == &state->btn_boundary) {
                state->boundary_mode = !state->boundary_mode;
                state->boundary_selected_edge = -1;
                state->boundary_hover_edge = -1;
            }
            return;
        }
    }

    NumericField *fields[] = {&state->radius_field, &state->strength_field};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        if (point_in_rect(&fields[i]->rect, x, y)) {
            begin_field_edit(state, fields[i]);
            return;
        }
    }

    int edge_hit = scene_editor_canvas_hit_edge(state->canvas_x,
                                                state->canvas_y,
                                                state->canvas_width,
                                                state->canvas_height,
                                                x,
                                                y);
    if (edge_hit >= 0) {
        state->boundary_selected_edge = edge_hit;
        if (state->boundary_mode) {
            if (state->working.boundary_flows[edge_hit].mode == BOUNDARY_FLOW_DISABLED) {
                cycle_boundary_emitter(state, edge_hit);
            }
        }
        return;
    }

    if (!point_in_rect(&state->panel_rect, x, y)) {
        int handle_hit = scene_editor_canvas_hit_object_handle(&state->working,
                                                               state->canvas_x,
                                                               state->canvas_y,
                                                               state->canvas_width,
                                                               state->canvas_height,
                                                               x,
                                                               y);
        if (handle_hit >= 0) {
            state->selected_object = handle_hit;
            state->selected_emitter = -1;
            state->selection_kind = SELECTION_OBJECT;
            state->dragging_object_handle = true;
            PresetObject *obj = &state->working.objects[handle_hit];
            if (obj->type == PRESET_OBJECT_CIRCLE) {
                state->object_handle_ratio = 1.0f;
            } else {
                state->object_handle_ratio = (obj->size_x > 0.0001f)
                                                 ? (obj->size_y / obj->size_x)
                                                 : 1.0f;
                if (state->object_handle_ratio <= 0.0f) state->object_handle_ratio = 1.0f;
            }
            float nx = 0.0f, ny = 0.0f;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              x,
                                              y,
                                              &nx,
                                              &ny);
            float dx = nx - obj->position_x;
            float dy = ny - obj->position_y;
            float len = sqrtf(dx * dx + dy * dy);
            float adjusted = len - SCENE_EDITOR_OBJECT_HANDLE_MARGIN;
            if (adjusted < 0.0f) adjusted = 0.0f;
            state->handle_initial_length = adjusted;
            state->handle_resize_started = false;
            return;
        }

        EditorDragMode mode = DRAG_NONE;
        int obj_hit = scene_editor_canvas_hit_object(&state->working,
                                                     state->canvas_x,
                                                     state->canvas_y,
                                                     state->canvas_width,
                                                     state->canvas_height,
                                                     x,
                                                     y);
        if (obj_hit >= 0) {
            state->selected_object = obj_hit;
            state->selected_emitter = -1;
            state->selection_kind = SELECTION_OBJECT;
            state->dragging_object = true;
            state->dragging_object_handle = false;
            state->handle_resize_started = false;
            PresetObject *obj = &state->working.objects[obj_hit];
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              x,
                                              y,
                                              &nx,
                                              &ny);
            state->object_drag_offset_x = nx - obj->position_x;
            state->object_drag_offset_y = ny - obj->position_y;
            return;
        }

        int hit = scene_editor_canvas_hit_test(&state->working,
                                               state->canvas_x,
                                               state->canvas_y,
                                               state->canvas_width,
                                               state->canvas_height,
                                               x,
                                               y,
                                               &mode);
        state->hover_emitter = hit;
        if (hit < 0) {
            state->dragging = false;
            state->drag_mode = DRAG_NONE;
            state->selection_kind = SELECTION_NONE;
            return;
        }
        state->selection_kind = SELECTION_EMITTER;
        state->selected_emitter = hit;
        state->selected_object = -1;
        state->drag_mode = mode;
        state->dragging = true;
        state->dragging_object_handle = false;
        state->handle_resize_started = false;
        state->drag_offset_x = 0.0f;
        state->drag_offset_y = 0.0f;
        if (mode == DRAG_POSITION) {
            FluidEmitter *em = &state->working.emitters[hit];
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              x,
                                              y,
                                              &nx,
                                              &ny);
            state->drag_offset_x = nx - em->position_x;
            state->drag_offset_y = ny - em->position_y;
        }
    }
}

static void editor_pointer_up(void *user, const InputPointerState *ptr) {
    (void)ptr;
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
}

static void editor_pointer_move(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    state->pointer_x = ptr->x;
    state->pointer_y = ptr->y;
    if (state->dragging_object_handle &&
        state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        float norm_x = 0.0f, norm_y = 0.0f;
        canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &norm_x, &norm_y);
        float dx = norm_x - obj->position_x;
        float dy = norm_y - obj->position_y;
        float len = sqrtf(dx * dx + dy * dy);
        float adjusted = len - SCENE_EDITOR_OBJECT_HANDLE_MARGIN;
        if (adjusted < SCENE_EDITOR_OBJECT_HANDLE_MIN) {
            adjusted = SCENE_EDITOR_OBJECT_HANDLE_MIN;
        }
        if (!state->handle_resize_started) {
            if (fabsf(adjusted - state->handle_initial_length) <= 0.002f) {
                return;
            }
            state->handle_resize_started = true;
        }
        obj->angle = atan2f(dy, dx);
        if (obj->type == PRESET_OBJECT_BOX) {
            float ratio = state->object_handle_ratio;
            if (ratio <= 0.01f) ratio = 1.0f;
            obj->size_x = adjusted;
            float scaled_y = adjusted * ratio;
            if (scaled_y < SCENE_EDITOR_OBJECT_HANDLE_MIN) scaled_y = SCENE_EDITOR_OBJECT_HANDLE_MIN;
            obj->size_y = scaled_y;
        } else {
            obj->size_x = adjusted;
            obj->size_y = adjusted;
        }
        clamp_object(obj);
        set_dirty(state);
        return;
    }
    if (state->dragging_object && state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        float nx = 0.0f, ny = 0.0f;
        canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &nx, &ny);
        nx -= state->object_drag_offset_x;
        ny -= state->object_drag_offset_y;
        if (object_is_outside(nx, ny)) {
            remove_selected_object(state);
            return;
        }
        obj->position_x = clamp01(nx);
        obj->position_y = clamp01(ny);
        clamp_object(obj);
        set_dirty(state);
        return;
    }
    if (state->dragging && state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        if (state->drag_mode == DRAG_POSITION) {
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              ptr->x,
                                              ptr->y,
                                              &nx,
                                              &ny);
            nx -= state->drag_offset_x;
            ny -= state->drag_offset_y;
            em->position_x = clamp01(nx);
            em->position_y = clamp01(ny);
            set_dirty(state);
        } else if (state->drag_mode == DRAG_DIRECTION) {
            int cx, cy;
            scene_editor_canvas_project(state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        em->position_x,
                                        em->position_y,
                                        &cx,
                                        &cy);
            float dx = (float)(ptr->x - cx);
            float dy = (float)(ptr->y - cy);
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0.0001f) {
                em->dir_x = dx / len;
                em->dir_y = dy / len;
                set_dirty(state);
            }
        }
    } else {
        EditorDragMode mode = DRAG_NONE;
            state->hover_emitter = scene_editor_canvas_hit_test(&state->working,
                                                                state->canvas_x,
                                                                state->canvas_y,
                                                                state->canvas_width,
                                                                state->canvas_height,
                                                                ptr->x,
                                                                ptr->y,
                                                                &mode);
    }
    state->hover_object = scene_editor_canvas_hit_object(&state->working,
                                                         state->canvas_x,
                                                         state->canvas_y,
                                                         state->canvas_width,
                                                         state->canvas_height,
                                                         ptr->x,
                                                         ptr->y);
    if (state->hover_object < 0) {
        int handle_hover = scene_editor_canvas_hit_object_handle(&state->working,
                                                                 state->canvas_x,
                                                                 state->canvas_y,
                                                                 state->canvas_width,
                                                                 state->canvas_height,
                                                                 ptr->x,
                                                                 ptr->y);
        if (handle_hover >= 0) {
            state->hover_object = handle_hover;
        }
    }

    state->boundary_hover_edge = scene_editor_canvas_hit_edge(state->canvas_x,
                                                              state->canvas_y,
                                                              state->canvas_width,
                                                              state->canvas_height,
                                                              ptr->x,
                                                              ptr->y);
}

static void editor_text_input(void *user, const char *text) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !text) return;
    if (state->renaming_name) {
        text_input_handle_text(&state->name_input, text);
        return;
    }
    if (state->editing_width) {
        text_input_handle_text(&state->width_input, text);
        return;
    }
    if (state->editing_height) {
        text_input_handle_text(&state->height_input, text);
        return;
    }
}

static void editor_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
    if (state->renaming_name) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_name_edit(state, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_name_edit(state, false);
        } else {
            text_input_handle_key(&state->name_input, key);
        }
        return;
    }
    if (state->editing_width) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, true, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, true, false);
        } else {
            text_input_handle_key(&state->width_input, key);
        }
        return;
    }
    if (state->editing_height) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, false, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, false, false);
        } else {
            text_input_handle_key(&state->height_input, key);
        }
        return;
    }
    if (field_handle_key(state, key)) {
        return;
    }

    if (state->boundary_selected_edge >= 0) {
        switch (key) {
        case SDLK_e:
            cycle_boundary_emitter(state, state->boundary_selected_edge);
            return;
        case SDLK_r:
            set_boundary_receiver(state, state->boundary_selected_edge);
            return;
        case SDLK_x:
            clear_boundary(state, state->boundary_selected_edge);
            return;
        default:
            break;
        }
    }
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        finish_and_apply(state);
        break;
    case SDLK_ESCAPE:
        cancel_and_close(state);
        break;
    case SDLK_TAB: {
        if (state->working.emitter_count == 0) break;
        if (state->selected_emitter < 0) state->selected_emitter = 0;
        int dir = (mod & KMOD_SHIFT) ? -1 : 1;
        int next = state->selected_emitter + dir;
        if (next < 0) next = (int)state->working.emitter_count - 1;
        if (next >= (int)state->working.emitter_count) next = 0;
        state->selected_emitter = next;
        state->selection_kind = SELECTION_EMITTER;
        state->selected_object = -1;
        break;
    }
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        if (state->selection_kind == SELECTION_OBJECT) {
            remove_selected_object(state);
        } else {
            remove_selected(state);
        }
        break;
    case SDLK_PLUS:
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            PresetObject *obj = &state->working.objects[state->selected_object];
            obj->size_x *= 1.1f;
            obj->size_y *= 1.1f;
            clamp_object(obj);
            set_dirty(state);
        } else if (state->selected_emitter >= 0 &&
                   state->selected_emitter < (int)state->working.emitter_count) {
            adjust_emitter_radius(&state->working.emitters[state->selected_emitter], 1.1f);
            set_dirty(state);
        }
        break;
    case SDLK_MINUS:
    case SDLK_UNDERSCORE:
    case SDLK_KP_MINUS:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            PresetObject *obj = &state->working.objects[state->selected_object];
            obj->size_x *= 0.9f;
            obj->size_y *= 0.9f;
            clamp_object(obj);
            set_dirty(state);
        } else if (state->selected_emitter >= 0 &&
                   state->selected_emitter < (int)state->working.emitter_count) {
            adjust_emitter_radius(&state->working.emitters[state->selected_emitter], 0.9f);
            set_dirty(state);
        }
        break;
    case SDLK_UP:
        nudge_selected(state, 0.0f, -0.01f);
        break;
    case SDLK_DOWN:
        nudge_selected(state, 0.0f, 0.01f);
        break;
    case SDLK_LEFT:
        nudge_selected(state, -0.01f, 0.0f);
        break;
    case SDLK_RIGHT:
        nudge_selected(state, 0.01f, 0.0f);
        break;
    default:
        break;
    }
}

static void editor_key_up(void *user, SDL_Keycode key, SDL_Keymod mod) {
    (void)user;
    (void)key;
    (void)mod;
}

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      const AppConfig *cfg,
                      FluidScenePreset *preset,
                      InputContextManager *ctx_mgr,
                      char *name_buffer,
                      size_t name_capacity) {
    if (!window || !renderer || !preset) return false;

    SceneEditorState state = {
        .window = window,
        .renderer = renderer,
        .font_main = font_main,
        .font_small = font_small,
        .cfg = *cfg,
        .working = *preset,
        .target = preset,
        .context_mgr = ctx_mgr,
        .owns_context_mgr = false,
        .name_buffer = name_buffer,
        .name_capacity = name_capacity,
        .renaming_name = false,
        .last_name_click = 0,
        .selected_emitter = (preset->emitter_count > 0) ? 0 : -1,
        .hover_emitter = -1,
        .drag_mode = DRAG_NONE,
        .dragging = false,
        .drag_offset_x = 0.0f,
        .drag_offset_y = 0.0f,
        .selected_object = (preset->emitter_count == 0 && preset->object_count > 0) ? 0 : -1,
        .hover_object = -1,
        .dragging_object = false,
        .object_drag_offset_x = 0.0f,
        .object_drag_offset_y = 0.0f,
        .dragging_object_handle = false,
        .object_handle_ratio = 1.0f,
        .handle_initial_length = 0.0f,
        .handle_resize_started = false,
        .selection_kind = (preset->emitter_count > 0)
                              ? SELECTION_EMITTER
                              : ((preset->object_count > 0) ? SELECTION_OBJECT : SELECTION_NONE),
        .active_field = NULL,
        .boundary_mode = false,
        .boundary_hover_edge = -1,
        .boundary_selected_edge = -1,
        .pointer_x = -1,
        .pointer_y = -1,
        .running = true,
        .applied = false,
        .dirty = false
    };
    if (state.working.domain_width <= 0.0f) state.working.domain_width = 1.0f;
    if (state.working.domain_height <= 0.0f) state.working.domain_height = 1.0f;

    if (state.name_buffer) {
        state.name_edit_ptr = state.name_buffer;
        state.name_edit_capacity = state.name_capacity;
    } else {
        state.name_edit_ptr = state.local_name;
        state.name_edit_capacity = sizeof(state.local_name);
    }
    if (preset->name) {
        snprintf(state.name_edit_ptr, state.name_edit_capacity, "%s", preset->name);
    } else {
        state.name_edit_ptr[0] = '\0';
    }
    state.working.name = state.name_edit_ptr;

    InputContextManager local_mgr;
    if (!state.context_mgr) {
        input_context_manager_init(&local_mgr);
        state.context_mgr = &local_mgr;
        state.owns_context_mgr = true;
    }

    editor_update_canvas_layout(&state);

    int panel_width = state.panel_rect.w;
    int field_w = panel_width - 32;
    state.radius_field = (NumericField){
        .rect = {state.panel_rect.x + 16, state.panel_rect.y + 70, field_w, 40},
        .label = "Radius",
        .target = FIELD_RADIUS,
        .editing = false
    };
    state.strength_field = (NumericField){
        .rect = {state.panel_rect.x + 16, state.panel_rect.y + 140, field_w, 40},
        .label = "Strength",
        .target = FIELD_STRENGTH,
        .editing = false
    };

    int button_w = (field_w - 12) / 2;
    state.btn_add_source = (EditorButton){
        .rect = {state.panel_rect.x + 16, state.panel_rect.y + 210, button_w, 36},
        .label = "Add Source",
        .enabled = true
    };
    state.btn_add_jet = (EditorButton){
        .rect = {state.panel_rect.x + 28 + button_w, state.panel_rect.y + 210, button_w, 36},
        .label = "Add Jet",
        .enabled = true
    };
    state.btn_add_sink = (EditorButton){
        .rect = {state.panel_rect.x + 16, state.panel_rect.y + 256, button_w, 36},
        .label = "Add Sink",
        .enabled = true
    };
    state.btn_add_circle = (EditorButton){
        .rect = {state.panel_rect.x + 16, state.panel_rect.y + 308, button_w, 36},
        .label = "Add Circle",
        .enabled = true
    };
    state.btn_add_box = (EditorButton){
        .rect = {state.panel_rect.x + 28 + button_w, state.panel_rect.y + 308, button_w, 36},
        .label = "Add Box",
        .enabled = true
    };

    state.btn_boundary = (EditorButton){
        .rect = {state.panel_rect.x + 28 + button_w, state.panel_rect.y + 256, button_w, 36},
        .label = "Air Flow Mode",
        .enabled = true
    };

    state.btn_save = (EditorButton){
        .rect = {state.panel_rect.x + 16,
                 state.panel_rect.y + state.panel_rect.h - 90,
                 field_w,
                 40},
        .label = "Save Changes",
        .enabled = true
    };
    state.btn_cancel = (EditorButton){
        .rect = {state.panel_rect.x + 16,
                 state.panel_rect.y + state.panel_rect.h - 44,
                 field_w,
                 36},
        .label = "Cancel",
        .enabled = true
    };

    InputContext editor_ctx = {
        .on_pointer_down = editor_pointer_down,
        .on_pointer_up = editor_pointer_up,
        .on_pointer_move = editor_pointer_move,
        .on_key_down = editor_key_down,
        .on_key_up = editor_key_up,
        .on_text_input = editor_text_input,
        .user_data = &state
    };
    input_context_manager_push(state.context_mgr, &editor_ctx);

    Uint32 prev_ticks = SDL_GetTicks();
    while (state.running) {
        Uint32 now = SDL_GetTicks();
        double dt = (double)(now - prev_ticks) / 1000.0;
        prev_ticks = now;
        text_input_update(&state.name_input, dt);
        text_input_update(&state.width_input, dt);
        text_input_update(&state.height_input, dt);

        InputCommands cmds;
        if (!input_poll_events(&cmds, NULL, state.context_mgr)) {
            state.running = false;
            break;
        }
        if (cmds.quit) {
            state.running = false;
            break;
        }

        state.btn_add_source.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_jet.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_sink.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_circle.enabled = state.working.object_count < MAX_PRESET_OBJECTS;
        state.btn_add_box.enabled = state.working.object_count < MAX_PRESET_OBJECTS;

        draw_editor(&state);
        SDL_RenderPresent(renderer);
    }

    input_context_manager_pop(state.context_mgr);

    if (state.owns_context_mgr) {
        // nothing extra to clean up beyond stack pop
    }

    if (state.renaming_name) {
        editor_finish_name_edit(&state, false);
    }

    if (state.applied && state.target) {
        state.working.is_custom = true;
        *state.target = state.working;
    }

    return state.applied;
}
