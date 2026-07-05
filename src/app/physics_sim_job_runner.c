#include "app/physics_sim_job_runner.h"
#include "app/physics_sim_headless_job_bundle.h"
#include "app/physics_sim_job_runner_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__unix__)
extern time_t timegm(struct tm *tm);
#endif

static bool spawn_detached_headless(const char *headless_cli_path,
                                    const PhysicsSimDetachedJobPaths *paths,
                                    const PhysicsSimDetachedRequest *request,
                                    pid_t *out_pid) {
    pid_t pid = 0;
    FILE *stdout_file = NULL;
    FILE *stderr_file = NULL;
    char frames_arg[32];
    char sim_steps_arg[32];
    char progress_interval_arg[32];
    char volume_export_start_arg[32];
    char volume_export_stride_arg[32];
    char volume_export_max_arg[32];

    if (out_pid) *out_pid = 0;
    if (!headless_cli_path || !paths || !request) return false;
    if (!ensure_directory_exists(paths->job_root)) return false;

    snprintf(frames_arg, sizeof(frames_arg), "%d", request->frames);
    snprintf(sim_steps_arg, sizeof(sim_steps_arg), "%d", request->sim_steps_per_frame);
    snprintf(progress_interval_arg, sizeof(progress_interval_arg), "%d", request->progress_interval);
    snprintf(volume_export_start_arg, sizeof(volume_export_start_arg), "%d", request->volume_export_start_frame);
    snprintf(volume_export_stride_arg, sizeof(volume_export_stride_arg), "%d", request->volume_export_stride);
    snprintf(volume_export_max_arg, sizeof(volume_export_max_arg), "%d", request->volume_export_max_frames);

    pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        char *argv[38];
        int argc = 0;
        int null_fd = -1;
        if (setsid() < 0) _exit(126);
        stdout_file = fopen(paths->stdout_log_path, "ab");
        stderr_file = fopen(paths->stderr_log_path, "ab");
        if (!stdout_file || !stderr_file) _exit(126);
        if (dup2(fileno(stdout_file), STDOUT_FILENO) < 0) _exit(126);
        if (dup2(fileno(stderr_file), STDERR_FILENO) < 0) _exit(126);
        null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }

        argv[argc++] = (char *)headless_cli_path;
        argv[argc++] = (char *)"--runtime-scene";
        argv[argc++] = (char *)request->runtime_scene_path;
        argv[argc++] = (char *)"--frames";
        argv[argc++] = frames_arg;
        argv[argc++] = (char *)"--output-root";
        argv[argc++] = (char *)request->output_root;
        argv[argc++] = (char *)"--summary";
        argv[argc++] = (char *)paths->result_summary_path;
        argv[argc++] = (char *)"--progress";
        argv[argc++] = (char *)paths->progress_path;
        argv[argc++] = (char *)"--cancel-flag";
        argv[argc++] = (char *)paths->cancel_request_path;
        argv[argc++] = (char *)"--progress-interval";
        argv[argc++] = progress_interval_arg;
        argv[argc++] = (char *)"--sim-steps-per-frame";
        argv[argc++] = sim_steps_arg;
        if (request->grid[0]) {
            argv[argc++] = (char *)"--grid";
            argv[argc++] = (char *)request->grid;
        }
        if (request->wind_shot_camera_profile[0]) {
            argv[argc++] = (char *)"--wind-shot-camera";
            argv[argc++] = (char *)request->wind_shot_camera_profile;
        }
        if (request->save_volume_frames) argv[argc++] = (char *)"--save-volume-frames";
        if (request->save_volume_frames && request->volume_export_start_frame > 0) {
            argv[argc++] = (char *)"--volume-export-start-frame";
            argv[argc++] = volume_export_start_arg;
        }
        if (request->save_volume_frames && request->volume_export_stride > 1) {
            argv[argc++] = (char *)"--volume-export-stride";
            argv[argc++] = volume_export_stride_arg;
        }
        if (request->save_volume_frames && request->volume_export_max_frames > 0) {
            argv[argc++] = (char *)"--volume-export-max-frames";
            argv[argc++] = volume_export_max_arg;
        }
        if (request->save_render_frames) argv[argc++] = (char *)"--save-render-frames";
        if (request->save_wind_projection_frames) {
            argv[argc++] = (char *)"--save-wind-projection-frames";
        }
        argv[argc++] = (char *)(request->skip_present ? "--skip-present" : "--present");
        if (request->overwrite) argv[argc++] = (char *)"--overwrite";
        argv[argc] = NULL;

        execv(headless_cli_path, argv);
        _exit(127);
    }
    if (out_pid) *out_pid = pid;
    return true;
}

