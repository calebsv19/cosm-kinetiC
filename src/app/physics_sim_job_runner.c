#include "app/physics_sim_job_runner.h"
#include "app/physics_sim_headless_job_bundle.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

#if defined(__APPLE__) || defined(__unix__)
extern time_t timegm(struct tm *tm);
#endif

typedef struct PhysicsSimDetachedRequest {
    char schema_version[64];
    char runtime_scene_path[PATH_MAX];
    char output_root[PATH_MAX];
    int frames;
    int sim_steps_per_frame;
    int progress_interval;
    bool save_volume_frames;
    bool save_render_frames;
    bool skip_present;
    bool overwrite;
} PhysicsSimDetachedRequest;

typedef struct PhysicsSimDetachedJobPaths {
    char jobs_root[PATH_MAX];
    char job_root[PATH_MAX];
    char job_request_path[PATH_MAX];
    char job_status_path[PATH_MAX];
    char shared_job_path[PATH_MAX];
    char shared_report_path[PATH_MAX];
    char progress_path[PATH_MAX];
    char stdout_log_path[PATH_MAX];
    char stderr_log_path[PATH_MAX];
    char pid_path[PATH_MAX];
    char cancel_request_path[PATH_MAX];
    char result_summary_path[PATH_MAX];
} PhysicsSimDetachedJobPaths;

typedef struct PhysicsSimDetachedJobRecord {
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char state[32];
    char stage[32];
    char overwrite_policy[32];
    char submitted_at_utc[32];
    char started_at_utc[32];
    char updated_at_utc[32];
    char finished_at_utc[32];
    char request_path[PATH_MAX];
    char output_root[PATH_MAX];
    char progress_path[PATH_MAX];
    char summary_path[PATH_MAX];
    char stdout_path[PATH_MAX];
    char stderr_path[PATH_MAX];
    pid_t pid;
    int exit_code;
    int frame_index;
    int frames_requested;
    int frames_completed;
    int sim_steps_per_frame;
    int sim_steps_completed_in_frame;
    int sim_steps_total_in_frame;
    char diagnostics[256];
} PhysicsSimDetachedJobRecord;

#define PHYSICS_SIM_JOB_STALL_TIMEOUT_SECONDS (15 * 60)

static void diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}

static bool copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

static bool file_exists(const char *path) {
    return path && path[0] && access(path, F_OK) == 0;
}

static bool dir_is_empty(const char *path) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!path || !path[0]) return false;
    dir = opendir(path);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        closedir(dir);
        return false;
    }
    closedir(dir);
    return true;
}

static bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return copy_string(out_dir, out_dir_size, ".");
    len = (size_t)(slash - path);
    if (len == 0u) return copy_string(out_dir, out_dir_size, "/");
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return true;
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);
    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
        tmp[i] = '/';
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!parent_dir_of(path, dir, sizeof(dir))) return false;
    return ensure_directory_exists(dir);
}

