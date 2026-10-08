#include "app/physics_sim_job_file.h"
#include "app/physics_sim_headless_output.h"
#include "app/physics_sim_job_json.h"
#include "app/physics_sim_job_runner_internal.h"

#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>

static bool serialize_job_status(FILE *file, const PhysicsSimDetachedJobRecord *record) {
    if (!file || !record) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"artifact_class\": \"operational_job\",\n");
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
    return !ferror(file);
}

bool write_job_status_file(const PhysicsSimDetachedJobPaths *paths,
                           const PhysicsSimDetachedJobRecord *record) {
    FILE *file = NULL;
    PhysicsSimJobFile publication;
    if (!paths || !record) return false;
    if (!ensure_parent_directory_exists(paths->job_status_path)) return false;
    file = physics_sim_job_file_begin(paths->job_status_path, true, &publication);
    if (!file) return false;
    if (!serialize_job_status(file, record)) {
        physics_sim_job_file_discard(&publication, file);
        return false;
    }
    if (!physics_sim_job_file_finish(&publication, file)) return false;
    return true;
}

static bool build_shared_report(const PhysicsSimDetachedJobRecord *record,
                                CoreHeadlessJobReport *report,
                                CoreHeadlessJobArtifact artifacts[5]) {
    char volume_frames_dir[PATH_MAX];
    char render_frames_dir[PATH_MAX];
    size_t artifact_count = 0u;

    if (!record || !report || !artifacts) return false;

    core_headless_job_report_init(report);
    if (!copy_string(report->job_id, sizeof(report->job_id), record->job_id) ||
        !copy_string(report->program, sizeof(report->program), "physics_sim") ||
        !copy_string(report->state, sizeof(report->state), shared_report_state_label(record->state)) ||
        !copy_string(report->stage, sizeof(report->stage), record->stage) ||
        !copy_string(report->created_at, sizeof(report->created_at), record->submitted_at_utc) ||
        !copy_string(report->started_at, sizeof(report->started_at), record->started_at_utc) ||
        !copy_string(report->updated_at, sizeof(report->updated_at), record->updated_at_utc) ||
        !copy_string(report->finished_at, sizeof(report->finished_at), record->finished_at_utc)) {
        return false;
    }

    for (size_t i = 0u; i < 5u; ++i) {
        core_headless_job_artifact_init(&artifacts[i]);
    }
    if (record->summary_path[0]) {
        if (!copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "result_summary")) return false;
        if (!copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->summary_path)) return false;
        artifact_count += 1u;
    }
    if (build_volume_frames_dir_path(record->output_root, volume_frames_dir, sizeof(volume_frames_dir)) &&
        file_exists(volume_frames_dir)) {
        if (!copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "volume_frames")) return false;
        if (!copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), volume_frames_dir)) return false;
        artifact_count += 1u;
    }
    if (build_render_frames_dir_path(record->output_root, render_frames_dir, sizeof(render_frames_dir)) &&
        file_exists(render_frames_dir)) {
        if (!copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "render_frames")) return false;
        if (!copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), render_frames_dir)) return false;
        artifact_count += 1u;
    }
    if (record->stdout_path[0]) {
        if (!copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "stdout_log")) return false;
        if (!copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->stdout_path)) return false;
        artifact_count += 1u;
    }
    if (record->stderr_path[0]) {
        if (!copy_string(artifacts[artifact_count].type, sizeof(artifacts[artifact_count].type), "stderr_log")) return false;
        if (!copy_string(artifacts[artifact_count].path, sizeof(artifacts[artifact_count].path), record->stderr_path)) return false;
        artifact_count += 1u;
    }

    report->artifacts = artifacts;
    report->artifact_count = artifact_count;
    return core_headless_job_report_validate(report);
}

static bool publish_shared_report(const PhysicsSimDetachedJobPaths *paths,
                                  const CoreHeadlessJobReport *report,
                                  const CoreHeadlessJobArtifact artifacts[5]) {
    char diagnostics[256];
    return physics_sim_headless_job_report_write(paths->shared_report_path, report,
        artifacts, report->artifact_count, diagnostics, sizeof(diagnostics));
}

bool write_shared_report_file(const PhysicsSimDetachedJobPaths *paths,
                              const PhysicsSimDetachedJobRecord *record) {
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[5];
    return paths && build_shared_report(record, &report, artifacts) && publish_shared_report(paths, &report, artifacts);
}

