#include "app/editor/scene_editor_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/shape_lookup.h"
#include "geo/shape_asset.h"

static NumericField *current_field(SceneEditorState *state) {
    return state ? state->active_field : NULL;
}

static void normalize_direction(FluidEmitter *em);

void scene_editor_sync_selection_session(SceneEditorState *state) {
    if (!state) return;
    physics_sim_editor_session_set_legacy_selection(&state->session,
                                                    state->selection_kind,
                                                    state->selected_emitter,
                                                    state->selected_object,
                                                    state->selected_row);
}

void scene_editor_set_selection(SceneEditorState *state,
                                EditorSelectionKind kind,
                                int emitter_index,
                                int object_index,
                                int import_index) {
    if (!state) return;
    state->selection_kind = kind;
    state->selected_emitter = emitter_index;
    state->selected_object = object_index;
    state->selected_row = import_index;
    scene_editor_sync_selection_session(state);
}

void scene_editor_select_none(SceneEditorState *state) {
    scene_editor_set_selection(state, SELECTION_NONE, -1, -1, -1);
}

void scene_editor_select_emitter(SceneEditorState *state,
                                 int emitter_index,
                                 int object_index,
                                 int import_index) {
    scene_editor_set_selection(state,
                               SELECTION_EMITTER,
                               emitter_index,
                               object_index,
                               import_index);
}

void scene_editor_select_object(SceneEditorState *state, int object_index) {
    scene_editor_set_selection(state, SELECTION_OBJECT, -1, object_index, -1);
}

void scene_editor_select_import(SceneEditorState *state, int import_index) {
    scene_editor_set_selection(state, SELECTION_IMPORT, -1, -1, import_index);
}

void adjust_emitter_radius(FluidEmitter *em, float scale) {
    if (!em) return;
    float new_radius = em->radius * scale;
    if (new_radius < 0.02f) new_radius = 0.02f;
    if (new_radius > 0.6f) new_radius = 0.6f;
    float ratio = (em->radius > 0.0f) ? (new_radius / em->radius) : 1.0f;
    em->radius = new_radius;
    em->strength *= ratio;
}

void clamp_object(PresetObject *obj) {
    if (!obj) return;
    if (obj->position_x < 0.0f) obj->position_x = 0.0f;
    if (obj->position_x > 1.0f) obj->position_x = 1.0f;
    if (obj->position_y < 0.0f) obj->position_y = 0.0f;
    if (obj->position_y > 1.0f) obj->position_y = 1.0f;
    if (!isfinite(obj->position_z)) obj->position_z = 0.0f;
    if (obj->position_z < -8.0f) obj->position_z = -8.0f;
    if (obj->position_z > 8.0f) obj->position_z = 8.0f;
    if (obj->size_x < 0.005f) obj->size_x = 0.005f;
    if (obj->size_y < 0.005f) obj->size_y = 0.005f;
    if (!isfinite(obj->size_z) || obj->size_z < 0.005f) obj->size_z = obj->size_x;
}

bool object_is_outside(float x, float y) {
    return (x < -OBJECT_DELETE_MARGIN) || (x > 1.0f + OBJECT_DELETE_MARGIN) ||
           (y < -OBJECT_DELETE_MARGIN) || (y > 1.0f + OBJECT_DELETE_MARGIN);
}

const char *emitter_type_name(FluidEmitterType type) {
    switch (type) {
    case EMITTER_DENSITY_SOURCE: return "Density";
    case EMITTER_VELOCITY_JET:   return "Jet";
    case EMITTER_SINK:           return "Sink";
    default:                     return "Emitter";
    }
}

