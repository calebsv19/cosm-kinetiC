#ifndef SCENE_EDITOR_MODEL_H
#define SCENE_EDITOR_MODEL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "app/editor/scene_editor_internal.h"

void adjust_emitter_radius(FluidEmitter *em, float scale);

void begin_field_edit(SceneEditorState *state, NumericField *field);
void cancel_field_edit(SceneEditorState *state);
void commit_field_edit(SceneEditorState *state);
bool field_handle_key(SceneEditorState *state, SDL_Keycode key);

void nudge_selected(SceneEditorState *state, float dx, float dy);

const char *emitter_type_name(FluidEmitterType type);
const char *boundary_edge_name(int edge);
const char *boundary_mode_label(BoundaryFlowMode mode);

void cycle_boundary_emitter(SceneEditorState *state, int edge);
void set_boundary_receiver(SceneEditorState *state, int edge);
void clear_boundary(SceneEditorState *state, int edge);

void sync_emitter_to_object(SceneEditorState *state, int obj_index);
int emitter_index_for_object(const SceneEditorState *state, int obj_index);
int emitter_index_for_import(const SceneEditorState *state, int import_index);
void sync_emitter_to_import(SceneEditorState *state, int import_index);
void remove_emitter_at(SceneEditorState *state, int em_idx);
int ensure_emitter_for_object(SceneEditorState *state,
                              int obj_index,
                              FluidEmitterType type,
                              bool toggle_clear);
int ensure_emitter_for_import(SceneEditorState *state,
                              int import_index,
                              FluidEmitterType type,
                              bool toggle_clear);
void add_emitter(SceneEditorState *state, FluidEmitterType type);
void remove_selected(SceneEditorState *state);
void remove_selected_object(SceneEditorState *state);

void clamp_object(PresetObject *obj);
bool object_is_outside(float x, float y);

#endif // SCENE_EDITOR_MODEL_H