/* Existing regular reports must satisfy the same bounded object admission as
 * their single-file writer, even when refresh has no new state to publish.
 * Missing legacy reports remain permitted; this is not pair-generation proof. */
static bool admitted_report_predecessor(const PhysicsSimDetachedJobPaths *paths) {
    struct stat predecessor;
    if (!paths) return false;
    if (lstat(paths->shared_report_path, &predecessor) != 0) return errno == ENOENT;
    json_object *report = physics_sim_job_json_read(paths->shared_report_path);
    if (!report) return false;
    json_object_put(report);
    return true;
}

/* Reader admission binds a published report to the saved predecessor, before
 * progress/summary merge. Optional output directories may grow after publication;
 * their roles are optional but their paths, when recorded, are fixed. */
static bool report_string(json_object *object, const char *key,
                          char *destination, size_t capacity, const char *expected) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) return false;
    const char *text = json_object_get_string(value);
    if (!text || strlen(text) != (size_t)json_object_get_string_len(value) ||
        !copy_string(destination, capacity, text)) return false;
    return !expected || strcmp(destination, expected) == 0;
}

static bool admitted_current_report(const PhysicsSimDetachedJobPaths *paths,
                                    const PhysicsSimDetachedJobRecord *record) {
    struct stat predecessor;
    if (!paths || !record) return false;
    if (lstat(paths->shared_report_path, &predecessor) != 0) return errno == ENOENT;
    json_object *object = physics_sim_job_json_read(paths->shared_report_path);
    if (!object) return false;
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[5];
    CoreHeadlessJobReport defaults;
    core_headless_job_report_init(&report);
    core_headless_job_report_init(&defaults);
    struct {
        const char *key;
        char *destination;
        size_t capacity;
        const char *expected;
    } fields[] = {
        {"schema_family", report.schema_family, sizeof(report.schema_family), defaults.schema_family},
        {"schema_variant", report.schema_variant, sizeof(report.schema_variant), defaults.schema_variant},
        {"job_id", report.job_id, sizeof(report.job_id), record->job_id},
        {"program", report.program, sizeof(report.program), "physics_sim"},
        {"state", report.state, sizeof(report.state), shared_report_state_label(record->state)},
        {"stage", report.stage, sizeof(report.stage), record->stage},
        {"created_at", report.created_at, sizeof(report.created_at), record->submitted_at_utc},
        {"started_at", report.started_at, sizeof(report.started_at), record->started_at_utc},
        {"updated_at", report.updated_at, sizeof(report.updated_at), record->updated_at_utc},
        {"finished_at", report.finished_at, sizeof(report.finished_at), record->finished_at_utc}
    };
    bool admitted = json_object_object_length(object) == 11;
    for (size_t i = 0u; admitted && i < sizeof(fields) / sizeof(fields[0]); ++i) {
        admitted = report_string(object, fields[i].key, fields[i].destination,
                                 fields[i].capacity, fields[i].expected);
    }
    json_object *array = NULL;
    if (admitted) {
        admitted = json_object_object_get_ex(object, "artifacts", &array) &&
            json_object_is_type(array, json_type_array) &&
            json_object_array_length(array) >= 3u && json_object_array_length(array) <= 5u;
    }
    char volume[PATH_MAX], render[PATH_MAX];
    if (admitted) {
        admitted = build_volume_frames_dir_path(record->output_root, volume, sizeof(volume)) &&
            build_render_frames_dir_path(record->output_root, render, sizeof(render));
    }
    const char *roles[5] = {"result_summary", "stdout_log", "stderr_log", "volume_frames", "render_frames"};
    const char *expected_paths[5] = {record->summary_path, record->stdout_path, record->stderr_path, volume, render};
    unsigned int seen = 0u;
    size_t count = admitted ? json_object_array_length(array) : 0u;
    for (size_t i = 0u; admitted && i < count; ++i) {
        json_object *artifact = json_object_array_get_idx(array, i);
        core_headless_job_artifact_init(&artifacts[i]);
        admitted = json_object_is_type(artifact, json_type_object) &&
            json_object_object_length(artifact) == 2 &&
            report_string(artifact, "type", artifacts[i].type, sizeof(artifacts[i].type), NULL) &&
            report_string(artifact, "path", artifacts[i].path, sizeof(artifacts[i].path), NULL);
        size_t role = 0u;
        while (admitted && role < 5u && strcmp(artifacts[i].type, roles[role]) != 0) ++role;
        if (admitted) {
            admitted = role < 5u && !(seen & (1u << role)) &&
                strcmp(artifacts[i].path, expected_paths[role]) == 0;
            if (admitted) seen |= 1u << role;
        }
    }
    report.artifacts = artifacts;
    report.artifact_count = count;
    admitted = admitted && (seen & 7u) == 7u && core_headless_job_report_validate(&report);
    json_object_put(object);
    return admitted;
}

