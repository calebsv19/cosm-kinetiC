#ifndef VELOCITY_OVERLAY_H
#define VELOCITY_OVERLAY_H

#include <SDL2/SDL.h>

#include "app/scene_state.h"

typedef struct VelocityOverlayConfig {
    int   sample_stride;  // grid spacing between vectors
    float vector_scale;   // multiplier when converting velocity to pixels
    float speed_threshold;
    bool  fixed_length;
    float fixed_fraction;
} VelocityOverlayConfig;

void velocity_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h,
                           const VelocityOverlayConfig *cfg);

#endif // VELOCITY_OVERLAY_H
