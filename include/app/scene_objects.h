#ifndef SCENE_OBJECTS_H
#define SCENE_OBJECTS_H

#include "app/scene_state.h"

// Initialize/shutdown object manager and related flags.
void scene_objects_init(SceneState *scene);
void scene_objects_shutdown(SceneState *scene);

// Add preset objects (circles/boxes) and apply static mask contributions.
void scene_objects_add_presets(SceneState *scene);

// Rebuild dynamic bodies for gravity-enabled imports (uses already-built colliders).
void scene_objects_rebuild_import_bodies(SceneState *scene);

// Toggle gravity for all objects.
void scene_objects_set_gravity(SceneState *scene, bool enabled);

// Set restitution based on elastic flag.
void scene_objects_set_elastic(SceneState *scene, bool elastic);

// Reset gravity-enabled import bodies to their start transforms and zero velocity.
void scene_objects_reset_gravity(SceneState *scene);

#endif // SCENE_OBJECTS_H
