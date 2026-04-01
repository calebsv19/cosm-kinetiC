#include "app/app_config.h"
#include <stdio.h>

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

AppConfig app_config_default(void) {
    AppConfig cfg;

    cfg.window_w = 772;
    cfg.window_h = 772;

    cfg.grid_w = 256;
    cfg.grid_h = 256;

    cfg.min_dt = 1.0 / 165.0;
    cfg.max_dt = 1.0 / 30.0;
    cfg.physics_fixed_dt = 1.0 / 90.0;
    cfg.max_physics_steps_per_frame = 8;

    cfg.physics_substeps = 2;
    cfg.command_batch_limit = 256;
    cfg.fluid_solver_iterations = 20;

    cfg.density_diffusion = 0.0001f;
    cfg.velocity_damping  = 0.000006f; // interpreted as viscosity
    cfg.density_decay     = 0.05f;    // per second
    cfg.fluid_buoyancy_force = 1.5f;

    cfg.stroke_sample_rate = 240.0;
    cfg.stroke_spacing = 3.0f;

    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_velocity_multiplier = 1.0f;
    cfg.emitter_sink_multiplier = 1.0f;

    cfg.save_volume_frames = false;
    cfg.save_render_frames = false;
    cfg.enable_render_blur = true;
    cfg.render_black_level = 0;

    cfg.quality_index = -1;
    cfg.text_zoom_step = 0;

    cfg.headless_enabled = false;
    cfg.headless_frame_count = 0;
    cfg.headless_custom_slot = 0;
    cfg.headless_quality_index = -1;
    cfg.headless_skip_present = true;
    snprintf(cfg.headless_output_dir, sizeof(cfg.headless_output_dir), "data/snapshots");

    cfg.sim_mode = SIM_MODE_BOX;
    cfg.space_mode = SPACE_MODE_2D;
    cfg.tunnel_inflow_speed = 40.0f;
    cfg.tunnel_inflow_density = 15.0f;
    cfg.tunnel_viscosity_scale = 0.5f;

    // Collider fidelity defaults
    cfg.collider_max_loops = 16;
    cfg.collider_max_loop_vertices = 256;
    cfg.collider_max_parts = 16;
    cfg.collider_max_part_vertices = 32;
    cfg.collider_simplify_epsilon = 1.5f;
    cfg.collider_raster_padding = 0.5f;

    // Broad-phase defaults
    cfg.physics_broadphase_enabled = true;
    cfg.physics_broadphase_cell_size = 128.0f; // pixels/world units; auto if <=0

    // Debug/logging
    cfg.collider_debug_logs = false;

    return cfg;
}

int app_config_text_zoom_step_clamp(int step) {
    return clamp_int(step,
                     PHYSICS_SIM_TEXT_ZOOM_STEP_MIN,
                     PHYSICS_SIM_TEXT_ZOOM_STEP_MAX);
}

int app_config_text_zoom_percent_from_step(int step) {
    int clamped_step = app_config_text_zoom_step_clamp(step);
    int percent = 100 + (clamped_step * 10);
    return clamp_int(percent,
                     PHYSICS_SIM_TEXT_ZOOM_PERCENT_MIN,
                     PHYSICS_SIM_TEXT_ZOOM_PERCENT_MAX);
}

int app_config_text_zoom_percent(const AppConfig *cfg) {
    if (!cfg) {
        return 100;
    }
    return app_config_text_zoom_percent_from_step(cfg->text_zoom_step);
}

int app_config_scale_text_point_size(const AppConfig *cfg,
                                     int base_point_size,
                                     int min_point_size) {
    int scaled = 0;
    if (base_point_size <= 0) {
        return min_point_size > 0 ? min_point_size : 1;
    }
    scaled = (base_point_size * app_config_text_zoom_percent(cfg) + 50) / 100;
    if (min_point_size <= 0) {
        min_point_size = 1;
    }
    if (scaled < min_point_size) {
        scaled = min_point_size;
    }
    return scaled;
}
