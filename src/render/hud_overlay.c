#include "render/hud_overlay.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "render/font_bridge.h"
#include "render/text_draw.h"
#include "app/atmospheric/atmospheric_warm_start.h"

static SDL_Renderer *g_hud_renderer = NULL;
static TTF_Font     *g_hud_font     = NULL;

static TTF_Font *load_hud_font(void) {
    TTF_Font *font = NULL;
    (void)physics_sim_font_bridge_open(g_hud_renderer,
                                       NULL,
                                       PHYSICS_SIM_FONT_SLOT_HUD_OVERLAY,
                                       &font,
                                       NULL);
    if (font) {
        return font;
    }
    fprintf(stderr, "[hud] Failed to load HUD font: %s\n", TTF_GetError());
    return NULL;
}

static const char *hud_warm_start_source_label(int source_kind) {
    switch ((AtmosphericWarmStartSourceKind)source_kind) {
        case ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW:
            return "VF3D";
        case ATMOSPHERIC_WARM_START_SOURCE_VF3H_PACK:
            return "VF3H pack";
        case ATMOSPHERIC_WARM_START_SOURCE_NONE:
        default:
            return "unknown";
    }
}

bool hud_overlay_init(SDL_Renderer *renderer) {
    g_hud_renderer = renderer;
    g_hud_font = load_hud_font();
    if (!g_hud_font) {
        fprintf(stderr, "[hud] HUD font unavailable, HUD disabled.\n");
        return false;
    }
    return true;
}

void hud_overlay_shutdown(void) {
    physics_sim_font_bridge_close(&g_hud_font);
    g_hud_renderer = NULL;
}

static void measure_hud_text_line(const char *text,
                                  int *w,
                                  int *h) {
    if (!w || !h) return;
    *w = 0;
    *h = 0;
    if (!text) return;
    if (!g_hud_font || !g_hud_renderer) {
        return;
    }
    (void)physics_sim_text_measure_utf8(g_hud_renderer, g_hud_font, text, w, h);
}

static void append_token(char *dst, size_t dst_size, const char *token) {
    if (!dst || dst_size == 0 || !token || !token[0]) return;
    size_t len = strlen(dst);
    if (len >= dst_size - 1) return;
    if (len > 0) {
        snprintf(dst + len, dst_size - len, " %s", token);
    } else {
        snprintf(dst + len, dst_size - len, "%s", token);
    }
}

static const char *hud_space_mode_label(SpaceMode mode) {
    switch (mode) {
    case SPACE_MODE_3D:
        return "3D";
    case SPACE_MODE_2D:
    default:
        return "2D";
    }
}

static const char *hud_backend_kind_label(SimRuntimeBackendKind kind) {
    switch (kind) {
    case SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD:
        return "3D scaffold backend";
    case SIM_RUNTIME_BACKEND_KIND_FLUID_2D:
        return "Fluid2D backend";
    case SIM_RUNTIME_BACKEND_KIND_NONE:
    default:
        return "No backend";
    }
}

