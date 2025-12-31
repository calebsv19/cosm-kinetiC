#ifndef SCENE_MASKS_H
#define SCENE_MASKS_H

#include "app/scene_state.h"

// Build the static mask from preset objects and static imports.
void scene_masks_build_static(SceneState *scene);

// Build emitter masks for emitters attached to imports.
void scene_masks_build_emitter(SceneState *scene);
void scene_masks_free_emitter_masks(SceneState *scene);
void scene_masks_mark_emitters_dirty(SceneState *scene);

// Build the obstacle mask (static + dynamic hook); marks obstacle_mask_dirty false.
void scene_masks_build_obstacle(SceneState *scene);

// Rasterize dynamic bodies into obstacle_mask/vel (keeps static intact).
void scene_masks_rasterize_dynamic(SceneState *scene);

#endif // SCENE_MASKS_H
