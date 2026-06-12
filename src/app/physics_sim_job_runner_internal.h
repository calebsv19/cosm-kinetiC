#ifndef PHYSICS_SIM_JOB_RUNNER_INTERNAL_H
#define PHYSICS_SIM_JOB_RUNNER_INTERNAL_H

#include "app/physics_sim_headless_job_bundle.h"
#include "app/physics_sim_job_runner.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

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
    char grid[32];
    char wind_shot_camera_profile[32];
    bool save_volume_frames;
    bool save_render_frames;
    bool save_wind_projection_frames;
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

void diag_set(char *out, size_t out_size, const char *message);
bool copy_string(char *dst, size_t dst_size, const char *src);
bool file_exists(const char *path);
bool dir_is_empty(const char *path);
bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size);
bool ensure_directory_exists(const char *path);
bool ensure_parent_directory_exists(const char *path);
bool read_text_file(const char *path, char **out_text);
bool write_text_file(const char *path, const char *text);
void json_write_string(FILE *file, const char *value);
bool utc_now_string(char *out, size_t out_size);
bool resolve_real_path(const char *path, char *out, size_t out_size);
bool derive_repo_root_from_argv0(const char *argv0, char *out_root, size_t out_root_size);
bool derive_headless_cli_path(const char *argv0, char *out_path, size_t out_path_size);
bool build_jobs_root(const char *argv0,
                     const char *jobs_root_override,
                     char *out_jobs_root,
                     size_t out_jobs_root_size);
void build_job_paths(const char *jobs_root,
                     const char *job_id,
                     PhysicsSimDetachedJobPaths *out_paths);
bool generate_job_id(char *out_job_id, size_t out_job_id_size);
const char *shared_report_state_label(const char *state);
bool build_volume_frames_dir_path(const char *output_root,
                                  char *out_path,
                                  size_t out_path_size);
bool build_render_frames_dir_path(const char *output_root,
                                  char *out_path,
                                  size_t out_path_size);
void detached_job_record_defaults(PhysicsSimDetachedJobRecord *record);
double record_progress_ratio(const PhysicsSimDetachedJobRecord *record);

bool write_job_status_file(const PhysicsSimDetachedJobPaths *paths,
                           const PhysicsSimDetachedJobRecord *record);
bool write_shared_report_file(const PhysicsSimDetachedJobPaths *paths,
                              const PhysicsSimDetachedJobRecord *record);
bool persist_job_state(const PhysicsSimDetachedJobPaths *paths,
                       const PhysicsSimDetachedJobRecord *record);
bool json_get_string(json_object *owner, const char *key, const char **out_value);
bool json_get_int(json_object *owner, const char *key, int *out_value);
bool json_get_bool(json_object *owner, const char *key, bool *out_value);
bool write_pid_file(const char *path, pid_t pid);
bool print_file_to_stream(FILE *out, const char *path);
bool load_job_status_record(const PhysicsSimDetachedJobPaths *paths,
                            PhysicsSimDetachedJobRecord *out_record);
bool merge_progress_into_record(const char *progress_path,
                                PhysicsSimDetachedJobRecord *record);
bool parse_summary_status(const char *summary_path,
                          char *out_status,
                          size_t out_status_size);
bool pid_is_alive(pid_t pid);
bool parse_utc_timestamp(const char *text, time_t *out_time);
bool refresh_job_status_record(const PhysicsSimDetachedJobPaths *paths,
                               PhysicsSimDetachedJobRecord *record);
bool write_cancel_flag_file(const char *path);

#endif
