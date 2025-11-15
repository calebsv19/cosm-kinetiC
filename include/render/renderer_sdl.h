#ifndef RENDERER_SDL_H
#define RENDERER_SDL_H

#include <stdbool.h>

#include "app/scene_state.h"

typedef struct RendererHudInfo {
    const char *preset_name;
    bool        preset_is_custom;
    int         grid_w;
    int         grid_h;
    size_t      emitter_count;
    size_t      stroke_samples;
    bool        paused;
} RendererHudInfo;

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH);
void renderer_sdl_shutdown(void);

bool renderer_sdl_render_scene(const SceneState *scene);
void renderer_sdl_present_with_hud(const RendererHudInfo *hud);
bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch);
void renderer_sdl_free_capture(uint8_t *pixels);
int renderer_sdl_output_width(void);
int renderer_sdl_output_height(void);

static inline void renderer_sdl_draw(const SceneState *scene,
                                     const RendererHudInfo *hud) {
    if (renderer_sdl_render_scene(scene)) {
        renderer_sdl_present_with_hud(hud);
    }
}

#endif // RENDERER_SDL_H