bool persist_job_state(const PhysicsSimDetachedJobPaths *paths,
                       const PhysicsSimDetachedJobRecord *record) {
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[5];
    /* Admit the entire shared representation before the first status mutation.
     * Retained pair publication follows below; explicit recovery remains required. */
    if (!paths || !build_shared_report(record, &report, artifacts) ||
        !admitted_report_predecessor(paths)) return false;
    if (!ensure_parent_directory_exists(paths->job_status_path) ||
        !ensure_parent_directory_exists(paths->shared_report_path)) return false;
    PhysicsSimJobFile status_file, report_file;
    FILE *status = physics_sim_job_file_begin(paths->job_status_path, true, &status_file);
    if (!status) return false;
    if (!serialize_job_status(status, record)) {
        physics_sim_job_file_discard(&status_file, status);
        return false;
    }
    if (!physics_sim_job_file_prepare(&status_file, status)) return false;
    FILE *shared = physics_sim_job_file_begin(paths->shared_report_path, true, &report_file);
    if (!shared) {
        physics_sim_job_file_discard(&status_file, status);
        return false;
    }
    if (!physics_sim_headless_job_report_serialize(shared, &report, artifacts, report.artifact_count)) {
        physics_sim_job_file_discard(&report_file, shared);
        physics_sim_job_file_discard(&status_file, status);
        return false;
    }
    if (!physics_sim_job_file_prepare(&report_file, shared)) {
        physics_sim_job_file_discard(&status_file, status);
        return false;
    }
    /* Both complete stages and predecessors remain owned through this check.
     * Retained pair publication takes ownership below; explicit recovery is required. */
    if (!physics_sim_job_file_ready(&status_file, status) ||
        !physics_sim_job_file_ready(&report_file, shared)) {
        physics_sim_job_file_discard(&report_file, shared);
        physics_sim_job_file_discard(&status_file, status);
        return false;
    }
    return physics_sim_job_file_publish_pair(&status_file, status, &report_file, shared);
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
    return physics_sim_job_json_integer(owner, key, out_value);
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
    if (!out || !path) return false;
    json_object *object = physics_sim_job_json_read(path);
    if (!object) return false;
    const char *text = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_NOSLASHESCAPE);
    bool valid = text && fputs(text, out) >= 0 && fputc('\n', out) != EOF;
    json_object_put(object);
    return valid;
}

static bool admitted_tag(json_object *root, const char *key, const char *expected) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(root, key, &value)) return true;
    return json_object_is_type(value, json_type_string) &&
        strcmp(json_object_get_string(value), expected) == 0;
}

static bool admitted_record_counters(const PhysicsSimDetachedJobRecord *record) {
    return record->frames_completed <= record->frames_requested &&
        (record->frames_requested > 0 ?
            (record->frame_index < record->frames_requested ||
             (record->frame_index == record->frames_requested &&
              record->frames_completed == record->frames_requested &&
              record->sim_steps_total_in_frame == 0 &&
              (strcmp(record->state, "cancelled") == 0 || strcmp(record->state, "failed") == 0))) :
            record->frame_index == 0) &&
        record->sim_steps_completed_in_frame <= record->sim_steps_total_in_frame &&
        record->sim_steps_total_in_frame <= record->sim_steps_per_frame &&
        (strcmp(record->state, "completed") != 0 || record->frames_completed == record->frames_requested);
}

static bool admitted_job_state(const char *state) {
    const char *states[] = {"queued", "starting", "running", "stalled", "completed", "cancelled", "failed"};
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        if (strcmp(state, states[i]) == 0) return true;
    }
    return false;
}

static bool admitted_timestamp_fields(json_object *root) {
    const char *keys[] = {"submitted_at_utc", "started_at_utc", "updated_at_utc", "finished_at_utc"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const char *text = NULL;
        time_t value;
        if (json_get_string(root, keys[i], &text) && text[0] &&
            !parse_utc_timestamp(text, &value)) return false;
    }
    return true;
}

