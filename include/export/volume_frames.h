#ifndef VOLUME_FRAMES_H
#define VOLUME_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

struct SceneState;

bool volume_frames_write(const struct SceneState *scene,
                         uint64_t frame_index);

#endif // VOLUME_FRAMES_H
