#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stddef.h>

typedef struct AppConfig {
    int window_w;
    int window_h;

    int grid_w;
    int grid_h;

    double min_dt;
    double max_dt;

    int    physics_substeps;
    int    command_batch_limit; // commands processed per frame (0 = unlimited)

    float  density_diffusion;  // how strongly density diffuses each step
    float  velocity_damping;   // multiplicative damping on velocity per step
    float  density_decay;      // fractional fade of density per second
    float  fluid_buoyancy_force; // upward force applied per density unit

    double stroke_sample_rate; // samples per second collected from cursor
    float  stroke_spacing;     // pixel spacing between stroke samples

    float  emitter_density_multiplier;
    float  emitter_velocity_multiplier;
    float  emitter_sink_multiplier;
} AppConfig;

AppConfig app_config_default(void);

#endif // APP_CONFIG_H
