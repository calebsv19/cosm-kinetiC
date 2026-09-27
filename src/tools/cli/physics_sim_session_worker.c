#define _DARWIN_C_SOURCE 1
// Trusted-local session owner. Only this process mutates the live scene.
#include "app/app_config.h"
#include "app/scene_core_sim_runtime_step.h"
#include "app/scene_runtime_launch_projection.h"
#include "app/scene_state.h"
#include "app/session_observation.h"
#include "core_scene_compile.h"
#include "export/export_paths.h"
#include "export/volume_frames.h"
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
    uint64_t tick, sequence, command_sequence;
    int limit;
    double dt, step_ms;
    const char *state;
    const char *error;
    bool paused;
} Session;

static struct json_object *snapshot(Session *s) {
    SimRuntimeBackendReport r = {0};
    scene_backend_report(&s->scene, &r);
    struct json_object *o = json_object_new_object();
    str(o, "schema", "physics_sim_session_snapshot_v1");
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
    num(o, "updated_at", (double)time(NULL));
    str(o, "model", "wind_approximate_v1");
    str(o, "model_limitations", "Injected wake; pressure and drag are proxies, not validated CFD.");
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
    str(health, "pressure_convergence", "unavailable: fixed-iteration solver");
    str(health, "nonfinite_scope", "bounded preview samples and reported metrics");
    json_object_object_add(o, "health", health);
    json_object_object_add(o, "preview", physics_sim_session_observation(&s->scene));
    return o;
}
static bool publish(Session *s, bool event) {
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
    return ok;
}
static bool step(Session *s, AppConfig *cfg, const SimModeHooks *hooks) {
    double begin = monotonic_seconds();
    s->scene.dt = s->dt;
    bool ok = physics_sim_scene_core_sim_step(&s->scene, cfg, hooks, s->dt, NULL);
    s->step_ms = (monotonic_seconds() - begin) * 1000;
    if (ok)
        s->tick++;
    else {
        s->state = "failed";
        s->error = "solver_failure_or_region_budget";
    }
    SimRuntimeBackendReport r = {0};
    scene_backend_report(&s->scene, &r);
    if (!isfinite(r.runtime_solver_max_abs_divergence_after_project) ||
        !isfinite(r.runtime_solver_max_velocity_magnitude_post_clamp)) {
        s->state = "failed";
        s->error = "nonfinite_solver_metrics";
        ok = false;
    }
    if (ok && s->tick >= (uint64_t)s->limit) {
        // Final full fields are an explicit result artifact, never a preview side effect.
        if (volume_frames_write(&s->scene, s->tick))
            s->state = "completed";
        else {
            s->state = "failed";
            s->error = "final_volume_export_failed";
            ok = false;
        }
    }
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
    SimModeRoute route = sim_mode_resolve_route(cfg.sim_mode, cfg.space_mode);
    if (route.hooks && route.hooks->configure_app)
        route.hooks->configure_app(&cfg, &preset);
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
    if (route.hooks && route.hooks->prepare_scene)
        route.hooks->prepare_scene(&s.scene);
    if (argc == 3) {
        struct json_object *o = snapshot(&s);
        puts(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
        json_object_put(o);
        scene_destroy(&s.scene);
        json_object_put(request);
        return 0;
    }
    signal(SIGTERM, stop_signal);
    signal(SIGINT, stop_signal);
    s.paused = json_object_get_boolean(member(request, "start_paused"));
    s.state = s.paused ? "paused" : "running";
    if (!publish(&s, true)) {
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
            str(receipt, "status", *error ? "rejected" : (s.error ? "failed" : "applied"));
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
    bool ok = publish(&s, true);
    scene_destroy(&s.scene);
    json_object_put(request);
    return ok && strcmp(s.state, "failed") ? 0 : 1;
}
