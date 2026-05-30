#include "app/physics_sim_job_runner_internal.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

bool write_job_status_file(const PhysicsSimDetachedJobPaths *paths,
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

bool write_shared_report_file(const PhysicsSimDetachedJobPaths *paths,
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

bool persist_job_state(const PhysicsSimDetachedJobPaths *paths,
                       const PhysicsSimDetachedJobRecord *record) {
    if (!write_job_status_file(paths, record)) return false;
    if (!write_shared_report_file(paths, record)) return false;
    return true;
}

bool json_get_string(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

bool json_get_int(json_object *owner, const char *key, int *out_value) {
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

bool json_get_bool(json_object *owner, const char *key, bool *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = false;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_boolean)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_boolean(obj) != 0;
    return true;
}

bool write_pid_file(const char *path, pid_t pid) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%ld\n", (long)pid);
    return write_text_file(path, buffer);
}

bool print_file_to_stream(FILE *out, const char *path) {
    char *text = NULL;
    if (!out || !path || !read_text_file(path, &text)) return false;
    fputs(text, out);
    free(text);
    return true;
}

bool load_job_status_record(const PhysicsSimDetachedJobPaths *paths,
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

bool merge_progress_into_record(const char *progress_path,
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

bool parse_summary_status(const char *summary_path,
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

bool pid_is_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    return errno != ESRCH;
}

bool parse_utc_timestamp(const char *text, time_t *out_time) {
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

bool refresh_job_status_record(const PhysicsSimDetachedJobPaths *paths,
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

bool write_cancel_flag_file(const char *path) {
    char timestamp[32];
    if (!path || !path[0]) return false;
    if (!utc_now_string(timestamp, sizeof(timestamp))) {
        snprintf(timestamp, sizeof(timestamp), "cancel_requested");
    }
    return write_text_file(path, timestamp);
}
