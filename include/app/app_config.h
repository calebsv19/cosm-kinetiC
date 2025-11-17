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
} AppConfig;

AppConfig app_config_default(void);

#endif // APP_CONFIG_H
