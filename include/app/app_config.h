#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stddef.h>
#include <stdbool.h>

typedef enum SimulationMode {
    SIM_MODE_BOX = 0,
    SIM_MODE_WIND_TUNNEL
} SimulationMode;

#define SIMULATION_MODE_COUNT 2

typedef struct AppConfig {
    int window_w;
    int window_h;

    int grid_w;
    int grid_h;

    double min_dt;
    double max_dt;
    double physics_fixed_dt;
    int    max_physics_steps_per_frame;

    int    physics_substeps;
    int    command_batch_limit; // commands processed per frame (0 = unlimited)
    int    fluid_solver_iterations;

    float  density_diffusion;  // how strongly density diffuses each step
    float  velocity_damping;   // multiplicative damping on velocity per step
    float  density_decay;      // fractional fade of density per second
    float  fluid_buoyancy_force; // upward force applied per density unit

    double stroke_sample_rate; // samples per second collected from cursor
    float  stroke_spacing;     // pixel spacing between stroke samples

    float  emitter_density_multiplier;
    float  emitter_velocity_multiplier;
    float  emitter_sink_multiplier;

    bool   save_volume_frames;
    bool   save_render_frames;
    bool   enable_render_blur;
    int    render_black_level; // 0-255 base luminance for empty space

    int    quality_index;

    bool   headless_enabled;
    int    headless_frame_count;
    int    headless_custom_slot;
    int    headless_quality_index;
    bool   headless_skip_present;
    char   headless_output_dir[256];

    SimulationMode sim_mode;
    float  tunnel_inflow_speed;
    float  tunnel_inflow_density;
    float  tunnel_viscosity_scale;

    // Collider generation / fidelity controls (authoring -> physics).
    int    collider_max_loops;           // max closed paths consumed per shape
    int    collider_max_loop_vertices;   // max vertices per extracted loop (after sampling)
    int    collider_max_parts;           // max convex parts per collider
    int    collider_max_part_vertices;   // max vertices per convex part
    float  collider_simplify_epsilon;    // simplification tolerance in grid space
    float  collider_raster_padding;      // cells of padding when rasterizing dynamic bodies

    // Physics broad-phase (coarse grid)
    bool   physics_broadphase_enabled;
    float  physics_broadphase_cell_size; // world units; 0 => auto

    // Debug/logging
    bool   collider_debug_logs;          // verbose collider logging
} AppConfig;

AppConfig app_config_default(void);

#endif // APP_CONFIG_H
