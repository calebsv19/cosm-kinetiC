#ifndef RENDERER_SDL_H
#define RENDERER_SDL_H

#include <stdbool.h>
#include <stdint.h>

#include "app/scene_state.h"

typedef struct RendererHudInfo {
    const char *preset_name;
    bool        preset_is_custom;
    int         grid_w;
    int         grid_h;
    int         window_w;
    int         window_h;
    uint64_t    frame_index;
    size_t      emitter_count;
    size_t      stroke_samples;
    bool        paused;
    SimulationMode sim_mode;
    SpaceMode   requested_space_mode;
    SpaceMode   projection_space_mode;
    SimBackendLane backend_lane;
    bool        backend_uses_canonical_2d_solver;
    SimRuntimeBackendKind backend_kind;
    int         backend_requested_major_axis_cells;
    int         backend_applied_major_axis_cells;
    int         backend_requested_depth_cells;
    int         backend_applied_depth_cells;
    SimRuntime3DDepthPolicy backend_depth_policy;
    int         backend_requested_solver_iterations;
    int         backend_applied_solver_iterations;
    int         backend_domain_w;
    int         backend_domain_h;
    int         backend_domain_d;
    size_t      backend_cell_count;
    bool        backend_volumetric_emitters_free_live;
    bool        backend_volumetric_emitters_attached_live;
    bool        backend_volumetric_obstacles_live;
    bool        backend_full_3d_solver_live;
    bool        backend_world_bounds_valid;
    float       backend_world_min_x;
    float       backend_world_min_y;
    float       backend_world_min_z;
    float       backend_world_max_x;
    float       backend_world_max_y;
    float       backend_world_max_z;
    float       backend_voxel_size;
    bool        backend_scene_up_valid;
    float       backend_scene_up_x;
    float       backend_scene_up_y;
    float       backend_scene_up_z;
    PhysicsSimRuntimeSceneUpSource backend_scene_up_source;
    bool        backend_compatibility_view_2d_available;
    bool        backend_compatibility_view_2d_derived;
    int         backend_compatibility_slice_z;
    bool        backend_compatibility_slice_has_activity;
    bool        backend_compatibility_slice_has_obstacles;
    bool        backend_secondary_debug_slice_stack_live;
    int         backend_secondary_debug_slice_stack_radius;
    SimRuntimeInitialStateSource backend_initial_state_source;
    bool        backend_atmospheric_seeded;
    uint32_t    backend_atmospheric_seed;
    size_t      backend_atmospheric_seeded_cell_count;
    float       backend_atmospheric_seed_max_density;
    float       backend_atmospheric_seed_max_velocity_magnitude;
    int         atmospheric_warm_start_status;
    int         atmospheric_warm_start_source_kind;
    const char *atmospheric_warm_start_path;
    const char *atmospheric_warm_start_error;
    int         atmospheric_warm_start_w;
    int         atmospheric_warm_start_h;
    int         atmospheric_warm_start_d;
    size_t      atmospheric_warm_start_cell_count;
    size_t      atmospheric_warm_start_active_density_cells;
    size_t      atmospheric_warm_start_solid_cells;
    float       atmospheric_warm_start_max_density;
    float       atmospheric_warm_start_max_velocity_magnitude;
    bool        backend_debug_volume_view_3d_available;
    size_t      backend_debug_volume_active_density_cells;
    size_t      backend_debug_volume_solid_cells;
    float       backend_debug_volume_max_density;
    float       backend_debug_volume_max_velocity_magnitude;
    bool        backend_debug_volume_scene_up_velocity_valid;
    float       backend_debug_volume_scene_up_velocity_avg;
    float       backend_debug_volume_scene_up_velocity_peak;
    size_t      backend_emitter_step_emitters_applied;
    size_t      backend_emitter_step_free_emitters_applied;
    size_t      backend_emitter_step_attached_emitters_applied;
    size_t      backend_emitter_step_affected_cells;
    size_t      backend_emitter_step_last_footprint_cells;
    float       backend_emitter_step_density_delta;
    float       backend_emitter_step_velocity_magnitude_delta;
    bool        backend_wind_analysis_available;
    size_t      backend_wind_analysis_sampled_cells;
    float       backend_wind_analysis_pressure_delta;
    float       backend_wind_analysis_inlet_throughput;
    float       backend_wind_analysis_outlet_throughput;
    float       backend_wind_analysis_drag_pressure_proxy;
    bool        backend_wind_analysis_object_drag_available;
    float       backend_wind_analysis_object_drag_pressure_proxy;
    float       backend_wind_analysis_vorticity_avg;
    float       backend_wind_analysis_vorticity_max;
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
    bool        retained_runtime_visual_active;
    bool        retained_runtime_slice_overlay_enabled;
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
