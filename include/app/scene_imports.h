#ifndef SCENE_IMPORTS_H
#define SCENE_IMPORTS_H

#include <stddef.h>
#include "app/scene_state.h"

// Resolve shape IDs for imports based on the shared shape library.
void scene_imports_resolve(SceneState *scene);

// Reset collider fields for an import.
void scene_import_reset_collider(ImportedShape *imp);

// Rebuild bodies for gravity-enabled imports and update import_body_map.
void scene_imports_rebuild_bodies(SceneState *scene);

#endif // SCENE_IMPORTS_H
