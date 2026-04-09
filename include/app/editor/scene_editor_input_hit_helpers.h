#ifndef SCENE_EDITOR_INPUT_HIT_HELPERS_H
#define SCENE_EDITOR_INPUT_HIT_HELPERS_H

#include <stdbool.h>

typedef struct SceneEditorState SceneEditorState;
typedef struct SceneEditorHit SceneEditorHit;

bool scene_editor_input_hit_matches_selection(const SceneEditorState *state, const SceneEditorHit *hit);
void scene_editor_input_clear_drag_flags(SceneEditorState *state);
void scene_editor_input_select_from_hit(SceneEditorState *state, const SceneEditorHit *hit);
void scene_editor_input_prepare_drag_for_hit(SceneEditorState *state,
                                             const SceneEditorHit *hit,
                                             int x,
                                             int y);

#endif // SCENE_EDITOR_INPUT_HIT_HELPERS_H
