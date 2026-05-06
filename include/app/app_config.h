#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stddef.h>
#include <stdbool.h>

typedef enum SimulationMode {
    SIM_MODE_BOX = 0,
    SIM_MODE_WIND_TUNNEL,
    SIM_MODE_STRUCTURAL
} SimulationMode;

#define SIMULATION_MODE_COUNT 3

typedef enum SpaceMode {
    SPACE_MODE_2D = 0,
    SPACE_MODE_3D
} SpaceMode;

#define SPACE_MODE_COUNT 2

typedef struct AppConfig {
    int window_w;
    int window_h;

    int grid_w;
    int grid_h;
    int grid_d;

    double min_dt;
    double max_dt;
    double physics_fixed_dt;
    int    max_physics_steps_per_frame;

    int    physics_substeps;
    int    command_batch_limit; // commands processed per frame (0 = unlimited)
    int    fluid_solver_iterations;
    int    fluid_3d_solver_region_cell_budget; // 0 = runtime default
    float  fluid_3d_max_velocity_displacement_cells; // <=0 = runtime default

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
    int    text_zoom_step; // runtime UI text zoom step; persisted in runtime app state

    bool   headless_enabled;
    int    headless_frame_count;
    int    headless_custom_slot;
    int    headless_quality_index;
    bool   headless_skip_present;
    char   input_root[256];
    char   headless_output_dir[256];

    SimulationMode sim_mode;
    SpaceMode space_mode;
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

enum {
    PHYSICS_SIM_TEXT_ZOOM_STEP_MIN = -4,
    PHYSICS_SIM_TEXT_ZOOM_STEP_MAX = 5,
    PHYSICS_SIM_TEXT_ZOOM_PERCENT_MIN = 60,
    PHYSICS_SIM_TEXT_ZOOM_PERCENT_MAX = 180
};

#define PHYSICS_SIM_3D_SOLVER_REGION_CELL_BUDGET_DEFAULT ((size_t)5 * 1024 * 1024)
#define PHYSICS_SIM_3D_MAX_VELOCITY_DISPLACEMENT_CELLS_DEFAULT 1.5f

AppConfig app_config_default(void);
int app_config_text_zoom_step_clamp(int step);
int app_config_text_zoom_percent_from_step(int step);
int app_config_text_zoom_percent(const AppConfig *cfg);
int app_config_scale_text_point_size(const AppConfig *cfg,
                                     int base_point_size,
                                     int min_point_size);

static inline size_t app_config_3d_solver_region_cell_budget(const AppConfig *cfg) {
    if (cfg && cfg->fluid_3d_solver_region_cell_budget > 0) {
        return (size_t)cfg->fluid_3d_solver_region_cell_budget;
    }
    return PHYSICS_SIM_3D_SOLVER_REGION_CELL_BUDGET_DEFAULT;
}

static inline bool app_config_3d_solver_region_cell_budget_overridden(const AppConfig *cfg) {
    return cfg && cfg->fluid_3d_solver_region_cell_budget > 0;
}

static inline float app_config_3d_max_velocity_displacement_cells(const AppConfig *cfg) {
    if (cfg && cfg->fluid_3d_max_velocity_displacement_cells > 0.0f) {
        return cfg->fluid_3d_max_velocity_displacement_cells;
    }
    return PHYSICS_SIM_3D_MAX_VELOCITY_DISPLACEMENT_CELLS_DEFAULT;
}

static inline bool app_config_3d_max_velocity_displacement_cells_overridden(const AppConfig *cfg) {
    return cfg && cfg->fluid_3d_max_velocity_displacement_cells > 0.0f;
}

#endif // APP_CONFIG_H
