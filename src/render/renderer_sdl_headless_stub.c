#include "render/renderer_sdl.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_window_w = 0;
static int g_window_h = 0;
static bool g_draw_vorticity = false;
static bool g_draw_pressure = false;
static bool g_draw_velocity_vectors = false;
static bool g_draw_flow_particles = false;
static bool g_velocity_fixed_length = false;
static bool g_use_kit_viz_density = true;
static bool g_use_kit_viz_velocity = true;
static bool g_use_kit_viz_pressure = true;
static bool g_use_kit_viz_vorticity = true;
static bool g_use_kit_viz_particles = true;

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH) {
    (void)gridW;
    (void)gridH;
    g_window_w = windowW;
    g_window_h = windowH;
    fprintf(stderr,
            "[renderer] headless worker stub renderer active; render-frame output is unavailable in this build.\n");
    return false;
}

void renderer_sdl_shutdown(void) {
}

bool renderer_sdl_render_scene(const SceneState *scene) {
    (void)scene;
    return false;
}

void renderer_sdl_present_with_hud(const RendererHudInfo *hud) {
    (void)hud;
}

bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch) {
    if (out_rgba) *out_rgba = NULL;
    if (out_pitch) *out_pitch = 0;
    return false;
}

void renderer_sdl_free_capture(uint8_t *pixels) {
    free(pixels);
}

int renderer_sdl_output_width(void) {
    return g_window_w;
}

int renderer_sdl_output_height(void) {
    return g_window_h;
}

bool renderer_sdl_device_lost(void) {
    return false;
}

bool renderer_sdl_toggle_vorticity(void) {
    g_draw_vorticity = !g_draw_vorticity;
    return g_draw_vorticity;
}

bool renderer_sdl_vorticity_enabled(void) {
    return g_draw_vorticity;
}

bool renderer_sdl_toggle_pressure(void) {
    g_draw_pressure = !g_draw_pressure;
    return g_draw_pressure;
}

bool renderer_sdl_pressure_enabled(void) {
    return g_draw_pressure;
}

bool renderer_sdl_toggle_velocity_vectors(void) {
    g_draw_velocity_vectors = !g_draw_velocity_vectors;
    return g_draw_velocity_vectors;
}

bool renderer_sdl_velocity_vectors_enabled(void) {
    return g_draw_velocity_vectors;
}

bool renderer_sdl_toggle_flow_particles(void) {
    g_draw_flow_particles = !g_draw_flow_particles;
    return g_draw_flow_particles;
}

bool renderer_sdl_flow_particles_enabled(void) {
    return g_draw_flow_particles;
}

bool renderer_sdl_toggle_velocity_mode(void) {
    g_velocity_fixed_length = !g_velocity_fixed_length;
    return g_velocity_fixed_length;
}

bool renderer_sdl_velocity_mode_fixed(void) {
    return g_velocity_fixed_length;
}

bool renderer_sdl_toggle_kit_viz_density(void) {
    g_use_kit_viz_density = !g_use_kit_viz_density;
    return g_use_kit_viz_density;
}

bool renderer_sdl_set_kit_viz_density_enabled(bool enabled) {
    g_use_kit_viz_density = enabled;
    return g_use_kit_viz_density;
}

bool renderer_sdl_kit_viz_density_enabled(void) {
    return g_use_kit_viz_density;
}

bool renderer_sdl_density_using_kit_viz(void) {
    return false;
}

bool renderer_sdl_toggle_kit_viz_velocity(void) {
    g_use_kit_viz_velocity = !g_use_kit_viz_velocity;
    return g_use_kit_viz_velocity;
}

bool renderer_sdl_set_kit_viz_velocity_enabled(bool enabled) {
    g_use_kit_viz_velocity = enabled;
    return g_use_kit_viz_velocity;
}

bool renderer_sdl_kit_viz_velocity_enabled(void) {
    return g_use_kit_viz_velocity;
}

bool renderer_sdl_velocity_using_kit_viz(void) {
    return false;
}

bool renderer_sdl_set_kit_viz_pressure_enabled(bool enabled) {
    g_use_kit_viz_pressure = enabled;
    return g_use_kit_viz_pressure;
}

bool renderer_sdl_kit_viz_pressure_enabled(void) {
    return g_use_kit_viz_pressure;
}

bool renderer_sdl_pressure_using_kit_viz(void) {
    return false;
}

bool renderer_sdl_toggle_kit_viz_pressure(void) {
    g_use_kit_viz_pressure = !g_use_kit_viz_pressure;
    return g_use_kit_viz_pressure;
}

bool renderer_sdl_set_kit_viz_vorticity_enabled(bool enabled) {
    g_use_kit_viz_vorticity = enabled;
    return g_use_kit_viz_vorticity;
}

bool renderer_sdl_kit_viz_vorticity_enabled(void) {
    return g_use_kit_viz_vorticity;
}

bool renderer_sdl_vorticity_using_kit_viz(void) {
    return false;
}

bool renderer_sdl_toggle_kit_viz_vorticity(void) {
    g_use_kit_viz_vorticity = !g_use_kit_viz_vorticity;
    return g_use_kit_viz_vorticity;
}

bool renderer_sdl_set_kit_viz_particles_enabled(bool enabled) {
    g_use_kit_viz_particles = enabled;
    return g_use_kit_viz_particles;
}

bool renderer_sdl_kit_viz_particles_enabled(void) {
    return g_use_kit_viz_particles;
}

bool renderer_sdl_particles_using_kit_viz(void) {
    return false;
}

bool renderer_sdl_toggle_kit_viz_particles(void) {
    g_use_kit_viz_particles = !g_use_kit_viz_particles;
    return g_use_kit_viz_particles;
}