static bool validate_grid_request(const char *text) {
    const char *p = text;
    char *end = NULL;
    if (!text || !text[0]) return false;
    for (int axis = 0; axis < 3; ++axis) {
        long value = 0;
        errno = 0;
        value = strtol(p, &end, 10);
        if (errno != 0 || end == p || value < 4 || value > 512) {
            return false;
        }
        if (axis < 2) {
            if (*end != 'x' && *end != 'X') return false;
            p = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }
    return true;
}

static bool load_request_file(const char *request_path,
                              PhysicsSimDetachedRequest *out_request,
                              char *out_diagnostics,
                              size_t out_diagnostics_size) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!request_path || !request_path[0] || !out_request) {
        diag_set(out_diagnostics, out_diagnostics_size, "invalid request path");
        return false;
    }
    memset(out_request, 0, sizeof(*out_request));
    snprintf(out_request->schema_version,
             sizeof(out_request->schema_version),
             "physics_sim_headless_request_v1");
    out_request->frames = 1;
    out_request->sim_steps_per_frame = 1;
    out_request->progress_interval = 1;
    out_request->volume_export_start_frame = 0;
    out_request->volume_export_stride = 1;
    out_request->volume_export_max_frames = 0;
    copy_string(out_request->wind_shot_camera_profile,
                sizeof(out_request->wind_shot_camera_profile),
                "three_quarter");
    out_request->skip_present = true;

    root = json_object_from_file(request_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "failed to parse request json");
        return false;
    }
    if (json_get_string(root, "schema_version", &text_value)) {
        copy_string(out_request->schema_version, sizeof(out_request->schema_version), text_value);
    }
    if (!json_get_string(root, "runtime_scene_path", &text_value) ||
        !copy_string(out_request->runtime_scene_path, sizeof(out_request->runtime_scene_path), text_value)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "request missing runtime_scene_path");
        return false;
    }
    if (!json_get_string(root, "output_root", &text_value) ||
        !copy_string(out_request->output_root, sizeof(out_request->output_root), text_value)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "request missing output_root");
        return false;
    }
    (void)json_get_int(root, "frames", &out_request->frames);
    (void)json_get_int(root, "sim_steps_per_frame", &out_request->sim_steps_per_frame);
    (void)json_get_int(root, "progress_interval", &out_request->progress_interval);
    if (json_get_string(root, "wind_shot_camera", &text_value) ||
        json_get_string(root, "wind_shot_camera_profile", &text_value)) {
        if (strcmp(text_value, "three_quarter") != 0 &&
            strcmp(text_value, "side") != 0 &&
            strcmp(text_value, "top") != 0 &&
            strcmp(text_value, "downstream") != 0 &&
            strcmp(text_value, "runtime_default") != 0) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "invalid wind_shot_camera profile");
            return false;
        }
        copy_string(out_request->wind_shot_camera_profile,
                    sizeof(out_request->wind_shot_camera_profile),
                    text_value);
    }
    if (json_get_string(root, "grid", &text_value)) {
        if (!validate_grid_request(text_value) ||
            !copy_string(out_request->grid, sizeof(out_request->grid), text_value)) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "invalid grid override");
            return false;
        }
    }
    (void)json_get_bool(root, "save_volume_frames", &out_request->save_volume_frames);
    if (json_get_int(root, "volume_export_start_frame", &int_value)) {
        out_request->volume_export_start_frame = int_value;
    }
    if (json_get_int(root, "volume_export_stride", &int_value)) {
        out_request->volume_export_stride = int_value;
    }
    if (json_get_int(root, "volume_export_max_frames", &int_value)) {
        out_request->volume_export_max_frames = int_value;
    }
    (void)json_get_bool(root, "save_render_frames", &out_request->save_render_frames);
    (void)json_get_bool(root, "save_wind_projection_frames", &out_request->save_wind_projection_frames);
    (void)json_get_bool(root, "skip_present", &out_request->skip_present);
    (void)json_get_bool(root, "overwrite", &out_request->overwrite);
    json_object_put(root);

    if (out_request->frames <= 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request frames must be positive");
        return false;
    }
    if (out_request->sim_steps_per_frame <= 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request sim_steps_per_frame must be positive");
        return false;
    }
    if (out_request->progress_interval < 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request progress_interval must be non-negative");
        return false;
    }
    if (out_request->volume_export_start_frame < 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request volume_export_start_frame must be non-negative");
        return false;
    }
    if (out_request->volume_export_stride <= 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request volume_export_stride must be positive");
        return false;
    }
    if (out_request->volume_export_max_frames < 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "request volume_export_max_frames must be non-negative");
        return false;
    }
    if (!file_exists(out_request->runtime_scene_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "runtime_scene_path does not exist");
        return false;
    }
    return true;
}