static bool read_text_file(const char *path, char **out_text) {
    FILE *file = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;
    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

static bool write_text_file(const char *path, const char *text) {
    FILE *file = NULL;
    if (!path || !path[0] || !text) return false;
    if (!ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (fputs(text, file) < 0) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static void json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\': fputs("\\\\", file); break;
            case '"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static bool utc_now_string(char *out, size_t out_size) {
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

static bool resolve_real_path(const char *path, char *out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !path[0] || !out || out_size == 0u) return false;
    if (!realpath(path, resolved)) return false;
    return copy_string(out, out_size, resolved);
}

static bool derive_repo_root_from_argv0(const char *argv0, char *out_root, size_t out_root_size) {
    char binary_path[PATH_MAX];
    char *build_marker = NULL;
    if (!resolve_real_path(argv0, binary_path, sizeof(binary_path))) return false;
    build_marker = strstr(binary_path, "/build/");
    if (!build_marker) return false;
    *build_marker = '\0';
    return copy_string(out_root, out_root_size, binary_path);
}

static bool derive_headless_cli_path(const char *argv0, char *out_path, size_t out_path_size) {
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

static bool build_jobs_root(const char *argv0,
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

static void build_job_paths(const char *jobs_root,
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

static bool generate_job_id(char *out_job_id, size_t out_job_id_size) {
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

static const char *shared_report_state_label(const char *state) {
    if (!state || !state[0]) return "queued";
    if (strcmp(state, "completed") == 0) return "succeeded";
    if (strcmp(state, "starting") == 0) return "running";
    return state;
}

static bool build_volume_frames_dir_path(const char *output_root,
                                         char *out_path,
                                         size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path, out_path_size, "%s/volume_frames", output_root) < (int)out_path_size;
}

static bool build_render_frames_dir_path(const char *output_root,
                                         char *out_path,
                                         size_t out_path_size) {
    if (!output_root || !output_root[0] || !out_path || out_path_size == 0u) return false;
    return snprintf(out_path, out_path_size, "%s/render_frames", output_root) < (int)out_path_size;
}

static void detached_job_record_defaults(PhysicsSimDetachedJobRecord *record) {
    if (!record) return;
    memset(record, 0, sizeof(*record));
    snprintf(record->state, sizeof(record->state), "queued");
    snprintf(record->stage, sizeof(record->stage), "queued");
    snprintf(record->overwrite_policy, sizeof(record->overwrite_policy), "fail_if_exists");
    snprintf(record->diagnostics, sizeof(record->diagnostics), "queued");
    record->pid = 0;
    record->exit_code = -1;
}

static double record_progress_ratio(const PhysicsSimDetachedJobRecord *record) {
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

static bool write_job_status_file(const PhysicsSimDetachedJobPaths *paths,
                                  const PhysicsSimDetachedJobRecord *record) {
    FILE *file = NULL;
    if (!paths || !record) return false;
    if (!ensure_parent_directory_exists(paths->job_status_path)) return false;
    file = fopen(paths->job_status_path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": ");
    json_write_string(file, PHYSICS_SIM_DETACHED_JOB_STATUS_SCHEMA);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": \"physics_sim\",\n");
    fprintf(file, "  \"tool\": \"physics_sim_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    json_write_string(file, record->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    json_write_string(file, record->state);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    json_write_string(file, record->stage);
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    json_write_string(file, record->request_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    json_write_string(file, record->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    json_write_string(file, record->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    json_write_string(file, record->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    json_write_string(file, record->stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    json_write_string(file, record->stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)record->pid);
    fprintf(file, "  \"exit_code\": %d,\n", record->exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    json_write_string(file, record->overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"frames_requested\": %d,\n", record->frames_requested);
    fprintf(file, "  \"frames_completed\": %d,\n", record->frames_completed);
    fprintf(file, "  \"frame_index\": %d,\n", record->frame_index);
    fprintf(file, "  \"sim_steps_per_frame\": %d,\n", record->sim_steps_per_frame);
    fprintf(file, "  \"sim_steps_completed_in_frame\": %d,\n", record->sim_steps_completed_in_frame);
    fprintf(file, "  \"sim_steps_total_in_frame\": %d,\n", record->sim_steps_total_in_frame);
    fprintf(file, "  \"progress_ratio\": %.6f,\n", record_progress_ratio(record));
    fprintf(file, "  \"submitted_at_utc\": ");
    json_write_string(file, record->submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    json_write_string(file, record->started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    json_write_string(file, record->updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    json_write_string(file, record->finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    json_write_string(file, record->diagnostics);
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

static bool write_shared_report_file(const PhysicsSimDetachedJobPaths *paths,
                                     const PhysicsSimDetachedJobRecord *record) {
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[5];
    char volume_frames_dir[PATH_MAX];
    char render_frames_dir[PATH_MAX];
    size_t artifact_count = 0u;
    char diagnostics[256];

    if (!paths || !record) return false;

    core_headless_job_report_init(&report);
    if (!copy_string(report.job_id, sizeof(report.job_id), record->job_id) ||
        !copy_string(report.program, sizeof(report.program), "physics_sim") ||
        !copy_string(report.state, sizeof(report.state), shared_report_state_label(record->state)) ||
        !copy_string(report.stage, sizeof(report.stage), record->stage) ||
        !copy_string(report.created_at, sizeof(report.created_at), record->submitted_at_utc) ||
        !copy_string(report.started_at, sizeof(report.started_at), record->started_at_utc) ||
        !copy_string(report.updated_at, sizeof(report.updated_at), record->updated_at_utc) ||
        !copy_string(report.finished_at, sizeof(report.finished_at), record->finished_at_utc)) {
        return false;
    }

    for (size_t i = 0u; i < 5u; ++i) {
        core_headless_job_artifact_init(&artifacts[i]);
    }
    if (record->summary_path[0]) {
        copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "result_summary");
        copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->summary_path);
        artifact_count += 1u;
    }
    if (build_volume_frames_dir_path(record->output_root, volume_frames_dir, sizeof(volume_frames_dir)) &&
        file_exists(volume_frames_dir)) {
        copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "volume_frames");
        copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), volume_frames_dir);
        artifact_count += 1u;
    }
    if (build_render_frames_dir_path(record->output_root, render_frames_dir, sizeof(render_frames_dir)) &&
        file_exists(render_frames_dir)) {
        copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "render_frames");
        copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), render_frames_dir);
        artifact_count += 1u;
    }
    if (record->stdout_path[0]) {
        copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "stdout_log");
        copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->stdout_path);
        artifact_count += 1u;
    }
    if (record->stderr_path[0]) {
        copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "stderr_log");
        copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->stderr_path);
        artifact_count += 1u;
    }

    report.artifacts = artifacts;
    report.artifact_count = artifact_count;
    return physics_sim_headless_job_report_write(paths->shared_report_path,
                                                 &report,
                                                 artifacts,
                                                 artifact_count,
                                                 diagnostics,
                                                 sizeof(diagnostics));
}

static bool persist_job_state(const PhysicsSimDetachedJobPaths *paths,
                              const PhysicsSimDetachedJobRecord *record) {
    if (!write_job_status_file(paths, record)) return false;
    if (!write_shared_report_file(paths, record)) return false;
    return true;
}

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

    if (out_pid) *out_pid = 0;
    if (!headless_cli_path || !paths || !request) return false;
    if (!ensure_directory_exists(paths->job_root)) return false;

    snprintf(frames_arg, sizeof(frames_arg), "%d", request->frames);
    snprintf(sim_steps_arg, sizeof(sim_steps_arg), "%d", request->sim_steps_per_frame);
    snprintf(progress_interval_arg, sizeof(progress_interval_arg), "%d", request->progress_interval);

    pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        char *argv[24];
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
        if (request->save_volume_frames) argv[argc++] = (char *)"--save-volume-frames";
        if (request->save_render_frames) argv[argc++] = (char *)"--save-render-frames";
        argv[argc++] = (char *)(request->skip_present ? "--skip-present" : "--present");
        if (request->overwrite) argv[argc++] = (char *)"--overwrite";
        argv[argc] = NULL;

        execv(headless_cli_path, argv);
        _exit(127);
    }
    if (out_pid) *out_pid = pid;
    return true;
}

static bool json_get_string(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

static bool json_get_int(json_object *owner, const char *key, int *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_int(obj);
    return true;
}

static bool json_get_bool(json_object *owner, const char *key, bool *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = false;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_boolean)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_boolean(obj) != 0;
    return true;
}

static bool load_request_file(const char *request_path,
                              PhysicsSimDetachedRequest *out_request,
                              char *out_diagnostics,
                              size_t out_diagnostics_size) {
    json_object *root = NULL;
    const char *text_value = NULL;
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
    (void)json_get_bool(root, "save_volume_frames", &out_request->save_volume_frames);
    (void)json_get_bool(root, "save_render_frames", &out_request->save_render_frames);
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
    fprintf(file, "  \"save_volume_frames\": %s,\n", request->save_volume_frames ? "true" : "false");
    fprintf(file, "  \"save_render_frames\": %s,\n", request->save_render_frames ? "true" : "false");
    fprintf(file, "  \"skip_present\": %s,\n", request->skip_present ? "true" : "false");
    fprintf(file, "  \"overwrite\": %s\n", request->overwrite ? "true" : "false");
    fprintf(file, "}\n");
    fclose(file);
    return true;
}

static bool write_pid_file(const char *path, pid_t pid) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%ld\n", (long)pid);
    return write_text_file(path, buffer);
}

static bool print_file_to_stream(FILE *out, const char *path) {
    char *text = NULL;
    if (!out || !path || !read_text_file(path, &text)) return false;
    fputs(text, out);
    free(text);
    return true;
}

static bool load_job_status_record(const PhysicsSimDetachedJobPaths *paths,
                                   PhysicsSimDetachedJobRecord *out_record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!paths || !out_record) return false;
    detached_job_record_defaults(out_record);
    root = json_object_from_file(paths->job_status_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    if (json_get_string(root, "job_id", &text_value)) {
        copy_string(out_record->job_id, sizeof(out_record->job_id), text_value);
    }
    if (json_get_string(root, "state", &text_value)) {
        copy_string(out_record->state, sizeof(out_record->state), text_value);
    }
    if (json_get_string(root, "stage", &text_value)) {
        copy_string(out_record->stage, sizeof(out_record->stage), text_value);
    }
    if (json_get_string(root, "overwrite_policy", &text_value)) {
        copy_string(out_record->overwrite_policy, sizeof(out_record->overwrite_policy), text_value);
    }
    if (json_get_string(root, "request_path", &text_value)) {
        copy_string(out_record->request_path, sizeof(out_record->request_path), text_value);
    }
    if (json_get_string(root, "output_root", &text_value)) {
        copy_string(out_record->output_root, sizeof(out_record->output_root), text_value);
    }
    if (json_get_string(root, "progress_path", &text_value)) {
        copy_string(out_record->progress_path, sizeof(out_record->progress_path), text_value);
    }
    if (json_get_string(root, "summary_path", &text_value)) {
        copy_string(out_record->summary_path, sizeof(out_record->summary_path), text_value);
    }
    if (json_get_string(root, "stdout_path", &text_value)) {
        copy_string(out_record->stdout_path, sizeof(out_record->stdout_path), text_value);
    }
    if (json_get_string(root, "stderr_path", &text_value)) {
        copy_string(out_record->stderr_path, sizeof(out_record->stderr_path), text_value);
    }
    if (json_get_string(root, "submitted_at_utc", &text_value)) {
        copy_string(out_record->submitted_at_utc, sizeof(out_record->submitted_at_utc), text_value);
    }
    if (json_get_string(root, "started_at_utc", &text_value)) {
        copy_string(out_record->started_at_utc, sizeof(out_record->started_at_utc), text_value);
    }
    if (json_get_string(root, "updated_at_utc", &text_value)) {
        copy_string(out_record->updated_at_utc, sizeof(out_record->updated_at_utc), text_value);
    }
    if (json_get_string(root, "finished_at_utc", &text_value)) {
        copy_string(out_record->finished_at_utc, sizeof(out_record->finished_at_utc), text_value);
    }
    if (json_get_string(root, "diagnostics", &text_value)) {
        copy_string(out_record->diagnostics, sizeof(out_record->diagnostics), text_value);
    }
    if (json_get_int(root, "pid", &int_value)) out_record->pid = (pid_t)int_value;
    if (json_get_int(root, "exit_code", &int_value)) out_record->exit_code = int_value;
    if (json_get_int(root, "frames_requested", &int_value)) out_record->frames_requested = int_value;
    if (json_get_int(root, "frames_completed", &int_value)) out_record->frames_completed = int_value;
    if (json_get_int(root, "frame_index", &int_value)) out_record->frame_index = int_value;
    if (json_get_int(root, "sim_steps_per_frame", &int_value)) out_record->sim_steps_per_frame = int_value;
    if (json_get_int(root, "sim_steps_completed_in_frame", &int_value)) {
        out_record->sim_steps_completed_in_frame = int_value;
    }
    if (json_get_int(root, "sim_steps_total_in_frame", &int_value)) {
        out_record->sim_steps_total_in_frame = int_value;
    }
    json_object_put(root);
    return true;
}

static bool merge_progress_into_record(const char *progress_path,
                                       PhysicsSimDetachedJobRecord *record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!progress_path || !progress_path[0] || !record || !file_exists(progress_path)) return false;
    root = json_object_from_file(progress_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    if (json_get_string(root, "stage", &text_value)) {
        copy_string(record->stage, sizeof(record->stage), text_value);
    }
    if (json_get_string(root, "updated_at_utc", &text_value)) {
        copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), text_value);
        if (record->started_at_utc[0] == '\0' &&
            strcmp(record->state, "running") == 0) {
            copy_string(record->started_at_utc, sizeof(record->started_at_utc), text_value);
        }
    }
    if (json_get_string(root, "status", &text_value)) {
        if (strcmp(text_value, "pending") == 0) {
            snprintf(record->state, sizeof(record->state), "starting");
        } else if (strcmp(text_value, "running") == 0 || strcmp(text_value, "finishing") == 0) {
            snprintf(record->state, sizeof(record->state), "running");
        } else if (strcmp(text_value, "canceled") == 0) {
            snprintf(record->state, sizeof(record->state), "cancelled");
        } else if (strcmp(text_value, "passed") == 0) {
            snprintf(record->state, sizeof(record->state), "completed");
        } else if (strcmp(text_value, "failed") == 0) {
            snprintf(record->state, sizeof(record->state), "failed");
        }
    }
    if (json_get_int(root, "frame_index", &int_value)) record->frame_index = int_value;
    if (json_get_int(root, "frames_completed", &int_value)) record->frames_completed = int_value;
    if (json_get_int(root, "frames_requested", &int_value)) record->frames_requested = int_value;
    if (json_get_int(root, "sim_steps_per_frame", &int_value)) record->sim_steps_per_frame = int_value;
    if (json_get_int(root, "sim_steps_completed_in_frame", &int_value)) {
        record->sim_steps_completed_in_frame = int_value;
    }
    if (json_get_int(root, "sim_steps_total_in_frame", &int_value)) {
        record->sim_steps_total_in_frame = int_value;
    }
    json_object_put(root);
    return true;
}