void hud_overlay_draw(const RendererHudInfo *hud) {
    if (!hud || !g_hud_renderer || !g_hud_font) return;

    const char *mode_name = "Box";
    if (hud->sim_mode == SIM_MODE_WIND_TUNNEL) {
        mode_name = "Wind";
    } else if (hud->sim_mode == SIM_MODE_STRUCTURAL) {
        mode_name = "Structural";
    } else if (hud->sim_mode == SIM_MODE_ATMOSPHERIC) {
        mode_name = "Atmospheric";
    }

    char status_line[96];
    snprintf(status_line, sizeof(status_line), "%s | %s | Grid %dx%d",
             hud->paused ? "Paused" : "Run",
             mode_name,
             hud->grid_w, hud->grid_h);

    char frame_line[64];
    snprintf(frame_line,
             sizeof(frame_line),
             "Frame: %llu",
             (unsigned long long)hud->frame_index);

    char space_line[120];
    snprintf(space_line,
             sizeof(space_line),
             "Space: %s%s%s",
             hud_space_mode_label(hud->requested_space_mode),
             (hud->requested_space_mode != hud->projection_space_mode) ? " -> " : "",
             (hud->requested_space_mode != hud->projection_space_mode)
                 ? hud_space_mode_label(hud->projection_space_mode)
                 : "");

    char backend_line[160];
    snprintf(backend_line,
             sizeof(backend_line),
             "Backend: %s%s",
             (hud->backend_lane == SIM_BACKEND_CONTROLLED_3D)
                 ? "Controlled 3D lane"
                 : "Canonical 2D lane",
             (hud->backend_kind != SIM_RUNTIME_BACKEND_KIND_NONE)
                 ? ""
                 : " (unavailable)");

    char backend_kind_line[160];
    snprintf(backend_kind_line,
             sizeof(backend_kind_line),
             "Backend impl: %s",
             hud_backend_kind_label(hud->backend_kind));

    char domain_line[160];
    if (hud->backend_domain_d > 1) {
        snprintf(domain_line,
                 sizeof(domain_line),
                 "Domain: %dx%dx%d voxels (%zu cells)",
                 hud->backend_domain_w,
                 hud->backend_domain_h,
                 hud->backend_domain_d,
                 hud->backend_cell_count);
    } else {
        snprintf(domain_line,
                 sizeof(domain_line),
                 "Domain: %dx%d cells (%zu total)",
                 hud->backend_domain_w,
                 hud->backend_domain_h,
                 hud->backend_cell_count);
    }

    char resolution_line[160];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD &&
        hud->backend_requested_major_axis_cells > 0 &&
        hud->backend_applied_major_axis_cells > 0) {
        if (hud->backend_requested_major_axis_cells == hud->backend_applied_major_axis_cells) {
            snprintf(resolution_line,
                     sizeof(resolution_line),
                     "3D resolution: requested XY %dx%d, axis %d -> %d",
                     hud->grid_w,
                     hud->grid_h,
                     hud->backend_requested_major_axis_cells,
                     hud->backend_applied_major_axis_cells);
        } else {
            snprintf(resolution_line,
                     sizeof(resolution_line),
                     "3D resolution: requested XY %dx%d, axis %d -> %d (cap)",
                     hud->grid_w,
                     hud->grid_h,
                     hud->backend_requested_major_axis_cells,
                     hud->backend_applied_major_axis_cells);
        }
    } else {
        resolution_line[0] = '\0';
    }

    char depth_policy_line[160];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD &&
        hud->backend_depth_policy != SIM_RUNTIME_3D_DEPTH_POLICY_NONE) {
        if (hud->backend_requested_depth_cells > 0 &&
            hud->backend_applied_depth_cells > 0) {
            if (hud->backend_requested_depth_cells == hud->backend_applied_depth_cells) {
                snprintf(depth_policy_line,
                         sizeof(depth_policy_line),
                         "3D depth: requested Z %d, applied %d (%s)",
                         hud->backend_requested_depth_cells,
                         hud->backend_applied_depth_cells,
                         sim_runtime_3d_depth_policy_label(hud->backend_depth_policy));
            } else {
                snprintf(depth_policy_line,
                         sizeof(depth_policy_line),
                         "3D depth: requested Z %d, applied %d (%s)",
                         hud->backend_requested_depth_cells,
                         hud->backend_applied_depth_cells,
                         sim_runtime_3d_depth_policy_label(hud->backend_depth_policy));
            }
        } else if (hud->backend_applied_depth_cells > 0) {
            snprintf(depth_policy_line,
                     sizeof(depth_policy_line),
                     "3D depth: derived Z %d (%s)",
                     hud->backend_applied_depth_cells,
                     sim_runtime_3d_depth_policy_label(hud->backend_depth_policy));
        } else {
            snprintf(depth_policy_line,
                     sizeof(depth_policy_line),
                     "3D depth: %s",
                     sim_runtime_3d_depth_policy_label(hud->backend_depth_policy));
        }
    } else {
        depth_policy_line[0] = '\0';
    }

    char solver_contract_line[160];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD &&
        hud->backend_requested_solver_iterations > 0 &&
        hud->backend_applied_solver_iterations > 0) {
        if (hud->backend_requested_solver_iterations == hud->backend_applied_solver_iterations) {
            snprintf(solver_contract_line,
                     sizeof(solver_contract_line),
                     "3D solver: requested %d, applied %d",
                     hud->backend_requested_solver_iterations,
                     hud->backend_applied_solver_iterations);
        } else {
            snprintf(solver_contract_line,
                     sizeof(solver_contract_line),
                     "3D solver: requested %d, applied %d (3D clamp)",
                     hud->backend_requested_solver_iterations,
                     hud->backend_applied_solver_iterations);
        }
    } else {
        solver_contract_line[0] = '\0';
    }

    char domain_extent_line[160];
    if (hud->backend_world_bounds_valid) {
        float span_x = hud->backend_world_max_x - hud->backend_world_min_x;
        float span_y = hud->backend_world_max_y - hud->backend_world_min_y;
        float span_z = hud->backend_world_max_z - hud->backend_world_min_z;
        snprintf(domain_extent_line,
                 sizeof(domain_extent_line),
                 "World span: %.2f x %.2f x %.2f @ %.4f voxel",
                 span_x,
                 span_y,
                 span_z,
                 hud->backend_voxel_size);
    } else {
        domain_extent_line[0] = '\0';
    }

    char compatibility_line[160];
    if (hud->backend_compatibility_view_2d_available) {
        if (hud->backend_compatibility_view_2d_derived && hud->backend_domain_d > 1) {
            snprintf(compatibility_line,
                     sizeof(compatibility_line),
                     "Compat view: live derived XY slice z=%d/%d",
                     hud->backend_compatibility_slice_z,
                     hud->backend_domain_d - 1);
        } else {
            snprintf(compatibility_line,
                     sizeof(compatibility_line),
                     "Compat view: authoritative XY field");
        }
    } else {
        compatibility_line[0] = '\0';
    }

    char backend_status_line[176];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD) {
        snprintf(backend_status_line,
                 sizeof(backend_status_line),
                 "3D status: emitters %s%s%s, obstacles %s, solver %s",
                 hud->backend_volumetric_emitters_free_live ? "free" : "none",
                 hud->backend_volumetric_emitters_attached_live ? "+" : "",
                 hud->backend_volumetric_emitters_attached_live ? "attached" : "",
                 hud->backend_volumetric_obstacles_live ? "live" : "pending",
                 hud->backend_full_3d_solver_live ? "live" : "compat");
    } else {
        backend_status_line[0] = '\0';
    }

    char scene_axis_line[160];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD &&
        hud->backend_scene_up_valid) {
        snprintf(scene_axis_line,
                 sizeof(scene_axis_line),
                 "3D axis: up=(%.2f, %.2f, %.2f) source=%s",
                 hud->backend_scene_up_x,
                 hud->backend_scene_up_y,
                 hud->backend_scene_up_z,
                 physics_sim_runtime_scene_up_source_label(hud->backend_scene_up_source));
    } else {
        scene_axis_line[0] = '\0';
    }

    char compatibility_activity_line[160];
    if (hud->backend_compatibility_view_2d_derived && hud->backend_domain_d > 1) {
        const char *fluid_status = hud->backend_compatibility_slice_has_activity
                                       ? "fluid visible"
                                       : "fluid empty";
        const char *obstacle_status = hud->backend_compatibility_slice_has_obstacles
                                          ? "obstacles present"
                                          : "obstacles clear";
        snprintf(compatibility_activity_line,
                 sizeof(compatibility_activity_line),
                 "Compat slice activity: %s, %s",
                 fluid_status,
                 obstacle_status);
    } else {
        compatibility_activity_line[0] = '\0';
    }

    char volume_truth_line[192];
    if (hud->backend_debug_volume_view_3d_available &&
        hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD) {
        if (hud->backend_debug_volume_scene_up_velocity_valid) {
            snprintf(volume_truth_line,
                     sizeof(volume_truth_line),
                     "3D volume: XYZ live act=%zu solid=%zu rho_max=%.2f |v|_max=%.2f v_up(avg/peak)=%.2f/%.2f",
                     hud->backend_debug_volume_active_density_cells,
                     hud->backend_debug_volume_solid_cells,
                     hud->backend_debug_volume_max_density,
                     hud->backend_debug_volume_max_velocity_magnitude,
                     hud->backend_debug_volume_scene_up_velocity_avg,
                     hud->backend_debug_volume_scene_up_velocity_peak);
        } else {
            snprintf(volume_truth_line,
                     sizeof(volume_truth_line),
                     "3D volume: XYZ live act=%zu solid=%zu rho_max=%.2f |v|_max=%.2f",
                     hud->backend_debug_volume_active_density_cells,
                     hud->backend_debug_volume_solid_cells,
                     hud->backend_debug_volume_max_density,
                     hud->backend_debug_volume_max_velocity_magnitude);
        }
    } else {
        volume_truth_line[0] = '\0';
    }

    char atmosphere_line[224];
    if (hud->sim_mode == SIM_MODE_ATMOSPHERIC &&
        hud->atmospheric_warm_start_status == ATMOSPHERIC_WARM_START_RUNTIME_APPLIED) {
        snprintf(atmosphere_line,
                 sizeof(atmosphere_line),
                 "Warm start: %s %dx%dx%d cells=%zu act=%zu solid=%zu rho_max=%.2f |v|_max=%.2f",
                 hud_warm_start_source_label(hud->atmospheric_warm_start_source_kind),
                 hud->atmospheric_warm_start_w,
                 hud->atmospheric_warm_start_h,
                 hud->atmospheric_warm_start_d,
                 hud->atmospheric_warm_start_cell_count,
                 hud->atmospheric_warm_start_active_density_cells,
                 hud->atmospheric_warm_start_solid_cells,
                 hud->atmospheric_warm_start_max_density,
                 hud->atmospheric_warm_start_max_velocity_magnitude);
    } else if (hud->sim_mode == SIM_MODE_ATMOSPHERIC &&
               hud->atmospheric_warm_start_status == ATMOSPHERIC_WARM_START_RUNTIME_REJECTED) {
        snprintf(atmosphere_line,
                 sizeof(atmosphere_line),
                 "Warm start rejected: %s",
                 hud->atmospheric_warm_start_error
                     ? hud->atmospheric_warm_start_error
                     : "unknown reason");
    } else if ((hud->backend_initial_state_source ==
                    SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE ||
                hud->backend_initial_state_source ==
                    SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_OPTIONAL_LAYER) &&
               hud->backend_atmospheric_seeded) {
        snprintf(atmosphere_line,
                 sizeof(atmosphere_line),
                 "Initial state: %s seed=%u cells=%zu rho_max=%.2f |v|_max=%.2f",
                 sim_runtime_initial_state_source_label(hud->backend_initial_state_source),
                 hud->backend_atmospheric_seed,
                 hud->backend_atmospheric_seeded_cell_count,
                 hud->backend_atmospheric_seed_max_density,
                 hud->backend_atmospheric_seed_max_velocity_magnitude);
    } else if (hud->backend_initial_state_source ==
                   SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE ||
               hud->backend_initial_state_source ==
                   SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_OPTIONAL_LAYER) {
        snprintf(atmosphere_line,
                 sizeof(atmosphere_line),
                 "Initial state: %s seed=%u no active cells",
                 sim_runtime_initial_state_source_label(hud->backend_initial_state_source),
                 hud->backend_atmospheric_seed);
    } else {
        atmosphere_line[0] = '\0';
    }

    char emitter_step_line[176];
    if (hud->backend_kind == SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD) {
        snprintf(emitter_step_line,
                 sizeof(emitter_step_line),
                 "Emit step: n=%zu free=%zu att=%zu vox=%zu last=%zu rho=%+.2f vel=%.2f",
                 hud->backend_emitter_step_emitters_applied,
                 hud->backend_emitter_step_free_emitters_applied,
                 hud->backend_emitter_step_attached_emitters_applied,
                 hud->backend_emitter_step_affected_cells,
                 hud->backend_emitter_step_last_footprint_cells,
                 hud->backend_emitter_step_density_delta,
                 hud->backend_emitter_step_velocity_magnitude_delta);
    } else {
        emitter_step_line[0] = '\0';
    }

    char debug_cue_line[160];
    if (hud->retained_runtime_slice_overlay_enabled &&
        hud->backend_secondary_debug_slice_stack_live &&
        hud->backend_compatibility_view_2d_derived &&
        hud->backend_domain_d > 1) {
        snprintf(debug_cue_line,
                 sizeof(debug_cue_line),
                 "3D cue: derived XY ghost slice stack +/- %d + true XYZ volume point cloud",
                 hud->backend_secondary_debug_slice_stack_radius);
    } else {
        debug_cue_line[0] = '\0';
    }

    char preset_line[128];
    snprintf(preset_line, sizeof(preset_line), "Preset: %s",
             (hud->preset_name && hud->preset_name[0]) ? hud->preset_name : "Preset");

    char wind_line[64];
    if (hud->sim_mode == SIM_MODE_WIND_TUNNEL) {
        snprintf(wind_line, sizeof(wind_line), "Inflow %.1f", hud->tunnel_inflow_speed);
    } else {
        wind_line[0] = '\0';
    }

    char quality_line[64];
    snprintf(quality_line, sizeof(quality_line), "Quality: %s",
             (hud->quality_name && hud->quality_name[0]) ? hud->quality_name : "Custom");

    char solver_line[64];
    snprintf(solver_line, sizeof(solver_line),
             "Solver: iter %d, substeps %d",
             hud->solver_iterations,
             hud->physics_substeps);

    char overlays_line[160] = {0};
    if (hud->vorticity_enabled) append_token(overlays_line, sizeof(overlays_line), "Vort(V)");
    if (hud->pressure_enabled) append_token(overlays_line, sizeof(overlays_line), "Press(B)");
    if (hud->velocity_overlay_enabled) append_token(overlays_line, sizeof(overlays_line), "Vel(S)");
    if (hud->particle_overlay_enabled) append_token(overlays_line, sizeof(overlays_line), "Flow(L)");

    char velocity_mode_line[72];
    if (hud->velocity_overlay_enabled) {
        snprintf(velocity_mode_line, sizeof(velocity_mode_line),
                 "Vel mode: %s (Shift+S)",
                 hud->velocity_fixed_length ? "Fixed" : "Magnitude");
    } else {
        velocity_mode_line[0] = '\0';
    }

    char path_warn_line[160] = {0};
    if (!hud->kit_viz_density_enabled || !hud->kit_viz_density_active) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_density_enabled ? "Dens:fb(K)" : "Dens:legacy(K)");
    }
    if (hud->velocity_overlay_enabled &&
        (!hud->kit_viz_velocity_enabled || !hud->kit_viz_velocity_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_velocity_enabled ? "Vel:fb(J)" : "Vel:legacy(J)");
    }
    if (hud->pressure_enabled &&
        (!hud->kit_viz_pressure_enabled || !hud->kit_viz_pressure_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_pressure_enabled ? "P:fb(S+B)" : "P:legacy(S+B)");
    }
    if (hud->vorticity_enabled &&
        (!hud->kit_viz_vorticity_enabled || !hud->kit_viz_vorticity_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_vorticity_enabled ? "V:fb(S+V)" : "V:legacy(S+V)");
    }
    if (hud->particle_overlay_enabled &&
        (!hud->kit_viz_particles_enabled || !hud->kit_viz_particles_active)) {
        append_token(path_warn_line, sizeof(path_warn_line),
                     hud->kit_viz_particles_enabled ? "L:fb(S+L)" : "L:legacy(S+L)");
    }

    char gravity_line[48];
    if (!hud->objects_gravity_enabled) {
        snprintf(gravity_line, sizeof(gravity_line), "Obj gravity: Off (G)");
    } else {
        gravity_line[0] = '\0';
    }

    char retained_runtime_line[128];
    char retained_runtime_mode_line[160];
    char retained_runtime_slice_line[96];
    if (hud->retained_runtime_visual_active) {
        snprintf(retained_runtime_line,
                 sizeof(retained_runtime_line),
                 "Retained 3D view: Alt+LMB orbit  MMB pan  Wheel zoom  [ ] slice  T overlay  F frame");
        if (hud->backend_compatibility_view_2d_derived && hud->backend_domain_d > 1) {
            snprintf(retained_runtime_slice_line,
                     sizeof(retained_runtime_slice_line),
                     "Slice overlay: %s (T)",
                     hud->retained_runtime_slice_overlay_enabled ? "On" : "Off");
            if (hud->retained_runtime_slice_overlay_enabled) {
                snprintf(retained_runtime_mode_line,
                         sizeof(retained_runtime_mode_line),
                         "Runtime fluid: derived XY slice [ ] + ghost stack; true XYZ volume, emitters/obstacles, and first-pass solver live");
            } else {
                snprintf(retained_runtime_mode_line,
                         sizeof(retained_runtime_mode_line),
                         "Runtime fluid: slice overlay hidden; true XYZ volume, emitters/obstacles, and first-pass solver live");
            }
        } else {
            retained_runtime_slice_line[0] = '\0';
            snprintf(retained_runtime_mode_line,
                     sizeof(retained_runtime_mode_line),
                     "Runtime fluid: authoritative XY field");
        }
    } else {
        retained_runtime_line[0] = '\0';
        retained_runtime_mode_line[0] = '\0';
        retained_runtime_slice_line[0] = '\0';
    }

    const char *hint_line_a = "Keys: P/C/E Esc 1/2  V/B/S/L  Shift+S  T slice";
    const char *hint_line_b = "Paths: K/J  Shift+V/B/L  G grav  H elastic";

    enum { MAX_HUD_LINES = 32 };
    const char *lines[MAX_HUD_LINES];
    const char *draw_lines[MAX_HUD_LINES];
    size_t line_count = 0;
    lines[line_count++] = status_line;
    lines[line_count++] = frame_line;
    lines[line_count++] = space_line;
    lines[line_count++] = backend_line;
    lines[line_count++] = backend_kind_line;
    lines[line_count++] = domain_line;
    if (resolution_line[0]) lines[line_count++] = resolution_line;
    if (depth_policy_line[0]) lines[line_count++] = depth_policy_line;
    if (solver_contract_line[0]) lines[line_count++] = solver_contract_line;
    if (domain_extent_line[0]) lines[line_count++] = domain_extent_line;
    if (compatibility_line[0]) lines[line_count++] = compatibility_line;
    if (backend_status_line[0]) lines[line_count++] = backend_status_line;
    if (emitter_step_line[0]) lines[line_count++] = emitter_step_line;
    if (compatibility_activity_line[0]) lines[line_count++] = compatibility_activity_line;
    if (scene_axis_line[0]) lines[line_count++] = scene_axis_line;
    if (volume_truth_line[0]) lines[line_count++] = volume_truth_line;
    if (atmosphere_line[0]) lines[line_count++] = atmosphere_line;
    if (debug_cue_line[0]) lines[line_count++] = debug_cue_line;
    lines[line_count++] = preset_line;
    if (wind_line[0]) lines[line_count++] = wind_line;
    lines[line_count++] = quality_line;
    if (hud->paused) lines[line_count++] = solver_line;
    if (overlays_line[0]) {
        static char overlays_buf[176];
        snprintf(overlays_buf, sizeof(overlays_buf), "Overlays:%s", overlays_line);
        lines[line_count++] = overlays_buf;
    }
    if (velocity_mode_line[0]) lines[line_count++] = velocity_mode_line;
    if (path_warn_line[0]) {
        static char path_warn_buf[176];
        snprintf(path_warn_buf, sizeof(path_warn_buf), "Path alerts:%s", path_warn_line);
        lines[line_count++] = path_warn_buf;
    }
    if (gravity_line[0]) lines[line_count++] = gravity_line;
    if (retained_runtime_line[0]) lines[line_count++] = retained_runtime_line;
    if (retained_runtime_slice_line[0]) lines[line_count++] = retained_runtime_slice_line;
    if (retained_runtime_mode_line[0]) lines[line_count++] = retained_runtime_mode_line;
    if (!hud->retained_runtime_visual_active) {
        lines[line_count++] = hint_line_a;
        if (hud->paused || path_warn_line[0] || overlays_line[0]) {
            lines[line_count++] = hint_line_b;
        }
    }

    int widths[MAX_HUD_LINES];
    int heights[MAX_HUD_LINES];
    for (int i = 0; i < MAX_HUD_LINES; ++i) {
        widths[i] = 0;
        heights[i] = 0;
    }
    int count = 0;
    int max_w = 0;
    int total_h = 0;

    for (size_t i = 0; i < line_count; ++i) {
        int w = 0, h = 0;
        measure_hud_text_line(lines[i], &w, &h);
        if (w <= 0 || h <= 0) continue;
        draw_lines[count] = lines[i];
        widths[count] = w;
        heights[count] = h;
        if (w > max_w) max_w = w;
        total_h += h;
        ++count;
    }

    if (count == 0) {
        return;
    }

    const int padding = 6;
    const int spacing = 1;
    total_h += spacing * (count - 1);

    SDL_Rect panel = {
        .x = 12,
        .y = 12,
        .w = max_w + padding * 2,
        .h = total_h + padding * 2
    };

    SDL_SetRenderDrawColor(g_hud_renderer, 20, 24, 28, 12);
    SDL_RenderFillRect(g_hud_renderer, &panel);
    SDL_SetRenderDrawColor(g_hud_renderer, 255, 255, 255, 60);
    SDL_RenderDrawRect(g_hud_renderer, &panel);

    int y = panel.y + padding;
    for (int i = 0; i < count; ++i) {
        SDL_Rect dst = {panel.x + padding, y, widths[i], heights[i]};
        (void)physics_sim_text_draw_utf8(g_hud_renderer,
                                         g_hud_font,
                                         draw_lines[i],
                                         (SDL_Color){240, 240, 240, 255},
                                         &dst);
        y += heights[i] + spacing;
    }
}
