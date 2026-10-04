#define _DARWIN_C_SOURCE 1
// Trusted-local session owner. Only this process mutates the live scene.
#include "app/app_config.h"
#include "app/cfd_channel_observation.h"
#include "app/cfd_refined_session.h"
#include "app/cfd_3d_session.h"
#include "app/cfd_mac2d_observation.h"
#include "app/cfd_mac2d_force_check.h"
#include "app/cfd_open2d_observation.h"
#include "app/cfd_steady_monitor.h"
#include "app/cfd_open2d_budget.h"
#include <sys/stat.h>
#include "app/scene_core_sim_runtime_step.h"
#include "app/scene_runtime_launch_projection.h"
#include "app/scene_state.h"
#include "app/session_observation.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/sim_runtime_mesh_diagnostics.h"
#include "core_scene_compile.h"
#include "export/export_paths.h"
#include "export/volume_frames.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted;
static void stop_signal(int sig) {
    (void)sig;
    interrupted = 1;
}
static double monotonic_seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static struct json_object *member(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, key, &v);
    return v;
}
static const char *string(struct json_object *o, const char *key) {
    const char *s = json_object_get_string(member(o, key));
    return s ? s : "";
}
static void str(struct json_object *o, const char *k, const char *v) {
    json_object_object_add(o, k, json_object_new_string(v));
}
static void num(struct json_object *o, const char *k, double v) {
    json_object_object_add(o, k, isfinite(v) ? json_object_new_double(v) : NULL);
}
static void integer(struct json_object *o, const char *k, int64_t v) {
    json_object_object_add(o, k, json_object_new_int64(v));
}
static bool atomic_json(const char *path, struct json_object *o) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());
    if (json_object_to_file_ext(tmp, o, JSON_C_TO_STRING_PLAIN) != 0)
        return false;
    if (rename(tmp, path) == 0)
        return true;
    unlink(tmp);
    return false;
}

typedef struct Session {
    const char *root;
    struct json_object *request;
    SceneState scene;
    bool channel_live, mac_live, open_live, refined_live, cartesian_live;
    Cfd3dSession cartesian;
    CfdRefinedSession refined;
    CfdOpen2D open;
    CfdSteadyMonitor steady;
    CfdOpen2DEnergy energy;
    CfdMac2DForceCheck open_previous_cv;
    bool open_previous_cv_valid;
    bool energy_rate_valid;
    double energy_rate;
    bool mac_cv_rate_valid;
    double mac_cv_momentum_rate;
    CfdMac2D mac;
    CfdChannel channel;
    uint64_t tick, sequence, command_sequence;
    int limit;
    double dt, step_ms, publication_ms, final_export_ms;
    const char *state;
    const char *error;
    bool paused, solve_in_progress, cancel_fields_exported;
    uint64_t pending_cancel_sequence;
    double checkpoint_poll_time, checkpoint_service_ms;
    uint64_t checkpoint_service_count;
    struct json_object *pending_cancel_command;
    struct json_object *history;
    struct json_object *geometry;
    uint64_t history_tick;
} Session;