static bool parse_summary_status(const char *summary_path,
                                 char *out_status,
                                 size_t out_status_size) {
    json_object *root = NULL;
    const char *status_value = NULL;
    if (!summary_path || !summary_path[0] || !out_status || out_status_size == 0u) return false;
    out_status[0] = '\0';
    if (!file_exists(summary_path)) return false;
    root = json_object_from_file(summary_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    if (!json_get_string(root, "status", &status_value)) {
        json_object_put(root);
        return false;
    }
    copy_string(out_status, out_status_size, status_value);
    json_object_put(root);
    return true;
}

static bool pid_is_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    return errno != ESRCH;
}

static bool parse_utc_timestamp(const char *text, time_t *out_time) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    struct tm tm_value;
    time_t value = (time_t)-1;
    if (out_time) *out_time = (time_t)-1;
    if (!text || !text[0] || !out_time) return false;
    if (sscanf(text,
               "%d-%d-%dT%d:%d:%dZ",
               &year,
               &month,
               &day,
               &hour,
               &minute,
               &second) != 6) {
        return false;
    }
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
#if defined(__APPLE__) || defined(__unix__)
    value = timegm(&tm_value);
#else
    value = mktime(&tm_value);
#endif
    if (value == (time_t)-1) return false;
    *out_time = value;
    return true;
}

