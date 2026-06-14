#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "sim_runtime_backend_3d_test_support.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
}

static bool closef(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static bool test_wind_tunnel_boundary_writes_inlet_and_receive_outlet(void) {
    AppConfig cfg = app_config_default();
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_WIND_TUNNEL,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .wind_tunnel_3d_active = true,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SceneState scene = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};
    SimRuntimeBackendReport report = {0};
    size_t inlet_idx = 0;
    size_t outlet_idx = 0;
    int center_y = 0;
    int center_z = 0;

    cfg.grid_w = 16;
    cfg.grid_h = 8;
    cfg.grid_d = 8;
    cfg.tunnel_inflow_speed = 12.0f;
    cfg.tunnel_inflow_density = 0.75f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    visual.scene_domain.max = (CoreObjectVec3){4.0, 2.0, 2.0};
    visual.wind_tunnel_authored = true;
    visual.wind_tunnel = wind_tunnel_3d_config_default(&cfg);
    visual.wind_tunnel.inlet_slab_cells = 2;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    scene.mode_route = route;
    scene.config = &cfg;
    scene.backend = backend;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 60.0);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &view)) return false;

    center_y = view.volume.desc.grid_h / 2;
    center_z = view.volume.desc.grid_d / 2;
    inlet_idx = sim_runtime_3d_volume_index(&view.volume.desc, 0, center_y, center_z);
    outlet_idx = sim_runtime_3d_volume_index(&view.volume.desc,
                                             view.volume.desc.grid_w - 1,
                                             center_y,
                                             center_z);

    if (!closef(view.volume.density[inlet_idx], 0.75f)) return false;
    if (!closef(view.volume.velocity_x[inlet_idx], 12.0f)) return false;
    if (!closef(view.volume.velocity_y[inlet_idx], 0.0f)) return false;
    if (!closef(view.volume.velocity_z[inlet_idx], 0.0f)) return false;
    if (!closef(view.volume.density[outlet_idx], 0.0f)) return false;
    if (!closef(view.volume.velocity_x[outlet_idx], 12.0f)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.wind_analysis_available) return false;
    if (report.wind_analysis_sampled_cells == 0u) return false;
    if (!closef(report.wind_analysis_pressure_delta, 0.0f)) return false;
    if (report.wind_analysis_inlet_throughput <= 0.0f) return false;
    if (!closef(report.wind_analysis_outlet_throughput, 0.0f)) return false;
    if (!closef(report.wind_analysis_drag_pressure_proxy, 0.0f)) return false;
    if (!closef(report.wind_analysis_vorticity_avg, 0.0f)) return false;
    if (!closef(report.wind_analysis_vorticity_max, 0.0f)) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_wind_tunnel_clear_outlet_zeros_receive_velocity(void) {
    AppConfig cfg = app_config_default();
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_WIND_TUNNEL,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .wind_tunnel_3d_active = true,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SceneState scene = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};
    size_t outlet_idx = 0;

    cfg.grid_w = 12;
    cfg.grid_h = 8;
    cfg.grid_d = 8;
    cfg.tunnel_inflow_speed = 9.0f;
    cfg.tunnel_inflow_density = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    visual.scene_domain.max = (CoreObjectVec3){3.0, 2.0, 2.0};
    visual.wind_tunnel_authored = true;
    visual.wind_tunnel = wind_tunnel_3d_config_default(&cfg);
    visual.wind_tunnel.outlet_policy = WIND_TUNNEL_3D_OUTLET_CLEAR;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    scene.mode_route = route;
    scene.config = &cfg;
    scene.backend = backend;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 60.0);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &view)) return false;
    outlet_idx = sim_runtime_3d_volume_index(&view.volume.desc,
                                             view.volume.desc.grid_w - 1,
                                             view.volume.desc.grid_h / 2,
                                             view.volume.desc.grid_d / 2);
    if (!closef(view.volume.density[outlet_idx], 0.0f)) return false;
    if (!closef(view.volume.velocity_x[outlet_idx], 0.0f)) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_wind_tunnel_corridor_solve_reaches_outlet(void) {
    AppConfig cfg = app_config_default();
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_WIND_TUNNEL,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .wind_tunnel_3d_active = true,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SceneState scene = {0};
    SimRuntimeBackendReport report = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};

    cfg.grid_w = 16;
    cfg.grid_h = 8;
    cfg.grid_d = 8;
    cfg.tunnel_inflow_speed = 12.0f;
    cfg.tunnel_inflow_density = 0.75f;
    cfg.fluid_3d_solver_region_cell_budget = 16 * 8 * 8;
    cfg.fluid_3d_max_velocity_displacement_cells = 4.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    visual.scene_domain.max = (CoreObjectVec3){2.0, 1.0, 1.0};
    visual.wind_tunnel_authored = true;
    visual.wind_tunnel = wind_tunnel_3d_config_default(&cfg);
    visual.wind_tunnel.inlet_slab_cells = 2;
    visual.wind_tunnel.outlet_policy = WIND_TUNNEL_3D_OUTLET_RECEIVE;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    scene.mode_route = route;
    scene.config = &cfg;
    scene.backend = backend;
    scene.runtime_visual = visual;

    for (int i = 0; i < 40; ++i) {
        sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 30.0);
        sim_runtime_backend_step(backend, &scene, &cfg, 1.0 / 30.0);
    }
    sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 30.0);

    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.wind_analysis_available) return false;
    if (!(report.wind_analysis_outlet_throughput > 0.0f) ||
        !(report.wind_analysis_vorticity_max > 0.0f) ||
        report.runtime_solver_skipped_cluster_count != 0u ||
        report.runtime_solver_solved_cluster_count == 0u) {
        int max_density_x = -1;
        float max_density = 0.0f;
        float max_vx = 0.0f;
        float outlet_density = 0.0f;
        (void)sim_runtime_backend_3d_test_view_refresh(backend, &view);
        for (int z = 0; z < view.volume.desc.grid_d; ++z) {
            for (int y = 0; y < view.volume.desc.grid_h; ++y) {
                for (int x = 0; x < view.volume.desc.grid_w; ++x) {
                    size_t idx = sim_runtime_3d_volume_index(&view.volume.desc, x, y, z);
                    float density = view.volume.density[idx];
                    if (density > 0.001f && x > max_density_x) max_density_x = x;
                    if (density > max_density) max_density = density;
                    if (view.volume.velocity_x[idx] > max_vx) max_vx = view.volume.velocity_x[idx];
                    if (x == view.volume.desc.grid_w - 1) outlet_density += density;
                }
            }
        }
        fprintf(stderr,
                "corridor report: inlet=%g outlet=%g delta=%g vort_max=%g "
                "solved=%zu skipped=%zu solver_cells=%zu active_cells=%zu "
                "max_density_x=%d max_density=%g max_vx=%g outlet_density=%g "
                "max_disp_pre=%g max_disp_post=%g clamp=%zu\n",
                report.wind_analysis_inlet_throughput,
                report.wind_analysis_outlet_throughput,
                report.wind_analysis_throughput_delta,
                report.wind_analysis_vorticity_max,
                report.runtime_solver_solved_cluster_count,
                report.runtime_solver_skipped_cluster_count,
                report.runtime_solver_region_cell_count,
                report.runtime_active_region_cell_count,
                max_density_x,
                max_density,
                max_vx,
                outlet_density,
                report.runtime_solver_max_velocity_displacement_cells_pre_clamp,
                report.runtime_solver_max_velocity_displacement_cells_post_clamp,
                report.runtime_solver_velocity_clamp_cell_count);
        return false;
    }
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &view)) return false;
    {
        const int probe_x = 10;
        const int probe_y = view.volume.desc.grid_h / 2;
        const int probe_z = view.volume.desc.grid_d / 2;
        const size_t probe_idx = sim_runtime_3d_volume_index(&view.volume.desc, probe_x, probe_y, probe_z);
        if (!(view.volume.density[probe_idx] > 0.001f)) {
            fprintf(stderr,
                    "corridor density transport failed: probe=(%d,%d,%d) density=%g\n",
                    probe_x,
                    probe_y,
                    probe_z,
                    view.volume.density[probe_idx]);
            sim_runtime_backend_destroy(backend);
            return false;
        }
    }

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool run_wind_tunnel_obstacle_probe(const FluidScenePreset *preset,
                                           SimRuntimeBackendReport *out_report,
                                           size_t *out_solid_cells) {
    AppConfig cfg = app_config_default();
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_WIND_TUNNEL,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .wind_tunnel_3d_active = true,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SceneState scene = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};
    size_t solid_cells = 0u;

    cfg.grid_w = 24;
    cfg.grid_h = 12;
    cfg.grid_d = 12;
    cfg.tunnel_inflow_speed = 12.0f;
    cfg.tunnel_inflow_density = 0.75f;
    cfg.fluid_3d_solver_region_cell_budget = 24 * 12 * 12;
    cfg.fluid_3d_max_velocity_displacement_cells = 4.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    visual.scene_domain.max = (CoreObjectVec3){2.4, 1.2, 1.2};
    visual.wind_tunnel_authored = true;
    visual.wind_tunnel = wind_tunnel_3d_config_default(&cfg);
    visual.wind_tunnel.inlet_slab_cells = 2;
    visual.wind_tunnel.outlet_policy = WIND_TUNNEL_3D_OUTLET_RECEIVE;

    backend = sim_runtime_backend_create(&cfg, preset, &route, &visual);
    if (!backend) return false;

    scene.mode_route = route;
    scene.config = &cfg;
    scene.backend = backend;
    scene.preset = preset;
    scene.runtime_visual = visual;
    sim_runtime_backend_build_obstacles(backend, &scene);

    for (int i = 0; i < 60; ++i) {
        sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 30.0);
        sim_runtime_backend_step(backend, &scene, &cfg, 1.0 / 30.0);
    }
    sim_runtime_backend_apply_boundary_flows(backend, &scene, 1.0 / 30.0);

    if (!sim_runtime_backend_get_report(backend, out_report)) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &view)) {
        sim_runtime_backend_destroy(backend);
        return false;
    }
    if (view.obstacle_occupancy) {
        for (size_t i = 0u; i < view.volume.desc.cell_count; ++i) {
            if (view.obstacle_occupancy[i]) solid_cells++;
        }
    }
    if (out_solid_cells) *out_solid_cells = solid_cells;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_wind_tunnel_static_object_creates_wake_response(void) {
    FluidScenePreset empty_preset = {0};
    FluidScenePreset obstacle_preset = {0};
    SimRuntimeBackendReport empty_report = {0};
    SimRuntimeBackendReport obstacle_report = {0};
    size_t empty_solid_cells = 0u;
    size_t obstacle_solid_cells = 0u;

    empty_preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    empty_preset.domain = SCENE_DOMAIN_WIND_TUNNEL;

    obstacle_preset = empty_preset;
    obstacle_preset.object_count = 1u;
    obstacle_preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.43f,
        .position_y = 0.50f,
        .position_z = 0.60f,
        .size_x = 0.09f,
        .size_y = 0.16f,
        .size_z = 0.16f,
        .is_static = true,
        .gravity_enabled = false,
    };

    if (!run_wind_tunnel_obstacle_probe(&empty_preset, &empty_report, &empty_solid_cells)) return false;
    if (!run_wind_tunnel_obstacle_probe(&obstacle_preset, &obstacle_report, &obstacle_solid_cells)) {
        return false;
    }
    if (obstacle_solid_cells <= empty_solid_cells ||
        !empty_report.wind_analysis_available || !obstacle_report.wind_analysis_available ||
        !(obstacle_report.wind_analysis_outlet_throughput > 0.0f) ||
        !(obstacle_report.runtime_solver_solved_cluster_count > 0u) ||
        obstacle_report.runtime_solver_skipped_cluster_count != 0u ||
        !obstacle_report.wind_analysis_object_drag_available ||
        obstacle_report.wind_analysis_object_solid_cells == 0u ||
        !(obstacle_report.wind_analysis_object_projected_area > 0.0f)) {
        fprintf(stderr,
                "wake setup report: empty_solid=%zu obstacle_solid=%zu "
                "empty_available=%d obstacle_available=%d obstacle_outlet=%g "
                "solved=%zu skipped=%zu solver_cells=%zu "
                "object_drag_available=%d object_solid=%zu object_area=%g\n",
                empty_solid_cells,
                obstacle_solid_cells,
                empty_report.wind_analysis_available ? 1 : 0,
                obstacle_report.wind_analysis_available ? 1 : 0,
                obstacle_report.wind_analysis_outlet_throughput,
                obstacle_report.runtime_solver_solved_cluster_count,
                obstacle_report.runtime_solver_skipped_cluster_count,
                obstacle_report.runtime_solver_region_cell_count,
                obstacle_report.wind_analysis_object_drag_available ? 1 : 0,
                obstacle_report.wind_analysis_object_solid_cells,
                obstacle_report.wind_analysis_object_projected_area);
        return false;
    }
    if (!(fabsf(obstacle_report.wind_analysis_object_drag_pressure_proxy) > 0.0001f)) {
        fprintf(stderr,
                "object drag report: upstream=%g downstream=%g delta=%g drag=%g area=%g solid=%zu\n",
                obstacle_report.wind_analysis_object_upstream_pressure_avg,
                obstacle_report.wind_analysis_object_downstream_pressure_avg,
                obstacle_report.wind_analysis_object_pressure_delta,
                obstacle_report.wind_analysis_object_drag_pressure_proxy,
                obstacle_report.wind_analysis_object_projected_area,
                obstacle_report.wind_analysis_object_solid_cells);
        return false;
    }
    if (!(obstacle_report.wind_analysis_object_pressure_delta > 0.0001f)) {
        fprintf(stderr,
                "object pressure sign report: upstream=%g downstream=%g delta=%g drag=%g area=%g solid=%zu\n",
                obstacle_report.wind_analysis_object_upstream_pressure_avg,
                obstacle_report.wind_analysis_object_downstream_pressure_avg,
                obstacle_report.wind_analysis_object_pressure_delta,
                obstacle_report.wind_analysis_object_drag_pressure_proxy,
                obstacle_report.wind_analysis_object_projected_area,
                obstacle_report.wind_analysis_object_solid_cells);
        return false;
    }
    if (!(obstacle_report.wind_analysis_vorticity_avg >
          empty_report.wind_analysis_vorticity_avg + 0.001f)) {
        fprintf(stderr,
                "wake report: empty_vort_avg=%g obstacle_vort_avg=%g "
                "empty_vort_max=%g obstacle_vort_max=%g "
                "empty_outlet=%g obstacle_outlet=%g solid=%zu\n",
                empty_report.wind_analysis_vorticity_avg,
                obstacle_report.wind_analysis_vorticity_avg,
                empty_report.wind_analysis_vorticity_max,
                obstacle_report.wind_analysis_vorticity_max,
                empty_report.wind_analysis_outlet_throughput,
                obstacle_report.wind_analysis_outlet_throughput,
                obstacle_solid_cells);
        return false;
    }
    return true;
}

int main(void) {
    if (!test_wind_tunnel_boundary_writes_inlet_and_receive_outlet()) {
        fprintf(stderr, "sim_runtime_backend_3d_wind_tunnel_contract_test: inlet/outlet write failed\n");
        return 1;
    }
    if (!test_wind_tunnel_clear_outlet_zeros_receive_velocity()) {
        fprintf(stderr, "sim_runtime_backend_3d_wind_tunnel_contract_test: clear outlet failed\n");
        return 1;
    }
    if (!test_wind_tunnel_corridor_solve_reaches_outlet()) {
        fprintf(stderr, "sim_runtime_backend_3d_wind_tunnel_contract_test: corridor solve failed\n");
        return 1;
    }
    if (!test_wind_tunnel_static_object_creates_wake_response()) {
        fprintf(stderr, "sim_runtime_backend_3d_wind_tunnel_contract_test: static object wake failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_wind_tunnel_contract_test: success\n");
    return 0;
}