const char *boundary_edge_name(int edge) {
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

void cycle_boundary_emitter(SceneEditorState *state, int edge) {
    if (!state) return;
    set_boundary_mode(state, edge, BOUNDARY_FLOW_EMIT);
}

void set_boundary_receiver(SceneEditorState *state, int edge) {
    if (!state) return;
    set_boundary_mode(state, edge, BOUNDARY_FLOW_RECEIVE);
}

void clear_boundary(SceneEditorState *state, int edge) {
    if (!state || edge < 0 || edge >= BOUNDARY_EDGE_COUNT) return;
    BoundaryFlow *flow = &state->working.boundary_flows[edge];
    flow->mode = BOUNDARY_FLOW_DISABLED;
    flow->strength = 0.0f;
    state->boundary_selected_edge = edge;
    set_dirty(state);
}

static float import_max_extent_norm(const SceneEditorState *state, int import_index) {
    if (!state || !state->shape_library) return 0.08f;
    if (import_index < 0 || import_index >= (int)state->working.import_shape_count) return 0.08f;
    const ImportedShape *imp = &state->working.import_shapes[import_index];
    const ShapeAsset *asset = shape_lookup_from_path(state->shape_library, imp->path);
    if (!asset) return 0.08f;
    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return 0.08f;
    float size_x = b.max_x - b.min_x;
    float size_y = b.max_y - b.min_y;
    float max_dim = fmaxf(size_x, size_y);
    if (max_dim <= 0.0001f) return 0.08f;
    const float desired_fit = 0.25f;
    float norm = (imp->scale * desired_fit) / max_dim;
    float max_extent = fmaxf(size_x, size_y) * norm;
    if (max_extent < 0.02f) max_extent = 0.02f;
    if (max_extent > 0.6f) max_extent = 0.6f;
    return max_extent;
}

void sync_emitter_to_import(SceneEditorState *state, int import_index) {
    if (!state) return;
    int em_idx = emitter_index_for_import(state, import_index);
    if (em_idx < 0 || em_idx >= (int)state->working.emitter_count) return;
    FluidEmitter *em = &state->working.emitters[em_idx];
    const ImportedShape *imp = &state->working.import_shapes[import_index];
    em->position_x = imp->position_x;
    em->position_y = imp->position_y;
    em->position_z = imp->position_z;
    em->attached_object = -1;
    em->attached_import = import_index;
    state->emitter_object_map[em_idx] = -1;
    state->emitter_import_map[em_idx] = import_index;
    normalize_direction(em);
    em->position_x = clamp01(em->position_x);
    em->position_y = clamp01(em->position_y);
}

const char *boundary_mode_label(BoundaryFlowMode mode) {
    switch (mode) {
    case BOUNDARY_FLOW_EMIT:    return "Emit";
    case BOUNDARY_FLOW_RECEIVE: return "Receive";
    default:                    return "Disabled";
    }
}

void begin_field_edit(SceneEditorState *state, NumericField *field) {
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

void cancel_field_edit(SceneEditorState *state) {
    NumericField *field = current_field(state);
    if (!field) return;
    field->editing = false;
    memset(field->buffer, 0, sizeof(field->buffer));
    state->active_field = NULL;
}

void commit_field_edit(SceneEditorState *state) {
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

bool field_handle_key(SceneEditorState *state, SDL_Keycode key) {
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

void nudge_selected(SceneEditorState *state, float dx, float dy) {
    if (!state) return;
    if (state->selection_kind == SELECTION_EMITTER &&
        state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        int attached = em->attached_object;
        if (state->emitter_object_map[state->selected_emitter] >= 0) {
            attached = state->emitter_object_map[state->selected_emitter];
        }
        int attached_imp = em->attached_import;
        if (state->emitter_import_map[state->selected_emitter] >= 0) {
            attached_imp = state->emitter_import_map[state->selected_emitter];
        }
        if (attached >= 0 &&
            attached < (int)state->working.object_count) {
            PresetObject *obj = &state->working.objects[attached];
            obj->position_x = clamp01(obj->position_x + dx);
            obj->position_y = clamp01(obj->position_y + dy);
            clamp_object(obj);
            sync_emitter_to_object(state, attached);
            set_dirty(state);
            return;
        } else if (attached_imp >= 0 &&
                   attached_imp < (int)state->working.import_shape_count) {
            ImportedShape *imp = &state->working.import_shapes[attached_imp];
            imp->position_x = clamp01(imp->position_x + dx);
            imp->position_y = clamp01(imp->position_y + dy);
            em->position_x = imp->position_x;
            em->position_y = imp->position_y;
            set_dirty(state);
            return;
        }
    }
    if (state->selection_kind == SELECTION_OBJECT &&
        state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        obj->position_x = clamp01(obj->position_x + dx);
        obj->position_y = clamp01(obj->position_y + dy);
        clamp_object(obj);
        sync_emitter_to_object(state, state->selected_object);
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
        em->dir_z = 0.0f;
    } else {
        em->dir_x /= len;
        em->dir_y /= len;
        if (!isfinite(em->dir_z)) em->dir_z = 0.0f;
        if (em->dir_z < -1.0f) em->dir_z = -1.0f;
        if (em->dir_z > 1.0f) em->dir_z = 1.0f;
    }
}

int emitter_index_for_object(const SceneEditorState *state, int obj_index) {
    if (!state || obj_index < 0 || obj_index >= (int)state->working.object_count) return -1;
    for (size_t i = 0; i < state->working.emitter_count; ++i) {
        int attached = state->emitter_object_map[i];
        if (attached < 0) {
            attached = state->working.emitters[i].attached_object;
        }
        if (attached == obj_index) {
            return (int)i;
        }
    }
    return -1;
}

int emitter_index_for_import(const SceneEditorState *state, int import_index) {
    if (!state || import_index < 0 || import_index >= (int)state->working.import_shape_count) return -1;
    for (size_t i = 0; i < state->working.emitter_count; ++i) {
        int attached = state->emitter_import_map[i];
        if (attached < 0) {
            attached = state->working.emitters[i].attached_import;
        }
        if (attached == import_index) {
            return (int)i;
        }
    }
    return -1;
}

static void apply_defaults_for_type(FluidEmitter *em, FluidEmitterType type) {
    if (!em) return;
    em->type = type;
    switch (type) {
    case EMITTER_DENSITY_SOURCE:
        em->radius = 0.08f;
        em->strength = 8.0f;
        em->dir_x = 0.0f; em->dir_y = -1.0f; em->dir_z = 0.0f;
        break;
    case EMITTER_VELOCITY_JET:
        em->radius = 0.08f;
        em->strength = 40.0f;
        em->dir_x = 0.0f; em->dir_y = -1.0f; em->dir_z = 0.0f;
        break;
    case EMITTER_SINK:
        em->radius = 0.08f;
        em->strength = 25.0f;
        em->dir_x = 0.0f; em->dir_y = -1.0f; em->dir_z = 0.0f;
        break;
    default:
        break;
    }
    normalize_direction(em);
    if (em->attached_object < 0) em->attached_object = -1;
    if (em->attached_import < 0) em->attached_import = -1;
}

void sync_emitter_to_object(SceneEditorState *state, int obj_index) {
    if (!state) return;
    int em_idx = emitter_index_for_object(state, obj_index);
    if (em_idx < 0 || em_idx >= (int)state->working.emitter_count) return;
    FluidEmitter *em = &state->working.emitters[em_idx];
    const PresetObject *obj = &state->working.objects[obj_index];
    em->position_x = obj->position_x;
    em->position_y = obj->position_y;
    em->position_z = obj->position_z;
    em->attached_object = obj_index;
    em->attached_import = -1;
    state->emitter_object_map[em_idx] = obj_index;
    state->emitter_import_map[em_idx] = -1;
    // Keep direction normalized in case callers mutated it.
    normalize_direction(em);
    // Also keep the emitter's position in range.
    em->position_x = clamp01(em->position_x);
    em->position_y = clamp01(em->position_y);
}

void remove_emitter_at(SceneEditorState *state, int em_idx) {
    if (!state || em_idx < 0 || em_idx >= (int)state->working.emitter_count) return;
    size_t count = state->working.emitter_count;
    for (size_t i = (size_t)em_idx; i + 1 < count; ++i) {
        state->working.emitters[i] = state->working.emitters[i + 1];
        state->emitter_object_map[i] = state->emitter_object_map[i + 1];
        state->emitter_import_map[i] = state->emitter_import_map[i + 1];
    }
    state->working.emitter_count--;
    state->emitter_object_map[state->working.emitter_count] = -1;
    state->emitter_import_map[state->working.emitter_count] = -1;
    if (state->selected_emitter >= em_idx) {
        state->selected_emitter--;
        if (state->selected_emitter < 0) state->selected_emitter = -1;
    }
    scene_editor_sync_selection_session(state);
}

int ensure_emitter_for_object(SceneEditorState *state,
                              int obj_index,
                              FluidEmitterType type,
                              bool toggle_clear) {
    if (!state || obj_index < 0 || obj_index >= (int)state->working.object_count) return -1;
    int existing = emitter_index_for_object(state, obj_index);
    if (existing >= 0) {
        FluidEmitter *em = &state->working.emitters[existing];
        if (toggle_clear && em->type == type) {
            remove_emitter_at(state, existing);
            set_dirty(state);
            return -1;
        }
        apply_defaults_for_type(em, type);
        state->emitter_object_map[existing] = obj_index;
        state->emitter_import_map[existing] = -1;
        em->attached_object = obj_index;
        em->attached_import = -1;
        sync_emitter_to_object(state, obj_index);
        set_dirty(state);
        return existing;
    }
    if (state->working.emitter_count >= MAX_FLUID_EMITTERS) return -1;
    const PresetObject *obj = &state->working.objects[obj_index];
    FluidEmitter emitter = {
        .type = type,
        .position_x = obj->position_x,
        .position_y = obj->position_y,
        .position_z = obj->position_z,
        .radius = fmaxf(obj->size_x, obj->size_y),
        .dir_x = 0.0f,
        .dir_y = -1.0f,
        .dir_z = 0.0f,
        .attached_object = obj_index,
        .attached_import = -1
    };
    apply_defaults_for_type(&emitter, type);
    state->working.emitters[state->working.emitter_count] = emitter;
    state->emitter_object_map[state->working.emitter_count] = obj_index;
    state->emitter_import_map[state->working.emitter_count] = -1;
    state->working.emitter_count++;
    sync_emitter_to_object(state, obj_index);
    set_dirty(state);
    return (int)state->working.emitter_count - 1;
}

int ensure_emitter_for_import(SceneEditorState *state,
                              int import_index,
                              FluidEmitterType type,
                              bool toggle_clear) {
    if (!state || import_index < 0 || import_index >= (int)state->working.import_shape_count) return -1;
    int existing = emitter_index_for_import(state, import_index);
    if (existing >= 0) {
        FluidEmitter *em = &state->working.emitters[existing];
        if (toggle_clear && em->type == type) {
            remove_emitter_at(state, existing);
            set_dirty(state);
            return -1;
        }
        apply_defaults_for_type(em, type);
        state->emitter_import_map[existing] = import_index;
        state->emitter_object_map[existing] = -1;
        em->attached_import = import_index;
        em->attached_object = -1;
        sync_emitter_to_import(state, import_index);
        set_dirty(state);
        return existing;
    }
    if (state->working.emitter_count >= MAX_FLUID_EMITTERS) return -1;
    const ImportedShape *imp = &state->working.import_shapes[import_index];
    FluidEmitter emitter = {
        .type = type,
        .position_x = imp->position_x,
        .position_y = imp->position_y,
        .position_z = imp->position_z,
        .radius = import_max_extent_norm(state, import_index),
        .dir_x = 0.0f,
        .dir_y = -1.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = import_index
    };
    apply_defaults_for_type(&emitter, type);
    state->working.emitters[state->working.emitter_count] = emitter;
    state->emitter_import_map[state->working.emitter_count] = import_index;
    state->emitter_object_map[state->working.emitter_count] = -1;
    state->working.emitter_count++;
    sync_emitter_to_import(state, import_index);
    set_dirty(state);
    return (int)state->working.emitter_count - 1;
}

void add_emitter(SceneEditorState *state, FluidEmitterType type) {
    if (!state) return;
    if (state->working.emitter_count >= MAX_FLUID_EMITTERS) return;
    FluidEmitter emitter = {
        .type = type,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.0f,
        .radius = 0.08f,
        .dir_x = 0.0f,
        .dir_y = -1.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1
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
    state->emitter_object_map[state->working.emitter_count] = -1;
    state->emitter_import_map[state->working.emitter_count] = -1;
    {
        int selected_emitter = (int)state->working.emitter_count;
        state->working.emitter_count++;
        normalize_direction(&state->working.emitters[selected_emitter]);
        scene_editor_select_emitter(state, selected_emitter, -1, -1);
    }
    set_dirty(state);
}

void remove_selected(SceneEditorState *state) {
    if (!state) return;
    size_t count = state->working.emitter_count;
    if (count == 0 || state->selected_emitter < 0 ||
        state->selected_emitter >= (int)count) {
        return;
    }
    remove_emitter_at(state, state->selected_emitter);
    if (state->working.emitter_count == 0) {
        state->selected_emitter = -1;
    } else if (state->selected_emitter >= (int)state->working.emitter_count) {
        state->selected_emitter = (int)state->working.emitter_count - 1;
    }
    cancel_field_edit(state);
    set_dirty(state);
    if (state->working.emitter_count > 0) {
        scene_editor_select_emitter(state, state->selected_emitter, state->selected_object, state->selected_row);
    } else if (state->working.object_count > 0) {
        if (state->selected_object < 0) {
            state->selected_object = 0;
        }
        scene_editor_select_object(state, state->selected_object);
    } else {
        scene_editor_select_none(state);
    }
}

void remove_selected_object(SceneEditorState *state) {
    if (!state || state->selected_object < 0) return;
    int index = state->selected_object;
    if (index >= (int)state->working.object_count) return;
    int em_idx = emitter_index_for_object(state, index);
    if (em_idx >= 0) {
        remove_emitter_at(state, em_idx);
    }
    for (int i = index; i < (int)state->working.object_count - 1; ++i) {
        state->working.objects[i] = state->working.objects[i + 1];
    }
    if (state->working.object_count > 0) {
        state->working.object_count--;
    }
    if (state->working.object_count == 0) {
        if (state->working.emitter_count > 0) {
            if (state->selected_emitter < 0) state->selected_emitter = 0;
            scene_editor_select_emitter(state, state->selected_emitter, -1, -1);
        } else {
            scene_editor_select_none(state);
        }
    } else if (state->selected_object >= (int)state->working.object_count) {
        state->selected_object = (int)state->working.object_count - 1;
        scene_editor_select_object(state, state->selected_object);
    } else {
        scene_editor_sync_selection_session(state);
    }
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
    set_dirty(state);
}
