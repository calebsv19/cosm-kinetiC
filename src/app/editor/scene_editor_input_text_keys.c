#include "app/editor/scene_editor_input.h"

#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input_common.h"
#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/editor/scene_editor_model.h"
#include <stdio.h>

static bool editor_retained_scene_read_only(const SceneEditorState *state) {
    return state && physics_sim_editor_session_has_retained_scene(&state->session);
}

void editor_text_input(void *user, const char *text) {
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
    if (state->editing_depth) {
        text_input_handle_text(&state->depth_input, text);
        return;
    }
}

void editor_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
    state->viewport.alt_modifier_down = (mod & KMOD_ALT) != 0;
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
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH, false);
        } else {
            text_input_handle_key(&state->width_input, key);
        }
        return;
    }
    if (state->editing_height) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT, false);
        } else {
            text_input_handle_key(&state->height_input, key);
        }
        return;
    }
    if (state->editing_depth) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH, false);
        } else {
            text_input_handle_key(&state->depth_input, key);
        }
        return;
    }
    if (key == SDLK_ESCAPE) {
        scene_editor_select_none(state);
        physics_sim_editor_session_select_retained_index(&state->session, -1);
        state->hover_object = -1;
        state->hover_emitter = -1;
        state->selected_import_row = -1;
        state->dragging = false;
        state->dragging_object = false;
        state->dragging_object_handle = false;
        state->dragging_import_new = false;
        state->showing_import_picker = false;
        return;
    }

    if (key == SDLK_e && state->selected_object >= 0) {
        int em_idx = emitter_index_for_object(state, state->selected_object);
        if (em_idx >= 0) {
            remove_emitter_at(state, em_idx);
            scene_editor_select_object(state, state->selected_object);
            set_dirty(state);
        }
        return;
    }
    if (field_handle_key(state, key)) {
        return;
    }

    if (key == SDLK_f) {
        editor_frame_viewport_to_scene(state);
        return;
    }

    if (editor_retained_scene_read_only(state)) {
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
        if (state->showing_import_picker && state->selected_import_row >= 0) {
            scene_editor_input_add_import_from_picker(state, state->selected_import_row);
        } else {
            scene_editor_input_finish_and_apply(state);
        }
        break;
    case SDLK_ESCAPE:
        scene_editor_input_cancel_and_close(state);
        break;
    case SDLK_TAB: {
        if (state->working.emitter_count == 0) break;
        if (state->selected_emitter < 0) state->selected_emitter = 0;
        int dir = (mod & KMOD_SHIFT) ? -1 : 1;
        int next = state->selected_emitter + dir;
        if (next < 0) next = (int)state->working.emitter_count - 1;
        if (next >= (int)state->working.emitter_count) next = 0;
        scene_editor_select_emitter(state, next, -1, -1);
        break;
    }
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        if (state->showing_import_picker) {
            if (state->selected_import_row >= 0 &&
                state->selected_import_row < state->import_file_count) {
            }
        } else if (state->selected_row >= 0 &&
                   state->selected_row < (int)state->working.import_shape_count) {
            scene_editor_input_remove_import_at(state, state->selected_row);
        } else if (state->selection_kind == SELECTION_OBJECT) {
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
    case SDLK_g:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            int em_idx = emitter_index_for_object(state, state->selected_object);
            if (em_idx < 0) {
                PresetObject *obj = &state->working.objects[state->selected_object];
                obj->gravity_enabled = !obj->gravity_enabled;
                fprintf(stderr, "[editor] G pressed: toggled gravity on obj %d -> %d\n",
                        state->selected_object, obj->gravity_enabled ? 1 : 0);
                set_dirty(state);
            } else {
                fprintf(stderr, "[editor] G pressed on emitter-bound obj %d (ignored)\n",
                        state->selected_object);
            }
        } else if (state->selection_kind == SELECTION_IMPORT &&
                   state->selected_row >= 0 &&
                   state->selected_row < (int)state->working.import_shape_count) {
            int imp_idx = state->selected_row;
            int em_idx = emitter_index_for_import(state, imp_idx);
            if (em_idx < 0) {
                ImportedShape *imp = &state->working.import_shapes[imp_idx];
                imp->gravity_enabled = !imp->gravity_enabled;
                fprintf(stderr, "[editor] G pressed: toggled gravity on import %d -> %d\n",
                        imp_idx, imp->gravity_enabled ? 1 : 0);
                set_dirty(state);
            } else {
                fprintf(stderr, "[editor] G on emitter-bound import %d (ignored)\n", imp_idx);
            }
        } else {
            fprintf(stderr, "[editor] G pressed with no object selection (kind=%d, obj=%d)\n",
                    state->selection_kind, state->selected_object);
        }
        break;
    default:
        break;
    }
}

void editor_key_up(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneEditorState *state = (SceneEditorState *)user;
    (void)key;
    if (!state) return;
    state->viewport.alt_modifier_down = (mod & KMOD_ALT) != 0;
    if (!state->viewport.alt_modifier_down &&
        state->viewport.navigation_active &&
        state->viewport.navigation_mode == SCENE_EDITOR_VIEWPORT_NAV_ORBIT) {
        scene_editor_viewport_end_navigation(&state->viewport);
    }
}
