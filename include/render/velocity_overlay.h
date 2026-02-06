#ifndef VELOCITY_OVERLAY_H
#define VELOCITY_OVERLAY_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "app/scene_state.h"

typedef struct VelocityOverlayConfig {
    int   sample_stride;  // grid spacing between vectors
    float vector_scale;   // multiplier when converting velocity to pixels
    float speed_threshold;
    bool  fixed_length;
    float fixed_fraction;
} VelocityOverlayConfig;

typedef enum VelocityOverlayRenderSource {
    VELOCITY_OVERLAY_SOURCE_LEGACY = 0,
    VELOCITY_OVERLAY_SOURCE_KIT_VIZ
} VelocityOverlayRenderSource;

void velocity_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h,
                           const VelocityOverlayConfig *cfg);

VelocityOverlayRenderSource velocity_overlay_draw_adapter_first(const SceneState *scene,
                                                                SDL_Renderer *renderer,
                                                                int window_w,
                                                                int window_h,
                                                                const VelocityOverlayConfig *cfg,
                                                                bool prefer_kit_viz);

#endif // VELOCITY_OVERLAY_H