static bool mac_control_volume(const CfdMac2D *c, CfdMac2DForceCheck *out) {
    if(!c->solid) return false;
    int x0=c->nx,y0=c->ny,x1=0,y1=0;
    for(int j=0;j<c->ny;j++)for(int i=0;i<c->nx;i++)if(c->solid[j*c->nx+i]) {
        if(i<x0)x0=i;if(j<y0)y0=j;if(i+1>x1)x1=i+1;if(j+1>y1)y1=j+1;
    }
    return cfd_mac2d_force_check(c,x0-2,y0-2,x1+2,y1+2,out);
}
static void mac_force_inspection(Session *s, struct json_object *out) {
    if(!s->mac.solid) return;
    struct json_object *budget=member(out,"boundary_force_budget"),*cv=json_object_new_object();
    CfdMac2DForceCheck f;
    bool valid=mac_control_volume(&s->mac,&f);
    json_object_object_add(cv,"available",json_object_new_boolean(valid&&s->mac_cv_rate_valid));
    str(cv,"scope","Control surface two fluid cells away; instantaneous boundary quadrature and one-tick momentum difference. Sensitivity diagnostic, not qualified force acceptance.");
    if(valid) {
        num(cv,"surface_pressure_force_x_n",f.surface_pressure_x_n);
        num(cv,"surface_viscous_force_x_n",f.surface_viscous_x_n);
        num(cv,"normal_strain_artifact_x_n",f.reconstructed_normal_viscous_x_n);
        num(cv,"boundary_pressure_force_x_n",f.cv_pressure_x_n);
        num(cv,"boundary_viscous_force_x_n",f.cv_viscous_x_n);
        num(cv,"outward_advective_momentum_x_n",f.cv_advective_x_n);
        num(cv,"body_drive_x_n",f.cv_drive_x_n);
        if(s->mac_cv_rate_valid) {
            double force=f.cv_pressure_x_n+f.cv_viscous_x_n+f.cv_drive_x_n-f.cv_advective_x_n-s->mac_cv_momentum_rate;
            num(cv,"momentum_change_rate_x_n",s->mac_cv_momentum_rate);
            num(cv,"inferred_body_force_x_n",force);
            num(cv,"surface_minus_control_volume_x_n",f.surface_pressure_x_n+f.surface_viscous_x_n-force);
        }
    }
    json_object_object_add(budget,"control_volume_comparison",cv);
}
static struct json_object *snapshot(Session *s) {
    SimRuntimeBackendReport r = {0};
    if (!s->channel_live && !s->mac_live && !s->open_live && !s->refined_live && !s->cartesian_live) scene_backend_report(&s->scene, &r);
    struct json_object *o = json_object_new_object();
    str(o, "schema", "physics_sim_session_snapshot_v1");
    integer(o, "sample_protocol", 1);
    str(o, "json_library_version", json_c_version());
    str(o, "run_id", string(s->request, "run_id"));
    str(o, "scene_revision", string(s->request, "scene_revision"));
    str(o, "state", s->state);
    str(o, "error", s->error ? s->error : "");
    integer(o, "tick", s->tick);
    integer(o, "tick_limit", s->limit);
    integer(o, "sequence", ++s->sequence);
    integer(o, "command_sequence", s->command_sequence);
    num(o, "simulation_time", s->scene.time);
    num(o, "dt", s->dt);
    num(o, "step_ms", s->step_ms);
    json_object_object_add(o, "solve_in_progress", json_object_new_boolean(s->solve_in_progress));
    str(o, "inspection_state", "last accepted fields and time; candidate solve remains private");
    struct json_object *runtime_cost = json_object_new_object();
    num(runtime_cost, "previous_publication_wall_ms", s->publication_ms);
    num(runtime_cost, "final_export_wall_ms", s->final_export_ms);
    num(runtime_cost, "checkpoint_service_wall_ms", s->checkpoint_service_ms);
    integer(runtime_cost, "checkpoint_service_count", s->checkpoint_service_count);
    str(runtime_cost, "scope", "monotonic wall time; previous completed snapshot/preview publication "
        "including serialization and write, final field export separately; zero before measurement");
    json_object_object_add(o, "runtime_cost", runtime_cost);
    num(o, "updated_at", (double)time(NULL));
    if (s->channel_live || s->mac_live || s->open_live || s->refined_live || s->cartesian_live) {
        if(s->cartesian_live) {
            cfd_3d_session_snapshot(&s->cartesian,o,false);
        } else if(s->refined_live) {
            cfd_refined_session_snapshot(&s->refined,o,false);
        } else if(s->open_live) {
            cfd_open2d_snapshot(&s->open,o,false,s->mac_cv_rate_valid,s->mac_cv_momentum_rate);
            struct json_object *energy=json_object_new_object();
            json_object_object_add(energy,"available",json_object_new_boolean(s->energy_rate_valid));
            str(energy,"method","fluid-cell physical strain quadrature; stationary body work zero; backward one-step energy rate");
            if(s->energy_rate_valid) {
                double power=s->energy.pressure_work_w+s->energy.viscous_work_w-s->energy.outward_kinetic_flux_w;
                num(energy,"pressure_work_w",s->energy.pressure_work_w);
                num(energy,"viscous_work_w",s->energy.viscous_work_w);
                num(energy,"outward_kinetic_flux_w",s->energy.outward_kinetic_flux_w);
                num(energy,"dissipation_w",s->energy.dissipation_w);
                num(energy,"kinetic_energy_rate_w",s->energy_rate);
                num(energy,"residual_w",power-s->energy.dissipation_w-s->energy_rate);
                if(fabs(power)>1e-30)num(energy,"relative_residual",fabs(power-s->energy.dissipation_w-s->energy_rate)/fabs(power));
            }
            json_object_object_add(o,"energy_budget",energy);
            struct json_object *steady=json_object_new_object();
            str(steady,"status",s->steady.invalid?"invalid":cfd_steady_passed(&s->steady)?"passed":"not_established");
            str(steady,"method","every-step whole-velocity-field range; two full 1 s windows starting after 5 s; relative span <= 1e-6 of inlet mean");
            num(steady,"last_window_relative_span",s->steady.last_span);
            num(steady,"active_window_relative_span",s->steady.active_span);
            integer(steady,"consecutive_passed_windows",s->steady.consecutive);
            integer(steady,"observed_windows",s->steady.windows);
            json_object_object_add(o,"steady_acceptance",steady);
            if(s->error) str(member(o,"health"),"projection_status","failed");
        } else if(s->mac_live) {
            cfd_mac2d_snapshot(&s->mac,o,false);
            mac_force_inspection(s,o);
            if(s->error) str(member(o,"health"),"projection_status","failed");
        } else cfd_channel_snapshot(&s->channel, o);
        json_object_object_add(o, "requested_grid", json_object_get(member(s->request,"grid")));
        if (!s->history) s->history=json_object_new_array();
        if (!json_object_array_length(s->history) || s->history_tick!=s->tick) {
            struct json_object *point=json_object_new_object();
            integer(point,"tick",s->tick);num(point,"simulation_time",s->scene.time);
            if(s->mac_live || s->open_live || s->refined_live || s->cartesian_live) {
                json_object_object_add(point,"health",json_object_get(member(o,"health")));
                if(s->open_live || s->refined_live || s->cartesian_live)json_object_object_add(point,"energy_budget",json_object_get(member(o,"energy_budget")));
                json_object_object_add(point,"boundary_force_budget",json_object_get(member(o,"boundary_force_budget")));
                if(s->cartesian_live)json_object_object_add(point,"qualification",json_object_get(member(o,"qualification")));
            }
            else {
            num(point,"volume_flux_m3_s",s->channel.volume_flux);
            num(point,"max_acceleration_m_s2",s->channel.max_acceleration);
            num(point,"momentum_balance_residual_n",s->channel.momentum_residual);
            num(point,"energy_balance_residual_w",s->channel.energy_residual);
            }
            json_object_array_add(s->history,point);s->history_tick=s->tick;
            if(json_object_array_length(s->history)>128)json_object_array_del_idx(s->history,0,1);
        }
        json_object_object_add(o,"history",json_object_get(s->history));
        json_object_object_add(o,"preview",s->cartesian_live?cfd_3d_session_sample(&s->cartesian,NULL):s->refined_live?cfd_refined_session_sample(&s->refined,NULL):s->open_live?cfd_open2d_sample(&s->open,NULL):s->mac_live?cfd_mac2d_sample(&s->mac,NULL):cfd_channel_sample(&s->channel,NULL));
        return o;
    }
    bool qualification = json_object_get_boolean(member(s->request, "qualification_mode"));
    str(o, "model", qualification ? "wind_numerical_qualification_v1" : "wind_approximate_v1");
    str(o, "model_limitations", qualification
        ? "Synthetic wake/carrier disabled; matched collocated projection; global boundaries remain unvalidated. No physical drag measurement."
        : "Injected wake; pressure and drag are proxies, not validated CFD.");
    struct json_object *physics = json_object_new_object();
    if (qualification) {
        str(physics, "boundary_model", "prescribed inlet velocity; zero-gradient velocity predictor at receive outlet, then free transpose projection; not calibrated pressure outlet");
        str(physics, "inlet_face", wind_tunnel_3d_face_label(r.wind_tunnel_inlet_face));
        str(physics, "outlet_face", wind_tunnel_3d_face_label(r.wind_tunnel_outlet_face));
        num(physics, "inlet_speed_m_s", r.wind_tunnel_inflow_speed);
    }
    struct json_object *fluid = member(s->request, "fluid");
    if (fluid) {
        double rho = json_object_get_double(member(fluid, "density_kg_m3"));
        double mu = json_object_get_double(member(fluid, "dynamic_viscosity_pa_s"));
        num(physics, "density_kg_m3", rho);
        num(physics, "dynamic_viscosity_pa_s", mu);
        num(physics, "kinematic_viscosity_m2_s", mu / rho);
        num(physics, "viscous_diffusion_number", mu / rho * s->dt / (r.voxel_size * r.voxel_size));
        str(physics, "viscosity_operator", "explicit Laplacian, alpha <= 1/6 per diffusion substep; cell-solid no-slip, outer zero-gradient");
        str(physics, "density_semantics", "constant carrier density used to derive nu; dye density is not kg/m3");
    } else str(physics, "viscosity_operator", "legacy smoothing, not SI viscosity");
    json_object_object_add(physics, "synthetic_wind_enabled", json_object_new_boolean(!qualification));
    integer(physics, "solver_iterations_requested", s->scene.config->fluid_solver_iterations);
    integer(physics, "solver_iterations_applied", sim_runtime_3d_solver_iterations_for_config(s->scene.config));
    json_object_object_add(physics, "drag_force_n", NULL);
    json_object_object_add(physics, "drag_coefficient", NULL);
    str(physics, "drag_status", "unavailable: no validated surface stress integration");
    json_object_object_add(o, "physics", physics);
    json_object_object_add(o, "geometry", json_object_get(s->geometry));
    struct json_object *grid = json_object_new_array();
    json_object_array_add(grid, json_object_new_int(r.domain_w));
    json_object_array_add(grid, json_object_new_int(r.domain_h));
    json_object_array_add(grid, json_object_new_int(r.domain_d));
    json_object_object_add(o, "effective_grid", grid);
    json_object_object_add(o, "requested_grid", json_object_get(member(s->request, "grid")));
    integer(o, "solver_cell_budget", r.runtime_solver_region_cell_budget);
    num(o, "velocity_displacement_limit_cells",
        r.runtime_solver_max_velocity_displacement_cells_limit);
    num(o, "voxel_size_m", r.voxel_size);
    integer(o, "estimated_dense_bytes", (int64_t)r.cell_count * 45);
    struct json_object *health = json_object_new_object();
    integer(health, "skipped_clusters", r.runtime_solver_skipped_cluster_count);
    integer(health, "solved_clusters", r.runtime_solver_solved_cluster_count);
    integer(health, "velocity_clamped_cells", r.runtime_solver_velocity_clamp_cell_count);
    integer(health, "export_materializations", r.runtime_export_cache_materialization_count);
    integer(health, "active_bricks", r.runtime_active_brick_count);
    num(health, "max_divergence", r.runtime_solver_max_abs_divergence_after_project);
    num(health, "max_speed", r.runtime_solver_max_velocity_magnitude_post_clamp);
    num(health, "inlet_throughput", r.wind_analysis_inlet_throughput);
    num(health, "outlet_throughput", r.wind_analysis_outlet_throughput);
    num(health, "divergence_before_projection_s_inv", r.runtime_solver_max_abs_divergence_before_project);
    num(health, "pressure_residual_linf_s_inv", r.runtime_solver_pressure_residual_linf);
    num(health, "advective_displacement_cells_pre_clamp", r.runtime_solver_max_velocity_displacement_cells_pre_clamp);
    integer(health, "solid_cells", r.debug_volume_solid_cells);
    if (qualification) {
        integer(health, "projection_iterations_used", r.runtime_projection_iterations_used);
        integer(health, "projection_unconverged_regions", r.runtime_projection_unconverged_count);
        str(health, "projection_status", r.runtime_solver_solved_cluster_count == 0 ? "not_solved"
            : (r.runtime_projection_unconverged_count ? "not_converged" : "converged"));
    }
    integer(health, "transport_corrected_components", r.runtime_transport_corrected_components);
    integer(health, "transport_limited_components", r.runtime_transport_limited_components);
    integer(health, "transport_fallback_components", r.runtime_transport_fallback_components);
    str(health, "velocity_transport", qualification
        ? "bounded_maccormack_visible_donors_v1" : "legacy_semi_lagrangian_v1");
    str(health, "transport_scope", "cell-centred velocity advection; not conservative momentum transport");
    str(health, "projection_operator", qualification
        ? "prescribed_inlet_transpose_cg_v2" : "legacy_jacobi_v1");
    str(health, "pressure_convergence", qualification
        ? "CG bounded by requested iterations, local tolerance max(1e-7, initial L_inf divergence * 1e-6); true residual reported; no global guarantee"
        : "fixed Jacobi iterations; reported L_inf residual of discrete Poisson equation, not a physical pressure error");
    str(health, "projection_scope", qualification
        ? "one full-domain solve; inlet velocity constrained, outlet free under transpose operator; not a calibrated pressure outlet"
        : "maxima over solved clusters before later boundary operations; not final global divergence");
    str(health, "nonfinite_scope", "bounded preview samples and reported metrics");
    struct json_object *balance = json_object_new_object();
    json_object_object_add(balance, "available", json_object_new_boolean(r.conservation.valid));
    str(balance, "scope", "final snapshot, averaged cell velocities at interior faces, zero solid-face flux, exterior cell velocity; not solver-owned MAC fluxes; max 262144 cells");
    if (r.conservation.valid) {
        struct json_object *faces = json_object_new_array();
        for (int f = 0; f < 6; ++f)
            json_object_array_add(faces, json_object_new_double(r.conservation.boundary_flux_m3_s[f]));
        json_object_object_add(balance, "outward_face_flux_m3_s_minmax_xyz", faces);
        num(balance, "net_outward_volume_flux_m3_s", r.conservation.net_outward_flux_m3_s);
        num(balance, "integrated_divergence_m3_s", r.conservation.integrated_divergence_m3_s);
        num(balance, "final_max_abs_divergence_s_inv", r.conservation.max_abs_divergence_s_inv);
        if (fluid) {
            double rho = json_object_get_double(member(fluid, "density_kg_m3"));
            num(balance, "net_outward_mass_flux_kg_s", rho*r.conservation.net_outward_flux_m3_s);
            num(balance, "kinetic_energy_j", rho*r.conservation.kinetic_energy_per_density);
        }
    }
    json_object_object_add(health, "conservation", balance);
    str(health, "throughput_semantics", "legacy inlet/outlet throughput is dye-weighted; use conservation for carrier volume and mass flux");
    json_object_object_add(o, "health", health);
    if (!s->history)
        s->history = json_object_new_array();
    if (!json_object_array_length(s->history) || s->history_tick != s->tick) {
        struct json_object *point = json_object_new_object();
        integer(point, "tick", s->tick);
        num(point, "simulation_time", s->scene.time);
        num(point, "step_ms", s->step_ms);
        num(point, "max_divergence", r.runtime_solver_max_abs_divergence_after_project);
        num(point, "pressure_residual_linf_s_inv", r.runtime_solver_pressure_residual_linf);
        num(point, "divergence_before_projection_s_inv", r.runtime_solver_max_abs_divergence_before_project);
        num(point, "max_speed", r.runtime_solver_max_velocity_magnitude_post_clamp);
        integer(point, "velocity_clamped_cells", r.runtime_solver_velocity_clamp_cell_count);
        json_object_array_add(s->history, point);
        s->history_tick = s->tick;
        if (json_object_array_length(s->history) > 128)
            json_object_array_del_idx(s->history, 0, 1);
    }
    json_object_object_add(o, "history", json_object_get(s->history));
    json_object_object_add(o, "preview", physics_sim_session_observation(&s->scene));
    return o;
}
static bool publish(Session *s, bool event) {
    double begin = monotonic_seconds();
    char path[1100];
    struct json_object *o = snapshot(s);
    snprintf(path, sizeof(path), "%s/snapshot.json", s->root);
    bool ok = atomic_json(path, o);
    if (event) {
        snprintf(path, sizeof(path), "%s/events.jsonl", s->root);
        FILE *f = fopen(path, "a");
        if (!f)
            ok = false;
        else {
            fprintf(f,
                    "{\"cursor\":%llu,\"tick\":%llu,\"state\":\"%s\",\"command_sequence\":%llu}\n",
                    (unsigned long long)s->sequence, (unsigned long long)s->tick, s->state,
                    (unsigned long long)s->command_sequence);
            if (fclose(f) != 0)
                ok = false;
        }
    }
    json_object_put(o);
    s->publication_ms = 1000 * (monotonic_seconds() - begin);
    return ok;
}

