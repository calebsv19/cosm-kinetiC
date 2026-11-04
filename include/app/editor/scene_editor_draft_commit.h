#ifndef SCENE_EDITOR_DRAFT_COMMIT_H
#define SCENE_EDITOR_DRAFT_COMMIT_H
#include "app/editor/scene_editor_internal.h"
/* Name edits belong to the working draft until the editor publishes Apply. */
void scene_editor_draft_begin_name(SceneEditorState *state);
void scene_editor_draft_publish(SceneEditorState *state);
#endif