bool load_job_status_record(const PhysicsSimDetachedJobPaths *paths,
                            PhysicsSimDetachedJobRecord *out_record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!paths || !out_record) return false;
    detached_job_record_defaults(out_record);
    root = physics_sim_job_json_read(paths->job_status_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    const PhysicsSimJobJsonField fields[] = {
        {"job_id", PHYSICS_JOB_STRING, 0, sizeof(out_record->job_id) - 1},
        {"state", PHYSICS_JOB_STRING, 0, sizeof(out_record->state) - 1},
        {"stage", PHYSICS_JOB_STRING, 0, sizeof(out_record->stage) - 1},
        {"overwrite_policy", PHYSICS_JOB_STRING, 0, sizeof(out_record->overwrite_policy) - 1},
        {"request_path", PHYSICS_JOB_STRING, 0, sizeof(out_record->request_path) - 1},
        {"output_root", PHYSICS_JOB_STRING, 0, sizeof(out_record->output_root) - 1},
        {"progress_path", PHYSICS_JOB_STRING, 0, sizeof(out_record->progress_path) - 1},
        {"summary_path", PHYSICS_JOB_STRING, 0, sizeof(out_record->summary_path) - 1},
        {"stdout_path", PHYSICS_JOB_STRING, 0, sizeof(out_record->stdout_path) - 1},
        {"stderr_path", PHYSICS_JOB_STRING, 0, sizeof(out_record->stderr_path) - 1},
        {"submitted_at_utc", PHYSICS_JOB_STRING, 0, sizeof(out_record->submitted_at_utc) - 1},
        {"started_at_utc", PHYSICS_JOB_STRING, 0, sizeof(out_record->started_at_utc) - 1},
        {"updated_at_utc", PHYSICS_JOB_STRING, 0, sizeof(out_record->updated_at_utc) - 1},
        {"finished_at_utc", PHYSICS_JOB_STRING, 0, sizeof(out_record->finished_at_utc) - 1},
        {"diagnostics", PHYSICS_JOB_STRING, 0, sizeof(out_record->diagnostics) - 1},
        {"pid", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frames_requested", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frames_completed", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frame_index", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_per_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_completed_in_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_total_in_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"exit_code", PHYSICS_JOB_INT, -1, 255}
    };
    if (!physics_sim_job_json_fields(root, fields, sizeof(fields)/sizeof(fields[0]))) { json_object_put(root); return false; }
    if (!admitted_timestamp_fields(root) ||
        (json_get_string(root, "state", &text_value) && !admitted_job_state(text_value))) {
        json_object_put(root);
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
    if (!admitted_record_counters(out_record) ||
        !admitted_tag(root, "schema_version", PHYSICS_SIM_DETACHED_JOB_STATUS_SCHEMA) ||
        !admitted_tag(root, "program", "physics_sim") ||
        !admitted_tag(root, "tool", "physics_sim_headless") ||
        !admitted_tag(root, "artifact_class", "operational_job") ||
        !admitted_tag(root, "request_path", paths->job_request_path) ||
        (strcmp(out_record->overwrite_policy, "overwrite") != 0 &&
         strcmp(out_record->overwrite_policy, "fail_if_exists") != 0)) {
        json_object_put(root);
        return false;
    }
    const char *selected_id = strrchr(paths->job_root, '/');
    bool bound = selected_id && strcmp(out_record->job_id, selected_id + 1) == 0 &&
        strcmp(out_record->progress_path, paths->progress_path) == 0 &&
        strcmp(out_record->summary_path, paths->result_summary_path) == 0 &&
        strcmp(out_record->stdout_path, paths->stdout_log_path) == 0 &&
        strcmp(out_record->stderr_path, paths->stderr_log_path) == 0;
    json_object_put(root);
    return bound;
}

bool detached_job_state_for_headless_progress_status(const char *progress_status,
                                                     char *out_state,
                                                     size_t out_state_size) {
    const char *state = NULL;
    if (!progress_status || !progress_status[0] || !out_state || out_state_size == 0u) {
        return false;
    }
    if (strcmp(progress_status, "pending") == 0) {
        state = "starting";
    } else if (strcmp(progress_status, "running") == 0 ||
               strcmp(progress_status, "finishing") == 0) {
        state = "running";
    } else if (strcmp(progress_status, "canceled") == 0) {
        state = "cancelled";
    } else if (strcmp(progress_status, "passed") == 0) {
        state = "completed";
    } else if (strcmp(progress_status, "failed") == 0) {
        state = "failed";
    } else {
        return false;
    }
    return copy_string(out_state, out_state_size, state);
}

bool merge_progress_into_record(const char *progress_path,
                                PhysicsSimDetachedJobRecord *record) {
    json_object *root = NULL;
    const char *text_value = NULL;
    int int_value = 0;
    if (!progress_path || !progress_path[0] || !record || !file_exists(progress_path)) return false;
    PhysicsSimDetachedJobRecord *destination = record;
    PhysicsSimDetachedJobRecord candidate = *record;
    record = &candidate;
    root = physics_sim_job_json_observe(progress_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    const PhysicsSimJobJsonField fields[] = {
        {"frames_requested", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frames_completed", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frame_index", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_per_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_completed_in_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_total_in_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"status", PHYSICS_JOB_STRING, 0, 31},
        {"stage", PHYSICS_JOB_STRING, 0, sizeof(record->stage)-1},
        {"updated_at_utc", PHYSICS_JOB_STRING, 0, sizeof(record->updated_at_utc)-1}
    };
    if (!physics_sim_job_json_fields(root, fields, sizeof(fields)/sizeof(fields[0]))) { json_object_put(root); return false; }
    if (!admitted_timestamp_fields(root)) { json_object_put(root); return false; }
    int declared;
    if (!admitted_tag(root, "schema", "physics_sim_headless_run_progress_v2") ||
        !admitted_tag(root, "artifact_class", "operational_job") ||
        !admitted_tag(root, "output_root", record->output_root) ||
        (json_get_int(root, "frames_requested", &declared) && declared != record->frames_requested) ||
        (json_get_int(root, "sim_steps_per_frame", &declared) && declared != record->sim_steps_per_frame)) {
        json_object_put(root);
        return false;
    }
    if (json_get_string(root, "status", &text_value)) {
        char admitted_state[32];
        if (!detached_job_state_for_headless_progress_status(text_value, admitted_state, sizeof(admitted_state))) { json_object_put(root); return false; }
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
        (void)detached_job_state_for_headless_progress_status(text_value,
                                                              record->state,
                                                              sizeof(record->state));
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
    if (!admitted_record_counters(record)) return false;
    *destination = candidate;
    return true;
}

static bool read_summary_observation(const PhysicsSimDetachedJobRecord *record,
                                     char *out_status, size_t out_status_size,
                                     int *out_completed) {
    json_object *root = physics_sim_job_json_observe(record->summary_path);
    if (!root) return false;
    const PhysicsSimJobJsonField fields[] = {
        {"status", PHYSICS_JOB_STRING, 0, 31},
        {"frames_requested", PHYSICS_JOB_INT, 0, INT_MAX},
        {"frames_completed", PHYSICS_JOB_INT, 0, INT_MAX},
        {"sim_steps_per_frame", PHYSICS_JOB_INT, 0, INT_MAX},
        {"result_code", PHYSICS_JOB_INT, 0, 255}
    };
    const char *status = NULL;
    int number, completed = record->frames_completed;
    bool valid = physics_sim_job_json_fields(root, fields, sizeof(fields)/sizeof(fields[0])) &&
        json_get_string(root, "status", &status) &&
        (strcmp(status, "passed") == 0 || strcmp(status, "canceled") == 0 || strcmp(status, "failed") == 0) &&
        admitted_tag(root, "schema", "physics_sim_headless_run_summary_v1") &&
        admitted_tag(root, "artifact_class", "operational_job") &&
        admitted_tag(root, "output_root", record->output_root);
    if (valid && json_get_int(root, "frames_requested", &number)) valid = number == record->frames_requested;
    if (valid && json_get_int(root, "sim_steps_per_frame", &number)) valid = number == record->sim_steps_per_frame;
    if (valid && json_get_int(root, "frames_completed", &number)) {
        completed = number;
        valid = number <= record->frames_requested;
    }
    if (valid && strcmp(status, "passed") == 0) valid = completed == record->frames_requested;
    if (valid && json_get_int(root, "result_code", &number)) {
        valid = strcmp(status, "passed") == 0 ? number == 0 :
            (strcmp(status, "canceled") == 0 ? number == 2 : number != 0 && number != 2);
    }
    if (valid && strcmp(status, "passed") == 0)
        valid = physics_sim_headless_output_completed(record->output_root);
    if (valid) valid = copy_string(out_status, out_status_size, status);
    if (valid) *out_completed = completed;
    json_object_put(root);
    return valid;
}

static const char *terminal_summary_label(const char *state) {
    if (strcmp(state, "completed") == 0) return "passed";
    if (strcmp(state, "cancelled") == 0) return "canceled";
    if (strcmp(state, "failed") == 0) return "failed";
    return NULL;
}

bool pid_is_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    return errno != ESRCH;
}

bool parse_utc_timestamp(const char *text, time_t *out_time) {
    struct tm tm_value, roundtrip;
    int parts[6] = {0};
    const size_t offsets[] = {0, 5, 8, 11, 14, 17};
    const size_t widths[] = {4, 2, 2, 2, 2, 2};
    if (out_time) *out_time = (time_t)-1;
    if (!text || !out_time || strlen(text) != 20 ||
        text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') return false;
    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = 0; j < widths[i]; ++j) {
            unsigned char digit = (unsigned char)text[offsets[i] + j];
            if (digit < '0' || digit > '9') return false;
            parts[i] = parts[i] * 10 + digit - '0';
        }
    }
    if (parts[0] < 1 || parts[1] < 1 || parts[1] > 12 ||
        parts[2] < 1 || parts[2] > 31 || parts[3] > 23 ||
        parts[4] > 59 || parts[5] > 59) return false;
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = parts[0] - 1900;
    tm_value.tm_mon = parts[1] - 1;
    tm_value.tm_mday = parts[2];
    tm_value.tm_hour = parts[3];
    tm_value.tm_min = parts[4];
    tm_value.tm_sec = parts[5];
#if defined(__APPLE__) || defined(__unix__)
    time_t value = timegm(&tm_value);
#else
    /* Local-time conversion cannot establish a UTC observation. */
    return false;
#endif
    if (!gmtime_r(&value, &roundtrip) || roundtrip.tm_year != parts[0] - 1900 ||
        roundtrip.tm_mon != parts[1] - 1 || roundtrip.tm_mday != parts[2] ||
        roundtrip.tm_hour != parts[3] || roundtrip.tm_min != parts[4] ||
        roundtrip.tm_sec != parts[5]) return false;
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
    if (!paths || !record || !admitted_current_report(paths, record)) return false;
    PhysicsSimDetachedJobRecord *destination = record;
    PhysicsSimDetachedJobRecord candidate = *record;
    record = &candidate;
    const char *initial_terminal = terminal_summary_label(destination->state);
    if (file_exists(record->progress_path)) {
        struct stat progress;
        bool initial_reservation = strcmp(record->state, "starting") == 0 &&
            lstat(record->progress_path, &progress) == 0 && S_ISREG(progress.st_mode) &&
            progress.st_nlink == 1 && progress.st_size == 0;
        if (!initial_reservation) {
            if (!merge_progress_into_record(record->progress_path, record)) return false;
            changed = true;
        }
    }
    alive = pid_is_alive(record->pid);
    const char *terminal_label = terminal_summary_label(record->state);
    if (initial_terminal && (!terminal_label || strcmp(initial_terminal, terminal_label) != 0)) return false;
    bool summary_available = false;
    int summary_completed = record->frames_completed;
    if ((!alive || terminal_label) && file_exists(record->summary_path)) {
        if (!read_summary_observation(record, summary_status, sizeof(summary_status), &summary_completed)) return false;
        summary_available = true;
        if (terminal_label && strcmp(terminal_label, summary_status) != 0) return false;
        if (terminal_label && record->exit_code >= 0 &&
            (strcmp(summary_status, "passed") == 0 ? record->exit_code != 0 :
             (strcmp(summary_status, "canceled") == 0 ? record->exit_code != 2 :
              record->exit_code == 0 || record->exit_code == 2))) return false;
    }
    /* A stored success label or final progress is not a completion summary. */
    if (strcmp(record->state, "completed") == 0 && !summary_available) return false;
    if ((strcmp(record->state, "starting") == 0 ||
         strcmp(record->state, "running") == 0 ||
         strcmp(record->state, "stalled") == 0) &&
        !alive) {
        utc_now_string(now_utc, sizeof(now_utc));
        if (summary_available) {
            if (strcmp(summary_status, "passed") == 0) {
                snprintf(record->state, sizeof(record->state), "completed");
                snprintf(record->stage, sizeof(record->stage), "completed");
                record->exit_code = 0;
                record->frames_completed = summary_completed;
                record->sim_steps_completed_in_frame = 0;
                record->sim_steps_total_in_frame = 0;
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
        summary_available) {
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
    if (!admitted_record_counters(record)) return false;
    if (changed) {
        if (!persist_job_state(paths, record)) return false;
    }
    *destination = candidate;
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
