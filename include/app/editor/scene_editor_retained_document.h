#ifndef SCENE_EDITOR_RETAINED_DOCUMENT_H
#define SCENE_EDITOR_RETAINED_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>

void scene_editor_retained_document_name_from_path(const char *runtime_scene_path,
                                                   const char *provenance_scene_id,
                                                   char *out_name,
                                                   size_t out_name_size);

bool scene_editor_retained_document_is_runtime_user_path(const char *runtime_dir,
                                                         const char *path);

bool scene_editor_retained_document_resolve_save_path(const char *runtime_dir,
                                                      const char *current_document_path,
                                                      const char *document_name,
                                                      const char *provenance_scene_id,
                                                      char *out_path,
                                                      size_t out_path_size);

#endif