static bool refresh_job_status_record(const PhysicsSimDetachedJobPaths *paths,
                                      PhysicsSimDetachedJobRecord *record) {
    char now_utc[32] = {0};
    char summary_status[32] = {0};
    time_t now_time = (time_t)-1;
    time_t updated_time = (time_t)-1;
    bool changed = false;
    bool alive = false;
    if (!paths || !record) return false;
    if (merge_progress_into_record(record->progress_path, record)) {
        changed = true;
    }
    alive = pid_is_alive(record->pid);
    if ((strcmp(record->state, "starting") == 0 ||
         strcmp(record->state, "running") == 0 ||
         strcmp(record->state, "stalled") == 0) &&
        !alive) {
        utc_now_string(now_utc, sizeof(now_utc));
        if (parse_summary_status(record->summary_path, summary_status, sizeof(summary_status))) {
            if (strcmp(summary_status, "passed") == 0) {
                snprintf(record->state, sizeof(record->state), "completed");
                snprintf(record->stage, sizeof(record->stage), "completed");
                record->exit_code = 0;
            } else if (strcmp(summary_status, "canceled") == 0) {
                snprintf(record->state, sizeof(record->state), "cancelled");
                snprintf(record->stage, sizeof(record->stage), "cancelled");
                record->exit_code = 2;
            } else {
                snprintf(record->state, sizeof(record->state), "failed");
                snprintf(record->stage, sizeof(record->stage), "failed");
                if (record->exit_code < 0) record->exit_code = 1;
            }
            if (record->finished_at_utc[0] == '\0') {
                copy_string(record->finished_at_utc, sizeof(record->finished_at_utc), now_utc);
            }
            if (record->updated_at_utc[0] == '\0') {
                copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), now_utc);
            }
        } else {
            snprintf(record->state, sizeof(record->state), "failed");
            if (record->stage[0] == '\0' || strcmp(record->stage, "starting") == 0) {
                snprintf(record->stage, sizeof(record->stage), "failed");
            }
            if (record->exit_code < 0) record->exit_code = 1;
            copy_string(record->finished_at_utc, sizeof(record->finished_at_utc), now_utc);
            copy_string(record->updated_at_utc, sizeof(record->updated_at_utc), now_utc);
            if (record->diagnostics[0] == '\0') {
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "process exited without completion summary");
            }
        }
        changed = true;
    }
    if ((strcmp(record->state, "completed") == 0 ||
         strcmp(record->state, "cancelled") == 0 ||
         strcmp(record->state, "failed") == 0) &&
        parse_summary_status(record->summary_path, summary_status, sizeof(summary_status))) {
        if (record->finished_at_utc[0] == '\0' && record->updated_at_utc[0] != '\0') {
            copy_string(record->finished_at_utc,
                        sizeof(record->finished_at_utc),
                        record->updated_at_utc);
            changed = true;
        }
        if (strcmp(summary_status, "passed") == 0 && record->exit_code < 0) {
            record->exit_code = 0;
            changed = true;
        } else if (strcmp(summary_status, "canceled") == 0 && record->exit_code < 0) {
            record->exit_code = 2;
            changed = true;
        } else if (strcmp(summary_status, "failed") == 0 && record->exit_code < 0) {
            record->exit_code = 1;
            changed = true;
        }
        if (record->diagnostics[0] == '\0' ||
            strcmp(record->diagnostics, "detached simulation launched") == 0) {
            snprintf(record->diagnostics,
                     sizeof(record->diagnostics),
                     "%s",
                     strcmp(summary_status, "passed") == 0
                         ? "simulation completed"
                         : (strcmp(summary_status, "canceled") == 0
                                ? "simulation canceled"
                                : "simulation failed"));
            changed = true;
        }
    }
    if ((strcmp(record->state, "starting") == 0 ||
         strcmp(record->state, "running") == 0 ||
         strcmp(record->state, "stalled") == 0) &&
        alive &&
        utc_now_string(now_utc, sizeof(now_utc)) &&
        parse_utc_timestamp(now_utc, &now_time) &&
        parse_utc_timestamp(record->updated_at_utc, &updated_time)) {
        const double stale_seconds = difftime(now_time, updated_time);
        if (stale_seconds >= (double)PHYSICS_SIM_JOB_STALL_TIMEOUT_SECONDS) {
            if (strcmp(record->state, "stalled") != 0) {
                snprintf(record->state, sizeof(record->state), "stalled");
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "no progress update for %.0f seconds while process remained alive",
                         stale_seconds);
                changed = true;
            }
        } else if (strcmp(record->state, "stalled") == 0) {
            snprintf(record->state, sizeof(record->state), "running");
            if (record->diagnostics[0] == '\0' ||
                strstr(record->diagnostics, "no progress update for") != NULL) {
                snprintf(record->diagnostics,
                         sizeof(record->diagnostics),
                         "simulation resumed within stall threshold");
            }
            changed = true;
        }
    }
    if (changed) {
        if (!persist_job_state(paths, record)) return false;
    }
    return true;
}

static bool write_cancel_flag_file(const char *path) {
    char timestamp[32];
    if (!path || !path[0]) return false;
    if (!utc_now_string(timestamp, sizeof(timestamp))) {
        snprintf(timestamp, sizeof(timestamp), "cancel_requested");
    }
    return write_text_file(path, timestamp);
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
