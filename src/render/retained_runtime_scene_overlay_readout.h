#ifndef PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_READOUT_H
#define PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_READOUT_H

#include <SDL2/SDL.h>
#include <stddef.h>

#include "app/editor/scene_editor_viewport.h"
#include "app/sim_runtime_backend.h"
#include "core_scene.h"

typedef struct SceneState SceneState;

int retained_runtime_overlay_readout_stride_for_cell_count(size_t cell_count);
float retained_runtime_overlay_readout_density_threshold(float max_density);
CoreObjectVec3 retained_runtime_overlay_readout_voxel_center(const SceneDebugVolumeView3D *view,
                                                             int x,
                                                             int y,
                                                             int z);
void retained_runtime_overlay_draw_volume_readout(SDL_Renderer *renderer,
                                                  const SceneEditorViewportState *viewport,
                                                  int window_w,
                                                  int window_h,
                                                  const SceneState *scene);

#endif
