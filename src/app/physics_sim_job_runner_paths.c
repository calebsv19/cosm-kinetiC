#include "app/physics_sim_job_runner_internal.h"

#include "app/physics_sim_diagnostic_helpers.h"
#include "app/physics_sim_file_helpers.h"
#include "app/physics_sim_json_helpers.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>

void diag_set(char *out, size_t out_size, const char *message) {
    physics_sim_diag_set(out, out_size, message);
}

bool copy_string(char *dst, size_t dst_size, const char *src) {
    return physics_sim_copy_string(dst, dst_size, src);
}

bool file_exists(const char *path) {
    return physics_sim_file_exists(path);
}

bool dir_is_empty(const char *path) {
    return physics_sim_dir_is_empty(path);
}

bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    return physics_sim_parent_dir_of(path, out_dir, out_dir_size);
}

bool ensure_directory_exists(const char *path) {
    return physics_sim_ensure_directory_exists(path);
}

bool ensure_parent_directory_exists(const char *path) {
    return physics_sim_ensure_parent_directory_exists(path);
}

bool read_text_file(const char *path, char **out_text) {
    return physics_sim_read_text_file(path, out_text);
}

bool write_text_file(const char *path, const char *text) {
    return physics_sim_write_text_file(path, text);
}

void json_write_string(FILE *file, const char *value) {
    physics_sim_json_write_string(file, value);
}

bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
#if defined(__APPLE__) || defined(__unix__)
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
#else
    {
        struct tm *tmp = gmtime(&now);
        if (!tmp) return false;
        tm_utc = *tmp;
    }
#endif
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

bool resolve_real_path(const char *path, char *out, size_t out_size) {
    return physics_sim_resolve_real_path(path, out, out_size);
}

bool derive_repo_root_from_argv0(const char *argv0, char *out_root, size_t out_root_size) {
    char binary_path[PATH_MAX];
    char *build_marker = NULL;
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    build_marker = strstr(binary_path, "/build/");
    if (!build_marker) return false;
    *build_marker = '\0';
    return copy_string(out_root, out_root_size, binary_path);
}

