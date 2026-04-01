#ifndef RENDERER_SDL_H
#define RENDERER_SDL_H

#include <stdbool.h>

#include "app/scene_state.h"

typedef struct RendererHudInfo {
    const char *preset_name;
    bool        preset_is_custom;
    int         grid_w;
    int         grid_h;
    int         window_w;
    int         window_h;
    size_t      emitter_count;
    size_t      stroke_samples;
    bool        paused;
    SimulationMode sim_mode;
    SpaceMode   requested_space_mode;
    SpaceMode   projection_space_mode;
    SimBackendLane backend_lane;
    bool        backend_uses_canonical_2d_solver;
    float       tunnel_inflow_speed;
    bool        vorticity_enabled;
    bool        pressure_enabled;
    bool        velocity_overlay_enabled;
    bool        particle_overlay_enabled;
    bool        velocity_fixed_length;
    bool        kit_viz_density_enabled;
    bool        kit_viz_density_active;
    bool        kit_viz_velocity_enabled;
    bool        kit_viz_velocity_active;
    bool        kit_viz_pressure_enabled;
    bool        kit_viz_pressure_active;
    bool        kit_viz_vorticity_enabled;
    bool        kit_viz_vorticity_active;
    bool        kit_viz_particles_enabled;
    bool        kit_viz_particles_active;
    bool        objects_gravity_enabled;
    const char *quality_name;
    int         solver_iterations;
    int         physics_substeps;
} RendererHudInfo;

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH);
void renderer_sdl_shutdown(void);

bool renderer_sdl_render_scene(const SceneState *scene);
void renderer_sdl_present_with_hud(const RendererHudInfo *hud);
bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch);
void renderer_sdl_free_capture(uint8_t *pixels);
int renderer_sdl_output_width(void);
int renderer_sdl_output_height(void);
bool renderer_sdl_device_lost(void);
bool renderer_sdl_toggle_vorticity(void);
bool renderer_sdl_vorticity_enabled(void);
bool renderer_sdl_toggle_pressure(void);
bool renderer_sdl_pressure_enabled(void);
bool renderer_sdl_toggle_velocity_vectors(void);
bool renderer_sdl_velocity_vectors_enabled(void);
bool renderer_sdl_toggle_flow_particles(void);
bool renderer_sdl_flow_particles_enabled(void);
bool renderer_sdl_toggle_velocity_mode(void);
bool renderer_sdl_velocity_mode_fixed(void);
bool renderer_sdl_toggle_kit_viz_density(void);
bool renderer_sdl_set_kit_viz_density_enabled(bool enabled);
bool renderer_sdl_kit_viz_density_enabled(void);
bool renderer_sdl_density_using_kit_viz(void);
bool renderer_sdl_toggle_kit_viz_velocity(void);
bool renderer_sdl_set_kit_viz_velocity_enabled(bool enabled);
bool renderer_sdl_kit_viz_velocity_enabled(void);
bool renderer_sdl_velocity_using_kit_viz(void);
bool renderer_sdl_set_kit_viz_pressure_enabled(bool enabled);
bool renderer_sdl_kit_viz_pressure_enabled(void);
bool renderer_sdl_pressure_using_kit_viz(void);
bool renderer_sdl_toggle_kit_viz_pressure(void);
bool renderer_sdl_set_kit_viz_vorticity_enabled(bool enabled);
bool renderer_sdl_kit_viz_vorticity_enabled(void);
bool renderer_sdl_vorticity_using_kit_viz(void);
bool renderer_sdl_toggle_kit_viz_vorticity(void);
bool renderer_sdl_set_kit_viz_particles_enabled(bool enabled);
bool renderer_sdl_kit_viz_particles_enabled(void);
bool renderer_sdl_particles_using_kit_viz(void);
bool renderer_sdl_toggle_kit_viz_particles(void);

static inline void renderer_sdl_draw(const SceneState *scene,
                                     const RendererHudInfo *hud) {
    if (renderer_sdl_render_scene(scene)) {
        renderer_sdl_present_with_hud(hud);
    }
}

#endif // RENDERER_SDL_H