// Diagnostic requests are independent of control receipts and never advance time.
// Bounded service admission and at most two reads per boundary prevent starvation.
static void sample_requests(Session *s) {
    char directory[1100];
    snprintf(directory, sizeof(directory), "%s/sample_requests", s->root);
    DIR *dir = opendir(directory);
    if (!dir)
        return;
    struct dirent *entry;
    int handled = 0;
    while (handled < 2 && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || !strstr(entry->d_name, ".json"))
            continue;
        char input[1400], output[1400];
        snprintf(input, sizeof(input), "%s/%s", directory, entry->d_name);
        snprintf(output, sizeof(output), "%s/sample_results/%s", s->root, entry->d_name);
        struct json_object *req = json_object_from_file(input);
        if (!req)
            continue;
        struct json_object *o = json_object_new_object();
        str(o, "schema", "physics_sim_sample_v1");
        str(o, "request_id", string(req, "request_id"));
        str(o, "run_id", string(s->request, "run_id"));
        str(o, "scene_revision", string(s->request, "scene_revision"));
        str(o, "status", "ready");
        str(o, "state", s->state);
        integer(o, "tick", s->tick);
        num(o, "simulation_time", s->scene.time);
        num(o, "sampled_at", (double)time(NULL));
        json_object_object_add(o, "preview", s->cartesian_live ? cfd_3d_session_sample(&s->cartesian,req) : s->refined_live ? cfd_refined_session_sample(&s->refined,req) : s->open_live ? cfd_open2d_sample(&s->open,req) : s->mac_live ? cfd_mac2d_sample(&s->mac,req) : s->channel_live ? cfd_channel_sample(&s->channel,req) : physics_sim_session_sample(&s->scene, req));
        if (atomic_json(output, o))
            unlink(input);
        json_object_put(req);
        json_object_put(o);
        handled++;
    }
    closedir(dir);
}

