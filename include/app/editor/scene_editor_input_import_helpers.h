#ifndef SCENE_EDITOR_INPUT_IMPORT_HELPERS_H
#define SCENE_EDITOR_INPUT_IMPORT_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct SceneEditorState SceneEditorState;

bool scene_editor_input_convert_import_to_asset(const char *import_path,
                                                const char *configured_root,
                                                char *out_asset_path,
                                                size_t out_sz);
bool scene_editor_input_path_contains_import_segment(const char *path, const char *configured_root);
bool scene_editor_input_add_import_from_picker(SceneEditorState *state, int row);
void scene_editor_input_remove_import_at(SceneEditorState *state, int index);

#endif // SCENE_EDITOR_INPUT_IMPORT_HELPERS_H
