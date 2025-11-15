#include "app/app_config.h"
#include <stdio.h>

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
    cfg.velocity_damping  = 0.00001f; // interpreted as viscosity
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

    cfg.quality_index = -1;

    cfg.headless_enabled = false;
    cfg.headless_frame_count = 0;
    cfg.headless_custom_slot = 0;
    cfg.headless_quality_index = -1;
    cfg.headless_skip_present = true;
    snprintf(cfg.headless_output_dir, sizeof(cfg.headless_output_dir), "data/snapshots");

    return cfg;
}
