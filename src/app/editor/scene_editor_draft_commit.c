#include "app/editor/scene_editor_draft_commit.h"
#include <stdio.h>

void scene_editor_draft_begin_name(SceneEditorState *state) {
    if (!state) return;
    state->name_edit_ptr = state->local_name;
    state->name_edit_capacity = sizeof(state->local_name);
    if (state->name_buffer && state->name_capacity &&
        state->name_capacity < state->name_edit_capacity)
        state->name_edit_capacity = state->name_capacity;
    snprintf(state->local_name, state->name_edit_capacity, "%s",
             state->working.name ? state->working.name : "");
    state->working.name = state->local_name;
}

void scene_editor_draft_publish(SceneEditorState *state) {
    if (!state || !state->applied || !state->target) return;
    /* Never publish a pointer into the editor's stack. Without caller-owned
       name storage, preserve the target's existing name lifetime. */
    const char *name = state->target->name;
    if (state->name_buffer && state->name_capacity) {
        snprintf(state->name_buffer, state->name_capacity, "%s", state->local_name);
        name = state->name_buffer;
    }
    state->working.is_custom = true;
    *state->target = state->working;
    state->target->name = name;
}
