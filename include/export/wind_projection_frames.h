#ifndef WIND_PROJECTION_FRAMES_H
#define WIND_PROJECTION_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

#include "app/scene_state.h"

bool wind_projection_frames_write_bmp(const SceneState *scene, uint64_t frame_index);
bool wind_projection_frames_write_render_fallback_bmp(const SceneState *scene,
                                                      uint64_t frame_index);

#endif
