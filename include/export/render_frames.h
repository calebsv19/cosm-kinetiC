#ifndef RENDER_FRAMES_H
#define RENDER_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

bool render_frames_write_bmp(const uint8_t *rgba_pixels,
                             int width,
                             int height,
                             int pitch,
                             uint64_t frame_index);

#endif // RENDER_FRAMES_H
