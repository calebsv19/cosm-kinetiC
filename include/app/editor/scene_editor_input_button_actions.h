#ifndef SCENE_EDITOR_INPUT_BUTTON_ACTIONS_H
#define SCENE_EDITOR_INPUT_BUTTON_ACTIONS_H

#include <stdbool.h>

typedef struct SceneEditorState SceneEditorState;

bool scene_editor_input_handle_button_actions(SceneEditorState *state, int x, int y);

#endif // SCENE_EDITOR_INPUT_BUTTON_ACTIONS_H