static bool load_request_for_job(const char *request_path,
                                 PhysicsSimDetachedRequest *out_request,
                                 PhysicsSimHeadlessJobBundle *out_bundle,
                                 bool *out_is_shared_bundle,
                                 char *out_diagnostics,
                                 size_t out_diagnostics_size) {
    char request_diag[256];
    char bundle_diag[256];

    if (out_is_shared_bundle) *out_is_shared_bundle = false;
    if (out_bundle) memset(out_bundle, 0, sizeof(*out_bundle));

    if (load_request_file(request_path, out_request, request_diag, sizeof(request_diag))) {
        diag_set(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    if (!physics_sim_headless_job_bundle_load(request_path,
                                              out_bundle,
                                              bundle_diag,
                                              sizeof(bundle_diag))) {
        if (out_diagnostics && out_diagnostics_size > 0u) {
            snprintf(out_diagnostics,
                     out_diagnostics_size,
                     "request load failed (%s); outer bundle load failed (%s)",
                     request_diag,
                     bundle_diag);
        }
        return false;
    }

    if (!load_request_file(out_bundle->resolved_run_config_path,
                           out_request,
                           request_diag,
                           sizeof(request_diag))) {
        diag_set(out_diagnostics, out_diagnostics_size, request_diag);
        return false;
    }
    if (!copy_string(out_request->runtime_scene_path,
                     sizeof(out_request->runtime_scene_path),
                     out_bundle->resolved_scene_payload_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to apply outer scene payload path");
        return false;
    }
    if (out_is_shared_bundle) *out_is_shared_bundle = true;
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool build_default_shared_job_envelope(const PhysicsSimDetachedRequest *request,
                                              const PhysicsSimDetachedJobPaths *paths,
                                              const char *job_id,
                                              const char *created_at_utc,
                                              CoreHeadlessJobEnvelope *out_envelope) {
    if (!request || !paths || !job_id || !created_at_utc || !out_envelope) return false;
    core_headless_job_envelope_init(out_envelope);
    if (!copy_string(out_envelope->job_id, sizeof(out_envelope->job_id), job_id) ||
        !copy_string(out_envelope->program, sizeof(out_envelope->program), "physics_sim") ||
        !copy_string(out_envelope->tool.name, sizeof(out_envelope->tool.name), "physics_sim_headless") ||
        !copy_string(out_envelope->tool.version, sizeof(out_envelope->tool.version), "0.1.0") ||
        !copy_string(out_envelope->tool.target_os, sizeof(out_envelope->tool.target_os), "linux") ||
        !copy_string(out_envelope->tool.target_arch, sizeof(out_envelope->tool.target_arch), "x86_64") ||
        !copy_string(out_envelope->scene_payload.schema_family,
                     sizeof(out_envelope->scene_payload.schema_family),
                     "codework_scene") ||
        !copy_string(out_envelope->scene_payload.schema_variant,
                     sizeof(out_envelope->scene_payload.schema_variant),
                     "scene_runtime_v1") ||
        !copy_string(out_envelope->scene_payload.path,
                     sizeof(out_envelope->scene_payload.path),
                     request->runtime_scene_path) ||
        !copy_string(out_envelope->run_config.schema_family,
                     sizeof(out_envelope->run_config.schema_family),
                     "physics_sim_request") ||
        !copy_string(out_envelope->run_config.schema_variant,
                     sizeof(out_envelope->run_config.schema_variant),
                     "physics_sim_headless_request_v1") ||
        !copy_string(out_envelope->run_config.path,
                     sizeof(out_envelope->run_config.path),
                     paths->job_request_path) ||
        !copy_string(out_envelope->outputs.root,
                     sizeof(out_envelope->outputs.root),
                     paths->job_root) ||
        !copy_string(out_envelope->outputs.report_path,
                     sizeof(out_envelope->outputs.report_path),
                     paths->shared_report_path) ||
        !copy_string(out_envelope->outputs.logs_dir,
                     sizeof(out_envelope->outputs.logs_dir),
                     paths->job_root) ||
        !copy_string(out_envelope->outputs.artifacts_dir,
                     sizeof(out_envelope->outputs.artifacts_dir),
                     request->output_root) ||
        !copy_string(out_envelope->metadata.created_by,
                     sizeof(out_envelope->metadata.created_by),
                     "physics_sim_job_runner") ||
        !copy_string(out_envelope->metadata.created_at,
                     sizeof(out_envelope->metadata.created_at),
                     created_at_utc)) {
        return false;
    }
    return core_headless_job_envelope_validate(out_envelope);
}

static bool write_canonical_request_file(const char *path,
                                         const PhysicsSimDetachedRequest *request) {
    FILE *file = NULL;
    if (!path || !request) return false;
    if (!ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": ");
    json_write_string(file, request->schema_version);
    fprintf(file, ",\n");
    fprintf(file, "  \"runtime_scene_path\": ");
    json_write_string(file, request->runtime_scene_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    json_write_string(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"frames\": %d,\n", request->frames);
    fprintf(file, "  \"sim_steps_per_frame\": %d,\n", request->sim_steps_per_frame);
    fprintf(file, "  \"progress_interval\": %d,\n", request->progress_interval);
    if (request->grid[0]) {
        fprintf(file, "  \"grid\": ");
        json_write_string(file, request->grid);
        fprintf(file, ",\n");
    }
    fprintf(file, "  \"wind_shot_camera\": ");
    json_write_string(file,
                      request->wind_shot_camera_profile[0]
                          ? request->wind_shot_camera_profile
                          : "three_quarter");
    fprintf(file, ",\n");
    fprintf(file, "  \"save_volume_frames\": %s,\n", request->save_volume_frames ? "true" : "false");
    fprintf(file, "  \"volume_export_start_frame\": %d,\n", request->volume_export_start_frame);
    fprintf(file, "  \"volume_export_stride\": %d,\n", request->volume_export_stride);
    fprintf(file, "  \"volume_export_max_frames\": %d,\n", request->volume_export_max_frames);
    fprintf(file, "  \"save_render_frames\": %s,\n", request->save_render_frames ? "true" : "false");
    fprintf(file,
            "  \"save_wind_projection_frames\": %s,\n",
            request->save_wind_projection_frames ? "true" : "false");
    fprintf(file, "  \"skip_present\": %s,\n", request->skip_present ? "true" : "false");
    fprintf(file, "  \"overwrite\": %s\n", request->overwrite ? "true" : "false");
    fprintf(file, "}\n");
    fclose(file);
    return true;
}

bool physics_sim_job_runner_submit(const char *argv0,
                                   const char *request_path,
                                   const char *jobs_root_override,
                                   bool overwrite,
                                   char *out_job_id,
                                   size_t out_job_id_size,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    PhysicsSimDetachedRequest request;
    PhysicsSimDetachedJobPaths paths;
    PhysicsSimDetachedJobRecord record;
    PhysicsSimHeadlessJobBundle source_bundle;
    CoreHeadlessJobEnvelope shared_envelope;
    char jobs_root[PATH_MAX];
    char headless_cli_path[PATH_MAX];
    char diagnostics[256];
    pid_t pid = 0;
    bool is_shared_bundle = false;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !request_path || !request_path[0] || !out_job_id || out_job_id_size == 0u) {
        return false;
    }
    if (!load_request_for_job(request_path,
                              &request,
                              &source_bundle,
                              &is_shared_bundle,
                              diagnostics,
                              sizeof(diagnostics))) {
        diag_set(out_diagnostics, out_diagnostics_size, diagnostics);
        return false;
    }
    if (overwrite) request.overwrite = true;
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    if (!derive_headless_cli_path(argv0, headless_cli_path, sizeof(headless_cli_path))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve headless cli path");
        return false;
    }
    if (is_shared_bundle) {
        if (!copy_string(out_job_id, out_job_id_size, source_bundle.envelope.job_id)) {
            diag_set(out_diagnostics, out_diagnostics_size, "bundle job id exceeds local buffer");
            return false;
        }
    } else {
        if (!generate_job_id(out_job_id, out_job_id_size)) {
            diag_set(out_diagnostics, out_diagnostics_size, "failed to generate job id");
            return false;
        }
    }
    build_job_paths(jobs_root, out_job_id, &paths);
    if (file_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job id collision");
        return false;
    }
    if (is_shared_bundle) {
        if (snprintf(request.output_root,
                     sizeof(request.output_root),
                     "%s/output/artifacts",
                     paths.job_root) >= (int)sizeof(request.output_root)) {
            diag_set(out_diagnostics, out_diagnostics_size, "failed to derive bundle artifacts path");
            return false;
        }
    }
    if (!request.overwrite &&
        file_exists(request.output_root) &&
        !dir_is_empty(request.output_root)) {
        diag_set(out_diagnostics,
                 out_diagnostics_size,
                 "output root already exists and is not empty; use --overwrite");
        return false;
    }
    if (!ensure_directory_exists(paths.job_root)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to create job directory");
        return false;
    }
    if (!write_canonical_request_file(paths.job_request_path, &request)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to stage job request");
        return false;
    }

    detached_job_record_defaults(&record);
    snprintf(record.job_id, sizeof(record.job_id), "%s", out_job_id);
    copy_string(record.request_path, sizeof(record.request_path), paths.job_request_path);
    copy_string(record.output_root, sizeof(record.output_root), request.output_root);
    copy_string(record.progress_path, sizeof(record.progress_path), paths.progress_path);
    copy_string(record.summary_path, sizeof(record.summary_path), paths.result_summary_path);
    copy_string(record.stdout_path, sizeof(record.stdout_path), paths.stdout_log_path);
    copy_string(record.stderr_path, sizeof(record.stderr_path), paths.stderr_log_path);
    utc_now_string(record.submitted_at_utc, sizeof(record.submitted_at_utc));
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    snprintf(record.overwrite_policy,
             sizeof(record.overwrite_policy),
             "%s",
             request.overwrite ? "overwrite" : "fail_if_exists");
    record.frames_requested = request.frames;
    record.frames_completed = 0;
    record.frame_index = 0;
    record.sim_steps_per_frame = request.sim_steps_per_frame;
    record.sim_steps_completed_in_frame = 0;
    record.sim_steps_total_in_frame = request.sim_steps_per_frame;
    snprintf(record.diagnostics, sizeof(record.diagnostics), "queued for detached simulation");
    if (!build_default_shared_job_envelope(&request,
                                           &paths,
                                           out_job_id,
                                           record.submitted_at_utc,
                                           &shared_envelope)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to build shared job envelope");
        return false;
    }
    if (is_shared_bundle) {
        if (!copy_string(shared_envelope.scene_payload.schema_family,
                         sizeof(shared_envelope.scene_payload.schema_family),
                         source_bundle.envelope.scene_payload.schema_family) ||
            !copy_string(shared_envelope.scene_payload.schema_variant,
                         sizeof(shared_envelope.scene_payload.schema_variant),
                         source_bundle.envelope.scene_payload.schema_variant) ||
            !copy_string(shared_envelope.run_config.schema_family,
                         sizeof(shared_envelope.run_config.schema_family),
                         source_bundle.envelope.run_config.schema_family) ||
            !copy_string(shared_envelope.run_config.schema_variant,
                         sizeof(shared_envelope.run_config.schema_variant),
                         source_bundle.envelope.run_config.schema_variant) ||
            !copy_string(shared_envelope.metadata.title,
                         sizeof(shared_envelope.metadata.title),
                         source_bundle.envelope.metadata.title) ||
            !copy_string(shared_envelope.metadata.description,
                         sizeof(shared_envelope.metadata.description),
                         source_bundle.envelope.metadata.description) ||
            !copy_string(shared_envelope.metadata.created_by,
                         sizeof(shared_envelope.metadata.created_by),
                         source_bundle.envelope.metadata.created_by) ||
            !copy_string(shared_envelope.metadata.created_at,
                         sizeof(shared_envelope.metadata.created_at),
                         source_bundle.envelope.metadata.created_at)) {
            diag_set(out_diagnostics, out_diagnostics_size, "failed to preserve source bundle metadata");
            return false;
        }
    }
    if (!physics_sim_headless_job_bundle_write(paths.shared_job_path,
                                               &shared_envelope,
                                               diagnostics,
                                               sizeof(diagnostics))) {
        diag_set(out_diagnostics, out_diagnostics_size, diagnostics);
        return false;
    }
    if (!persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to write queued job status");
        return false;
    }

    if (!spawn_detached_headless(headless_cli_path, &paths, &request, &pid)) {
        snprintf(record.state, sizeof(record.state), "failed");
        utc_now_string(record.finished_at_utc, sizeof(record.finished_at_utc));
        utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
        record.exit_code = 127;
        snprintf(record.diagnostics, sizeof(record.diagnostics), "failed to spawn detached simulation");
        (void)persist_job_state(&paths, &record);
        diag_set(out_diagnostics, out_diagnostics_size, "failed to spawn detached simulation");
        return false;
    }

    record.pid = pid;
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    snprintf(record.state, sizeof(record.state), "starting");
    snprintf(record.stage, sizeof(record.stage), "starting");
    snprintf(record.diagnostics, sizeof(record.diagnostics), "detached simulation launched");
    if (!write_pid_file(paths.pid_path, pid) || !persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to persist detached job state");
        return false;
    }

    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool physics_sim_job_runner_print_status(FILE *out,
                                         const char *argv0,
                                         const char *job_id,
                                         const char *jobs_root_override,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    PhysicsSimDetachedJobPaths paths;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!out || !argv0 || !job_id || !job_id[0]) return false;
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    build_job_paths(jobs_root, job_id, &paths);
    if (!file_exists(paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "job status file not found");
        return false;
    }
    {
        PhysicsSimDetachedJobRecord record;
        if (load_job_status_record(&paths, &record)) {
            (void)refresh_job_status_record(&paths, &record);
        }
    }
    if (!print_file_to_stream(out, paths.job_status_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read job status file");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool physics_sim_job_runner_cancel(const char *argv0,
                                   const char *job_id,
                                   const char *jobs_root_override,
                                   char *out_diagnostics,
                                   size_t out_diagnostics_size) {
    char jobs_root[PATH_MAX];
    PhysicsSimDetachedJobPaths paths;
    PhysicsSimDetachedJobRecord record;
    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!argv0 || !job_id || !job_id[0]) return false;
    if (!build_jobs_root(argv0, jobs_root_override, jobs_root, sizeof(jobs_root))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve jobs root");
        return false;
    }
    build_job_paths(jobs_root, job_id, &paths);
    if (!load_job_status_record(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to load job status");
        return false;
    }
    (void)refresh_job_status_record(&paths, &record);
    if (strcmp(record.state, "completed") == 0 ||
        strcmp(record.state, "cancelled") == 0 ||
        strcmp(record.state, "failed") == 0) {
        diag_set(out_diagnostics, out_diagnostics_size, "job already terminal");
        return true;
    }
    if (!write_cancel_flag_file(paths.cancel_request_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to write cancel flag");
        return false;
    }
    utc_now_string(record.updated_at_utc, sizeof(record.updated_at_utc));
    snprintf(record.stage, sizeof(record.stage), "cancel_requested");
    snprintf(record.diagnostics,
             sizeof(record.diagnostics),
             "cancel flag written; awaiting cooperative shutdown");
    if (!persist_job_state(&paths, &record)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to update cancel-requested status");
        return false;
    }
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
