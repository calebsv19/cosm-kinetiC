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

    cfg.headless_enabled = false;
    cfg.headless_frame_count = 0;
    cfg.headless_custom_slot = 0;
    cfg.headless_quality_index = -1;
    cfg.headless_skip_present = true;
    snprintf(cfg.headless_output_dir, sizeof(cfg.headless_output_dir), "data/snapshots");

    cfg.sim_mode = SIM_MODE_BOX;
    cfg.tunnel_inflow_speed = 40.0f;
    cfg.tunnel_inflow_density = 15.0f;
    cfg.tunnel_viscosity_scale = 0.5f;

    // Collider fidelity defaults
    cfg.collider_max_loops = 16;
    cfg.collider_max_loop_vertices = 256;
    cfg.collider_max_parts = 8;
    cfg.collider_max_part_vertices = 32;
    cfg.collider_simplify_epsilon = 1.5f;
    cfg.collider_curve_sample_rate = 24.0f; // samples per 100 px of path length
    cfg.collider_raster_padding = 0.5f;

    // Primitive collider defaults (can be tuned later).
    cfg.collider_primitives_enabled   = false; // start disabled until the pipeline is ready
    cfg.collider_corner_angle_deg     = 22.5f;
    cfg.collider_corner_simplify_eps  = 0.75f;
    cfg.collider_max_primitives       = 16;
    cfg.collider_max_hull_vertices    = 8;
    cfg.collider_capsule_max_len_ratio = 6.0f;
    cfg.collider_region_grid_res      = 64;
    cfg.collider_region_min_cells     = 4;
    cfg.collider_region_offset_eps    = 0.5f;

    // Broad-phase defaults
    cfg.physics_broadphase_enabled = true;
    cfg.physics_broadphase_cell_size = 128.0f; // pixels/world units; auto if <=0

    // Enable primitive colliders by default for testing; can be disabled in config.
    cfg.collider_primitives_enabled = true;

    // Debug/logging
    cfg.collider_debug_logs = false;

    return cfg;
}