// Only inspect the next ordered command here. Receipts/sequence advancement stay
// at the outer boundary, including a paused step interrupted by its next cancel.
static bool mixed_checkpoint(void *context) {
    Session *s = context;
    if (interrupted)
        return false;
    double now = monotonic_seconds();
    if (now - s->checkpoint_poll_time < .1)
        return true;
    s->checkpoint_poll_time = now;
    char path[1100];
    uint64_t next = s->command_sequence + 1;
    snprintf(path, sizeof(path), "%s/commands/%08llu.json", s->root,
             (unsigned long long)next);
    struct json_object *command = json_object_from_file(path);
    bool cancel = command && !strcmp(string(command, "action"), "cancel") &&
                  !strcmp(string(command, "scene_revision"), string(s->request, "scene_revision"));
    if (cancel) s->pending_cancel_command = json_object_get(command);
    json_object_put(command);
    if (cancel) {
        s->pending_cancel_sequence = next;
        return false;
    }
    double begin = monotonic_seconds();
    sample_requests(s);
    bool published = publish(s, false);
    s->checkpoint_service_ms += 1000 * (monotonic_seconds() - begin);
    s->checkpoint_service_count++;
    if (!published) {
        s->error = "snapshot_io_failed";
        return false;
    }
    return true;
}

static bool finish_checkpoint_cancel(Session *s) {
    if (!s->pending_cancel_sequence)
        return true;
    char path[1100];
    snprintf(path, sizeof(path), "%s/commands/%08llu.json", s->root,
             (unsigned long long)s->pending_cancel_sequence);
    struct json_object *command = json_object_from_file(path);
    bool valid = command && json_object_equal(command, s->pending_cancel_command) &&
                 s->pending_cancel_sequence == s->command_sequence + 1 &&
                 !strcmp(string(command, "action"), "cancel") &&
                 !strcmp(string(command, "scene_revision"), string(s->request, "scene_revision"));
    if (!valid) {
        json_object_put(command);
        s->error = "checkpoint_cancel_identity_changed";
        s->state = "failed";
        return false;
    }
    s->command_sequence++;
    struct json_object *receipt = json_object_new_object();
    str(receipt, "command_id", string(command, "command_id"));
    str(receipt, "run_id", string(s->request, "run_id"));
    str(receipt, "scene_revision", string(s->request, "scene_revision"));
    str(receipt, "action", "cancel");
    str(receipt, "state", s->state);
    str(receipt, "status", s->error ? "failed" : "applied");
    str(receipt, "error", s->error ? s->error : "");
    integer(receipt, "tick", s->tick);
    num(receipt, "simulation_time", s->scene.time);
    integer(receipt, "sequence", s->command_sequence);
    snprintf(path, sizeof(path), "%s/receipts/%08llu.json", s->root,
             (unsigned long long)s->command_sequence);
    bool ok = atomic_json(path, receipt);
    json_object_put(receipt);
    json_object_put(command);
    if (!ok) {
        s->state = "failed";
        s->error = "receipt_io_failed";
    }
    return ok;
}

