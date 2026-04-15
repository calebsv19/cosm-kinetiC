#ifndef SCENE_EDITOR_INPUT_COMMON_H
#define SCENE_EDITOR_INPUT_COMMON_H

#include <stdbool.h>

#include "app/editor/scene_editor_internal.h"
#include "input/input_context.h"

bool scene_editor_input_point_in_rect(const SDL_Rect *rect, int x, int y);
bool scene_editor_input_point_in_editor_side_panels(const SceneEditorState *state, int x, int y);
bool scene_editor_input_point_in_editor_active_viewport(const SceneEditorState *state, int x, int y);
bool scene_editor_input_try_begin_viewport_navigation(SceneEditorState *state,
                                                      const InputPointerState *ptr);
void scene_editor_input_pick_target_for_emitter(SceneEditorState *state, int *out_obj, int *out_imp);
bool scene_editor_input_apply_retained_overlay(SceneEditorState *state);
bool scene_editor_input_save_retained_scene(SceneEditorState *state);
void scene_editor_input_finish_and_apply(SceneEditorState *state);
void scene_editor_input_cancel_and_close(SceneEditorState *state);

#endif // SCENE_EDITOR_INPUT_COMMON_H