bool derive_headless_cli_path(const char *argv0, char *out_path, size_t out_path_size) {
    char binary_path[PATH_MAX];
    char dir_path[PATH_MAX];
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    if (!parent_dir_of(binary_path, dir_path, sizeof(dir_path))) return false;
    if (snprintf(out_path, out_path_size, "%s/physics_sim_headless", dir_path) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return file_exists(out_path);
}

bool physics_sim_job_runner_default_jobs_root(const char *argv0,
                                              char *out_path,
                                              size_t out_path_size) {
    char repo_root[PATH_MAX];
    if (!derive_repo_root_from_argv0(argv0, repo_root, sizeof(repo_root))) return false;
    if (snprintf(out_path, out_path_size, "%s/build/agent_runs/jobs", repo_root) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool build_jobs_root(const char *argv0,
                     const char *jobs_root_override,
                     char *out_jobs_root,
                     size_t out_jobs_root_size) {
    char resolved[PATH_MAX];
    if (jobs_root_override && jobs_root_override[0]) {
        if (jobs_root_override[0] == '/') {
            return copy_string(out_jobs_root, out_jobs_root_size, jobs_root_override);
        }
        if (!resolve_real_path(jobs_root_override, resolved, sizeof(resolved))) {
            return copy_string(out_jobs_root, out_jobs_root_size, jobs_root_override);
        }
        return copy_string(out_jobs_root, out_jobs_root_size, resolved);
    }
    return physics_sim_job_runner_default_jobs_root(argv0, out_jobs_root, out_jobs_root_size);
}

void build_job_paths(const char *jobs_root,
                     const char *job_id,
                     PhysicsSimDetachedJobPaths *out_paths) {
    if (!jobs_root || !job_id || !out_paths) return;
    memset(out_paths, 0, sizeof(*out_paths));
    snprintf(out_paths->jobs_root, sizeof(out_paths->jobs_root), "%s", jobs_root);
    snprintf(out_paths->job_root, sizeof(out_paths->job_root), "%s/%s", jobs_root, job_id);
    snprintf(out_paths->job_request_path,
             sizeof(out_paths->job_request_path),
             "%s/job_request.json",
             out_paths->job_root);
    snprintf(out_paths->job_status_path,
             sizeof(out_paths->job_status_path),
             "%s/job_status.json",
             out_paths->job_root);
    snprintf(out_paths->shared_job_path,
             sizeof(out_paths->shared_job_path),
             "%s/job.json",
             out_paths->job_root);
    snprintf(out_paths->shared_report_path,
             sizeof(out_paths->shared_report_path),
             "%s/output/report.json",
             out_paths->job_root);
    snprintf(out_paths->progress_path,
             sizeof(out_paths->progress_path),
             "%s/run_progress.json",
             out_paths->job_root);
    snprintf(out_paths->stdout_log_path,
             sizeof(out_paths->stdout_log_path),
             "%s/stdout.log",
             out_paths->job_root);
    snprintf(out_paths->stderr_log_path,
             sizeof(out_paths->stderr_log_path),
             "%s/stderr.log",
             out_paths->job_root);
    snprintf(out_paths->pid_path, sizeof(out_paths->pid_path), "%s/pid.txt", out_paths->job_root);
    snprintf(out_paths->cancel_request_path,
             sizeof(out_paths->cancel_request_path),
             "%s/cancel_requested.flag",
             out_paths->job_root);
    snprintf(out_paths->result_summary_path,
             sizeof(out_paths->result_summary_path),
             "%s/result_summary.json",
             out_paths->job_root);
}

bool generate_job_id(char *out_job_id, size_t out_job_id_size) {
    struct timespec ts;
    uint32_t salt = 0u;
    if (!out_job_id || out_job_id_size == 0u) return false;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return false;
    salt = (uint32_t)((uint64_t)getpid() ^ (uint64_t)ts.tv_nsec);
    return snprintf(out_job_id,
                    out_job_id_size,
                    "psjob_%lld_%06u",
                    (long long)ts.tv_sec,
                    (unsigned)(salt % 1000000u)) < (int)out_job_id_size;
}

const char *shared_report_state_label(const char *state) {
    if (!state || !state[0]) return "queued";
    if (strcmp(state, "completed") == 0) return "succeeded";
    if (strcmp(state, "starting") == 0) return "running";
    return state;
}

bool build_volume_frames_dir_path(const char *output_root,
                                  char *out_path,
                                  size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path, out_path_size, "%s/volume_frames", output_root) < (int)out_path_size;
}

bool build_render_frames_dir_path(const char *output_root,
                                  char *out_path,
                                  size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path, out_path_size, "%s/render_frames", output_root) < (int)out_path_size;
}

void detached_job_record_defaults(PhysicsSimDetachedJobRecord *record) {
    if (!record) return;
    memset(record, 0, sizeof(*record));
    snprintf(record->state, sizeof(record->state), "queued");
    snprintf(record->stage, sizeof(record->stage), "queued");
    snprintf(record->overwrite_policy, sizeof(record->overwrite_policy), "fail_if_exists");
    snprintf(record->diagnostics, sizeof(record->diagnostics), "queued");
    record->pid = 0;
    record->exit_code = -1;
}

double record_progress_ratio(const PhysicsSimDetachedJobRecord *record) {
    double completed = 0.0;
    if (!record) return 0.0;
    completed = (double)record->frames_completed;
    if (record->sim_steps_total_in_frame > 0) {
        completed +=
            (double)record->sim_steps_completed_in_frame /
            (double)record->sim_steps_total_in_frame;
    }
    if (record->frames_requested > 0) {
        double ratio = completed / (double)record->frames_requested;
        if (ratio > 1.0) ratio = 1.0;
        return ratio;
    }
    return 0.0;
}