// Finish accepted-field export before publishing a terminal cancellation or
// acknowledging its command. The field artifact exists before terminal readback.
static bool export_cancelled_fields(Session *s) {
    if (strcmp(s->state, "cancelled") || !s->cartesian_live ||
        (!s->cartesian.transient_kind && !s->cartesian.obstacle) || s->cancel_fields_exported)
        return true;
    double begin = monotonic_seconds();
    char path[1200];
    snprintf(path, sizeof(path), "%s/output", s->root);
    bool directory_ok = mkdir(path, 0700) == 0 || errno == EEXIST;
    snprintf(path, sizeof(path), "%s/output/channel_fields.json", s->root);
    struct json_object *fields = json_object_new_object();
    str(fields, "schema", "physics_sim_cartesian3d_fields_v1");
    num(fields, "time_s", s->scene.time);
    cfd_3d_session_snapshot(&s->cartesian, fields, true);
    bool ok = directory_ok && atomic_json(path, fields);
    json_object_put(fields);
    s->final_export_ms = 1000 * (monotonic_seconds() - begin);
    if (!ok) {
        s->state = "failed";
        s->error = "final_volume_export_failed";
    } else s->cancel_fields_exported = true;
    return ok;
}

static bool step(Session *s, AppConfig *cfg, const SimModeHooks *hooks) {
    double begin = monotonic_seconds();
    s->scene.dt = s->dt;
    CfdMac2DForceCheck cv_before,cv_after;
    CfdOpen2DEnergy energy_before=s->energy;
    bool energy_valid=s->open_live&&(s->energy_rate_valid||cfd_open2d_energy(&s->open,&energy_before));
    bool cv_valid;
    if(s->open_live&&s->open_previous_cv_valid){cv_before=s->open_previous_cv;cv_valid=true;}
    else cv_valid=s->open_live?cfd_open2d_body_check(&s->open,&cv_before):s->mac_live&&mac_control_volume(&s->mac,&cv_before);
    s->solve_in_progress = true;
    s->checkpoint_poll_time = 0;
    bool ok = s->cartesian_live ? cfd_3d_session_step(&s->cartesian) : s->refined_live ? cfd_refined_session_step(&s->refined) : s->open_live ? cfd_open2d_step(&s->open,s->dt) : s->mac_live ? cfd_mac2d_step(&s->mac,s->dt) : s->channel_live ? cfd_channel_step(&s->channel,s->dt)
        : physics_sim_scene_core_sim_step(&s->scene, cfg, hooks, s->dt, NULL);
    s->solve_in_progress = false;
    if(s->cartesian_live) s->scene.time=s->cartesian.time;
    if(s->refined_live) s->scene.time=s->refined.channel.time;
    if(s->channel_live) s->scene.time=s->channel.time;
    if(s->open_live) {
        s->scene.time=s->open.time;
        if(ok)cfd_steady_observe(&s->steady,&s->open);
        s->energy_rate_valid=ok&&energy_valid&&cfd_open2d_energy(&s->open,&s->energy);
        if(s->energy_rate_valid)s->energy_rate=(s->energy.kinetic_energy_j-energy_before.kinetic_energy_j)/s->dt;
        s->open_previous_cv_valid=ok&&cfd_open2d_body_check(&s->open,&cv_after);
        if(s->open_previous_cv_valid)s->open_previous_cv=cv_after;
        s->mac_cv_rate_valid=cv_valid&&s->open_previous_cv_valid;
        if(s->mac_cv_rate_valid)s->mac_cv_momentum_rate=(cv_after.cv_momentum_x_kg_m_s-cv_before.cv_momentum_x_kg_m_s)/s->dt;
    }
    if(s->mac_live) {
        s->scene.time=s->mac.time;
        s->mac_cv_rate_valid=ok&&cv_valid&&mac_control_volume(&s->mac,&cv_after);
        if(s->mac_cv_rate_valid)s->mac_cv_momentum_rate=(cv_after.cv_momentum_x_kg_m_s-cv_before.cv_momentum_x_kg_m_s)/s->dt;
    }
    s->step_ms = (monotonic_seconds() - begin) * 1000;
    if (ok)
        s->tick++;
    else if (s->cartesian_live && s->cartesian.cancelled && !s->error) {
        s->state = "cancelled";
    } else {
        s->state = "failed";
        if (!s->error) s->error = s->cartesian_live ? s->cartesian.error : s->refined_live ? s->refined.error : s->open_live ? "open_solver_failure_or_cfl_bound" : "solver_failure_or_region_budget";
    }
    SimRuntimeBackendReport r = {0};
    if (!s->channel_live && !s->mac_live && !s->open_live && !s->refined_live && !s->cartesian_live) scene_backend_report(&s->scene, &r);
    if (!isfinite(r.runtime_solver_max_abs_divergence_after_project) ||
        !isfinite(r.runtime_solver_max_velocity_magnitude_post_clamp)) {
        s->state = "failed";
        s->error = "nonfinite_solver_metrics";
        ok = false;
    }
    if (ok && s->tick >= (uint64_t)s->limit) {
        // Final full fields are an explicit result artifact, never a preview side effect.
        double export_begin = monotonic_seconds();
        bool exported=false;
        if(s->channel_live || s->mac_live || s->open_live || s->refined_live || s->cartesian_live) {
            char dir[1100],file[1200];snprintf(dir,sizeof(dir),"%s/output",s->root);
            if(mkdir(dir,0700)==0||errno==EEXIST) {
                snprintf(file,sizeof(file),"%s/channel_fields.json",dir);
                struct json_object *fields=json_object_new_object();
                str(fields,"schema",s->cartesian_live?"physics_sim_cartesian3d_fields_v1":s->refined_live?"physics_sim_refined2d_fields_v1":s->open_live?"physics_sim_open2d_fields_v1":s->mac_live?"physics_sim_mac2d_fields_v1":"physics_sim_channel_fields_v1");num(fields,"time_s",s->scene.time);
                if(s->cartesian_live)cfd_3d_session_snapshot(&s->cartesian,fields,true);else if(s->refined_live)cfd_refined_session_snapshot(&s->refined,fields,true);else if(s->open_live)cfd_open2d_snapshot(&s->open,fields,true,s->mac_cv_rate_valid,s->mac_cv_momentum_rate);else if(s->mac_live)cfd_mac2d_snapshot(&s->mac,fields,true);else cfd_channel_snapshot(&s->channel,fields);
                exported=atomic_json(file,fields);json_object_put(fields);
            }
        } else exported=volume_frames_write(&s->scene,s->tick);
        s->final_export_ms = 1000 * (monotonic_seconds() - export_begin);
        if (exported)
            s->state = "completed";
        else {
            s->state = "failed";
            s->error = "final_volume_export_failed";
            ok = false;
        }
    }
    if (!export_cancelled_fields(s)) ok = false;
    return ok;
}
static bool terminal(const Session *s) {
    return !strcmp(s->state, "failed") || !strcmp(s->state, "completed") ||
           !strcmp(s->state, "cancelled");
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "--compile")) {
        char diagnostics[512] = {0};
        CoreResult result = core_scene_compile_authoring_file_to_runtime_file(
            argv[2], argv[3], diagnostics, sizeof(diagnostics));
        if (result.code != CORE_OK)
            fprintf(stderr, "%s\n", diagnostics);
        return result.code == CORE_OK ? 0 : 2;
    }
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--validate"))) {
        fprintf(stderr, "usage: physics_sim_session_worker RUN_DIR [--validate]\n");
        return 2;
    }
    char path[1100];
    snprintf(path, sizeof(path), "%s/request.json", argv[1]);
    struct json_object *request = json_object_from_file(path);
    char owner_path[1100];
    snprintf(owner_path, sizeof(owner_path), "%s/owner.lock", argv[1]);
    const char *inherited_owner = getenv("PHYSICS_SIM_SESSION_OWNER_FD");
    int owner = inherited_owner ? atoi(inherited_owner) : open(owner_path, O_CREAT | O_RDWR, 0600);
    if (owner < 0 || flock(owner, LOCK_EX | LOCK_NB) != 0)
        return 2;
    snprintf(owner_path, sizeof(owner_path), "%s/ready", argv[1]);
    FILE *ready = fopen(owner_path, "w");
    if (!ready)
        return 2;
    fprintf(ready, "%ld\n", (long)getpid());
    fclose(ready);
    if (!request)
        return 2;
    Session s = {.root = argv[1], .request = request, .state = "starting"};
    s.limit = json_object_get_int(member(request, "steps"));
    s.dt = json_object_get_double(member(request, "dt"));
    if (s.limit < 1 || s.limit > 100000 || s.dt <= 0 || s.dt > 0.1 || !isfinite(s.dt))
        return 2;
    char output_root[1100];
    snprintf(output_root, sizeof(output_root), "%s/output", s.root);
    if (!export_paths_set_root(output_root))
        return 2;
    AppConfig cfg = app_config_default();
    SimModeRoute route={0};
    bool open_model=!strcmp(string(request,"model"),"incompressible_open2d_v1");
    bool mac_model=!strcmp(string(request,"model"),"incompressible_mac2d_v1");
    bool refined_model=!strcmp(string(request,"model"),"incompressible_refined2d_v1");
    bool cartesian_model=!strcmp(string(request,"model"),"incompressible_cartesian3d_v1");
    if (cartesian_model) {
        struct json_object *ch=member(request,"channel");
        snprintf(path,sizeof(path),"%s/scene_runtime.json",s.root);
        struct json_object *doc=json_object_from_file(path);
        struct json_object *authored=member(member(member(doc,"extensions"),"physics_sim"),"channel_flow");
        bool same=authored&&json_object_equal(authored,ch)&&json_object_array_length(member(doc,"objects"))==0&&
                  !strcmp(string(ch,"solver_model"),"incompressible_cartesian3d_v1");
        json_object_put(doc);s.cartesian_live=true;
        if(!same||!cfd_3d_session_init(&s.cartesian,request)) {
            s.state="failed";s.error=same?s.cartesian.error:"cartesian3d_scene_request_mismatch";
            publish(&s,true);cfd_3d_session_destroy(&s.cartesian);json_object_put(request);return 2;
        }
        cfd_3d_session_checkpoint(&s.cartesian, mixed_checkpoint, &s);
        s.geometry=json_object_new_array();
    } else if (refined_model) {
        struct json_object *ch=member(request,"channel");
        snprintf(path,sizeof(path),"%s/scene_runtime.json",s.root);
        struct json_object *doc=json_object_from_file(path);
        struct json_object *authored=member(member(member(doc,"extensions"),"physics_sim"),"channel_flow");
        bool same=authored&&json_object_equal(authored,ch)&&json_object_array_length(member(doc,"objects"))==0&&
                  !strcmp(string(ch,"solver_model"),"incompressible_refined2d_v1");
        json_object_put(doc);s.refined_live=true;
        if(!same||!cfd_refined_session_init(&s.refined,request)) {
            s.state="failed";s.error=same?s.refined.error:"refined_scene_request_mismatch";
            fprintf(stderr,"%s\n",s.error?s.error:"refined_initialization_failed");
            publish(&s,true);cfd_3d_session_destroy(&s.cartesian);cfd_refined_session_destroy(&s.refined);json_object_put(request);return 2;
        }
        s.geometry=json_object_new_array();
    } else if (open_model || mac_model || !strcmp(string(request,"model"),"incompressible_channel_fv_v1")) {
        struct json_object *ch=member(request,"channel"),*fluid=member(request,"fluid"),*dims=member(ch,"dimensions_m");
        double d[3];for(int a=0;a<3;a++)d[a]=json_object_get_double(json_object_array_get_idx(dims,a));
        snprintf(path,sizeof(path),"%s/scene_runtime.json",s.root);
        struct json_object *doc=json_object_from_file(path);
        struct json_object *authored=member(member(member(doc,"extensions"),"physics_sim"),"channel_flow");
        bool same=authored && json_object_equal(authored,ch) &&
            json_object_array_length(member(doc,"objects"))==0;
        json_object_put(doc);
        int n=json_object_get_int(json_object_array_get_idx(member(request,"grid"),1));
        double rho=json_object_get_double(member(fluid,"density_kg_m3")),mu=json_object_get_double(member(fluid,"dynamic_viscosity_pa_s"));
        double g=json_object_get_double(member(ch,"pressure_gradient_pa_m")),bottom=json_object_get_double(member(ch,"wall_bottom_m_s")),top=json_object_get_double(member(ch,"wall_top_m_s"));
        int nx=json_object_get_int(json_object_array_get_idx(member(request,"grid"),0));
        bool valid=same&&json_object_get_int(json_object_array_get_idx(member(request,"grid"),2))==1;
        if(open_model) {
            double mean=json_object_get_double(member(ch,"inlet_mean_m_s"));
            valid=valid&&!strcmp(string(ch,"solver_model"),"incompressible_open2d_v1")&&cfd_open2d_init(&s.open,nx,n,d[0],d[1],d[2],rho,mu,mean);
            if(valid){double dx=d[0]/nx,dy=d[1]/n;valid=s.dt*(1.5*mean/dx+2*mu/rho*(1/(dx*dx)+1/(dy*dy)))<=.4;}
            struct json_object *bounds=member(ch,"obstacle_bounds_m");
            if(valid&&bounds){int b[4];valid=json_object_get_type(bounds)==json_type_array&&json_object_array_length(bounds)==4;
                for(int k=0;k<4&&valid;k++){double x=json_object_get_double(json_object_array_get_idx(bounds,k))*(k%2?n:nx)/d[k%2];
                    valid=isfinite(x)&&x>=3&&x<=(k%2?n:nx)-3&&fabs(x-round(x))<1e-8;if(valid)b[k]=(int)round(x);}
                valid=valid&&cfd_open2d_set_obstacle(&s.open,b[0],b[1],b[2],b[3]);}
            if(!valid){fprintf(stderr,"invalid open model, obstacle padding or CFL/diffusion bound\n");cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);return 2;}
            s.open_live=true;
            if(!cfd_steady_init(&s.steady,&s.open)){cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);return 2;}
        } else if(mac_model) {
            double perturb=json_object_get_double(member(ch,"initial_divergence_perturbation_m_s"));
            valid=valid&&!strcmp(string(ch,"solver_model"),"incompressible_mac2d_v1")&&cfd_mac2d_init(&s.mac,nx,n,d,rho,mu,g,bottom,top,perturb);
            if(valid) {
                double dx=d[0]/nx,dy=d[1]/n;
                double speed=fmax(fabs(bottom),fabs(top))+fabs(g)*d[1]*d[1]/(8*mu)+fabs(perturb);
                valid=s.dt*(speed/dx+fabs(perturb)/dy+2*mu/rho*(1/(dx*dx)+1/(dy*dy))+sqrt(fabs(g/rho)/dx))/.4<=1024;
            }
            struct json_object *bounds=member(ch,"obstacle_bounds_m");
            if(valid && bounds) {
                int b[4];valid=json_object_get_type(bounds)==json_type_array && json_object_array_length(bounds)==4;
                for(int k=0;k<4 && valid;k++) {
                    double x=json_object_get_double(json_object_array_get_idx(bounds,k))*(k%2?n:nx)/d[k%2];
                    valid=isfinite(x)&&x>=2&&x<=(k%2?n:nx)-2&&fabs(x-round(x))<1e-8;
                    if(valid) b[k]=(int)round(x);
                }
                valid=valid && cfd_mac2d_set_obstacle(&s.mac,b[0],b[1],b[2],b[3]);
            }
            if(!valid){fprintf(stderr,"invalid MAC model or substep budget exceeded\n");cfd_3d_session_destroy(&s.cartesian);cfd_refined_session_destroy(&s.refined);cfd_mac2d_destroy(&s.mac);cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);return 2;}
            s.mac_live=true;
        } else {
            valid=valid&&nx==1&&cfd_channel_init(&s.channel,n,d,rho,mu,g,bottom,top);
            if(!valid){fprintf(stderr,"invalid channel configuration or characteristic Reynolds number exceeds 100\n");return 2;}
            double dy=s.channel.height/n;
            if(s.channel.mu*s.dt/(s.channel.rho*dy*dy)>1e8){fprintf(stderr,"channel diffusion condition budget exceeded\n");return 2;}
            s.channel_live=true;
        }
        s.geometry=json_object_new_array();
    } else {
    cfg.space_mode = SPACE_MODE_3D;
    cfg.sim_mode = SIM_MODE_WIND_TUNNEL;
    cfg.physics_substeps = 1;
    cfg.physics_fixed_dt = s.dt;
    cfg.fluid_3d_solver_region_cell_budget =
        json_object_get_int(member(request, "solver_cell_budget"));
    struct json_object *grid = member(request, "grid");
    cfg.grid_w = json_object_get_int(json_object_array_get_idx(grid, 0));
    cfg.grid_h = json_object_get_int(json_object_array_get_idx(grid, 1));
    cfg.grid_d = json_object_get_int(json_object_array_get_idx(grid, 2));
    if (cfg.grid_w < 4 || cfg.grid_h < 4 || cfg.grid_d < 4 || cfg.grid_w > 256 ||
        cfg.grid_h > 256 || cfg.grid_d > 256)
        return 2;
    FluidScenePreset preset = *scene_presets_get_default();
    SceneRuntimeLaunch launch = {.has_retained_scene = true};
    snprintf(launch.retained_runtime_scene_path, sizeof(launch.retained_runtime_scene_path),
             "%s/scene_runtime.json", s.root);
    char diagnostics[256];
    if (!scene_runtime_launch_apply_retained_projection(&launch, &cfg, &preset, diagnostics,
                                                        sizeof(diagnostics))) {
        fprintf(stderr, "%s\n", diagnostics);
        return 2;
    }
    route = sim_mode_resolve_route(cfg.sim_mode, cfg.space_mode);
    if (route.hooks && route.hooks->configure_app)
        route.hooks->configure_app(&cfg, &preset);
    struct json_object *fluid = member(request, "fluid");
    if (fluid) {
        double rho = json_object_get_double(member(fluid, "density_kg_m3"));
        double mu = json_object_get_double(member(fluid, "dynamic_viscosity_pa_s"));
        if (!isfinite(rho) || !isfinite(mu) || rho < .001 || rho > 50000 || mu < 1e-9 || mu > 1000) {
            fprintf(stderr, "invalid SI fluid properties\n");
            return 2;
        }
        cfg.fluid_3d_si_viscosity = true;
        cfg.fluid_3d_kinematic_viscosity_m2_s = (float)(mu/rho);
    }
    cfg.fluid_3d_disable_wind_heuristics = json_object_get_boolean(member(request, "qualification_mode"));
    if (cfg.fluid_3d_disable_wind_heuristics && !fluid) return 2;
    if (member(request, "solver_iterations")) {
        cfg.fluid_solver_iterations = json_object_get_int(member(request, "solver_iterations"));
        if (cfg.fluid_solver_iterations < 8 || cfg.fluid_solver_iterations > (cfg.fluid_3d_disable_wind_heuristics ? 512 : 48)) return 2;
    }
    s.scene = scene_create(&cfg, &preset, NULL, &route);
    if (!scene_load_runtime_visual_bootstrap(&s.scene, launch.retained_runtime_scene_path) ||
        !sim_runtime_backend_valid(s.scene.backend)) {
        s.state = "failed";
        s.error = "scene_initialization_failed";
        publish(&s, true);
        scene_destroy(&s.scene);
        json_object_put(request);
        return 1;
    }
    SimRuntimeBackendReport initial_report = {0};
    scene_backend_report(&s.scene, &initial_report);
    if (cfg.fluid_3d_disable_wind_heuristics &&
        initial_report.cell_count > app_config_3d_solver_region_cell_budget(&cfg)) {
        s.state="failed"; s.error="qualification_global_solver_budget_exceeded";
        publish(&s,true); scene_destroy(&s.scene); json_object_put(request); return 2;
    }
    s.geometry = json_object_new_array();
    SimRuntime3DDomainDesc mesh_domain = {
        .grid_w=initial_report.domain_w, .grid_h=initial_report.domain_h,
        .grid_d=initial_report.domain_d, .slice_cell_count=(size_t)initial_report.domain_w*initial_report.domain_h,
        .cell_count=initial_report.cell_count, .voxel_size=initial_report.voxel_size,
        .world_min_x=initial_report.world_min_x, .world_min_y=initial_report.world_min_y,
        .world_min_z=initial_report.world_min_z, .world_max_x=initial_report.world_max_x,
        .world_max_y=initial_report.world_max_y, .world_max_z=initial_report.world_max_z};
    for (int i=0; i<s.scene.runtime_visual.mesh_previews.instance_count; i++) {
        PhysicsSimRuntimeMeshDiagnostic diag={0};
        bool ok=physics_sim_runtime_mesh_diagnostic_collect(&s.scene.runtime_visual.mesh_previews,
            i, &mesh_domain, &diag);
        struct json_object *g=json_object_new_object();
        str(g,"object_id",diag.object_id);
        integer(g,"triangle_count",diag.runtime_triangle_count);
        integer(g,"obstacle_voxels",diag.obstacle_voxel_count);
        str(g,"diagnostics",diag.diagnostics);
        json_object_array_add(s.geometry,g);
        if (!ok || !diag.valid || diag.budget_limited || !diag.runtime_triangle_count || !diag.obstacle_voxel_count) {
            fprintf(stderr,"mesh qualification failed: %s\n",diag.diagnostics);
            s.state="failed"; s.error="mesh_geometry_not_resolved";
            publish(&s,true); scene_destroy(&s.scene);json_object_put(s.geometry);json_object_put(request);return 2;
        }
    }
    double diffusion_number = cfg.fluid_3d_kinematic_viscosity_m2_s * s.dt /
        ((double)initial_report.voxel_size * initial_report.voxel_size);
    if (cfg.fluid_3d_si_viscosity && (!isfinite(diffusion_number) || diffusion_number > 1024.0/6.0)) {
        fprintf(stderr, "SI viscosity requires more than 1024 diffusion substeps; reduce dt or resolution\n");
        s.state = "failed"; s.error = "viscosity_substep_budget_exceeded";
        publish(&s, true); scene_destroy(&s.scene); json_object_put(request); return 2;
    }
    if (route.hooks && route.hooks->prepare_scene)
        route.hooks->prepare_scene(&s.scene);
    }
    if (argc == 3) {
        struct json_object *o = snapshot(&s);
        puts(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
        json_object_put(o);
        cfd_3d_session_destroy(&s.cartesian);cfd_refined_session_destroy(&s.refined);cfd_mac2d_destroy(&s.mac);cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);
        scene_destroy(&s.scene);
        json_object_put(s.geometry);
        json_object_put(request);
        return 0;
    }
    signal(SIGTERM, stop_signal);
    signal(SIGINT, stop_signal);
    s.paused = json_object_get_boolean(member(request, "start_paused"));
    s.state = s.paused ? "paused" : "running";
    if (!publish(&s, true)) {
        cfd_3d_session_destroy(&s.cartesian);cfd_refined_session_destroy(&s.refined);cfd_mac2d_destroy(&s.mac);cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);
        scene_destroy(&s.scene);
        json_object_put(request);
        return 1;
    }
    double last_publish = monotonic_seconds();
    while (!terminal(&s)) {
        if (interrupted) {
            s.state = "cancelled";
            break;
        }
        snprintf(path, sizeof(path), "%s/commands/%08llu.json", s.root,
                 (unsigned long long)(s.command_sequence + 1));
        struct json_object *command = json_object_from_file(path);
        if (command) {
            const char *action = string(command, "action");
            const char *error = "";
            s.command_sequence++;
            if (strcmp(string(command, "scene_revision"), string(request, "scene_revision")))
                error = "stale_scene_revision";
            else if (!strcmp(action, "pause")) {
                s.paused = true;
                s.state = "paused";
            } else if (!strcmp(action, "continue")) {
                s.paused = false;
                s.state = "running";
            } else if (!strcmp(action, "cancel"))
                s.state = "cancelled";
            else if (!strcmp(action, "step") && s.paused)
                step(&s, &cfg, route.hooks);
            else
                error = !strcmp(action, "step") ? "step_requires_paused" : "unsupported_command";
            export_cancelled_fields(&s);
            if (!publish(&s, true)) {
                s.state = "failed";
                s.error = "snapshot_io_failed";
            }
            struct json_object *receipt = json_object_new_object();
            str(receipt, "command_id", string(command, "command_id"));
            str(receipt, "run_id", string(request, "run_id"));
            str(receipt, "scene_revision", string(request, "scene_revision"));
            str(receipt, "action", action);
            str(receipt, "state", s.state);
            str(receipt, "status", *error ? "rejected" : s.error ? "failed" :
                !strcmp(action,"step") && s.cartesian.cancelled ? "cancelled" : "applied");
            str(receipt, "error", *error ? error : (s.error ? s.error : ""));
            integer(receipt, "tick", s.tick);
            num(receipt, "simulation_time", s.scene.time);
            integer(receipt, "sequence", s.command_sequence);
            snprintf(path, sizeof(path), "%s/receipts/%08llu.json", s.root,
                     (unsigned long long)s.command_sequence);
            if (!atomic_json(path, receipt)) {
                s.state = "failed";
                s.error = "receipt_io_failed";
            }
            json_object_put(receipt);
            json_object_put(command);
            continue;
        }
        sample_requests(&s);
        if (s.paused) {
            struct timespec delay = {0, 10000000};
            nanosleep(&delay, NULL);
            continue;
        }
        step(&s, &cfg, route.hooks);
        if (monotonic_seconds() - last_publish >= 0.1) {
            if (!publish(&s, false)) {
                s.state = "failed";
                s.error = "snapshot_io_failed";
            }
            last_publish = monotonic_seconds();
        }
    }
    finish_checkpoint_cancel(&s);
    export_cancelled_fields(&s);
    sample_requests(&s);
    bool ok = publish(&s, true);
    json_object_put(s.pending_cancel_command);
    json_object_put(s.geometry);
    json_object_put(s.history);
    cfd_3d_session_destroy(&s.cartesian);cfd_refined_session_destroy(&s.refined);cfd_mac2d_destroy(&s.mac);cfd_steady_destroy(&s.steady);cfd_open2d_destroy(&s.open);
    scene_destroy(&s.scene);
    json_object_put(request);
    return ok && strcmp(s.state, "failed") ? 0 : 1;
}
