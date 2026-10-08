#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_file.h"
#include "app/physics_sim_job_json.h"
#include "app/physics_sim_job_guard.h"
#include <errno.h>
#include <stdlib.h>
#include "core_scene_compile.h"
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#define LOCK ".physics-sim-job-metadata.lock"
static bool lock_current(PhysicsSimJobFile *file) {
    struct stat path, witness;
    return physics_sim_job_guard_current() && physics_sim_job_guard_covers(file->sidecar.path) && file->lock_descriptor >= 0 &&
        fstat(file->lock_descriptor, &witness) == 0 &&
        fstatat(file->sidecar.parent_descriptor, LOCK, &path, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(path.st_mode) && path.st_nlink == 1 && path.st_size == 0 && path.st_dev == witness.st_dev && path.st_ino == witness.st_ino &&
        physics_sim_headless_sidecar_check(&file->sidecar);
}
static void close_file(PhysicsSimJobFile *file) {
    if (file->lock_descriptor >= 0) close(file->lock_descriptor);
    file->lock_descriptor = -1;
    physics_sim_headless_sidecar_close(&file->sidecar);
}
FILE *physics_sim_job_file_begin(const char *path, bool json, PhysicsSimJobFile *file) {
    file->lock_descriptor = -1; file->json = json; file->prepared = false;
    if (!physics_sim_job_guard_current() || !physics_sim_headless_sidecar_replace(path, &file->sidecar)) return NULL;
    if (!physics_sim_job_guard_covers(file->sidecar.path)) { close_file(file); return NULL; }
    if (!file->sidecar.missing) {
        if (json) {
            json_object *old = physics_sim_job_json_read(file->sidecar.path);
            if (!old) { close_file(file); return NULL; }
            json_object_put(old);
        } else if (file->sidecar.predecessor.st_size > 4096) { close_file(file); return NULL; }
    }
    file->lock_descriptor = openat(file->sidecar.parent_descriptor, LOCK,
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (file->lock_descriptor < 0 && errno == EEXIST)
        file->lock_descriptor = openat(file->sidecar.parent_descriptor, LOCK, O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (!lock_current(file) || flock(file->lock_descriptor, LOCK_EX | LOCK_NB) != 0 ||
        !lock_current(file) || fsync(file->lock_descriptor) != 0 || fsync(file->sidecar.parent_descriptor) != 0) {
        close_file(file); return NULL;
    }
    FILE *stream = physics_sim_headless_sidecar_stream(&file->sidecar);
    if (!stream) close_file(file);
    return stream;
}
static bool same_stage(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino ||
        a->st_mode != b->st_mode || a->st_nlink != b->st_nlink || a->st_size != b->st_size) return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static bool stage_current(PhysicsSimJobFile *file, FILE *stream, const struct stat *expected) {
    struct stat named, retained, opened;
    return lock_current(file) && file->sidecar.pending_name[0] &&
        fileno(stream) == file->sidecar.pending_stream_descriptor &&
        fstat(fileno(stream), &opened) == 0 &&
        fstat(file->sidecar.pending_descriptor, &retained) == 0 &&
        fstatat(file->sidecar.parent_descriptor, file->sidecar.pending_name, &named, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(named.st_mode) && named.st_nlink == 1 &&
        same_stage(&named, expected) && same_stage(&retained, expected) && same_stage(&opened, expected);
}
void physics_sim_job_file_discard(PhysicsSimJobFile *file, FILE *stream) {
    if (stream) fclose(stream);
    file->sidecar.pending_stream_descriptor = -1;
    file->prepared = false;
    close_file(file);
}
bool physics_sim_job_file_prepare(PhysicsSimJobFile *file, FILE *stream) {
    bool valid = !file->prepared && !ferror(stream) && fflush(stream) == 0 &&
        !ferror(stream) && fsync(fileno(stream)) == 0 && lock_current(file);
    struct stat stage;
    if (valid) valid = fstat(fileno(stream), &stage) == 0 && stage.st_size > 0 &&
        stage.st_size <= (file->json ? 16 * 1024 * 1024 : 4096) && stage_current(file, stream, &stage);
    if (valid && file->json) {
        char path[1200];
        int length = snprintf(path, sizeof(path), "%s/%s", file->sidecar.parent, file->sidecar.pending_name);
        valid = length >= 0 && length < (int)sizeof(path);
        json_object *object = valid ? physics_sim_job_json_read(path) : NULL;
        valid = object != NULL;
        if (object) json_object_put(object);
    }
    if (valid) valid = stage_current(file, stream, &stage);
    if (!valid) { physics_sim_job_file_discard(file, stream); return false; }
    file->prepared_stage = stage;
    file->prepared = true;
    return true;
}
bool physics_sim_job_file_ready(PhysicsSimJobFile *file, FILE *stream) {
    return file->prepared && !ferror(stream) && fflush(stream) == 0 &&
        !ferror(stream) && stage_current(file, stream, &file->prepared_stage);
}
bool physics_sim_job_file_publish(PhysicsSimJobFile *file, FILE *stream) {
    bool valid = physics_sim_job_file_ready(file, stream);
    if (valid) valid = physics_sim_headless_sidecar_publish(&file->sidecar, stream);
    else fclose(stream);
    file->sidecar.pending_stream_descriptor = -1;
    file->prepared = false;
    close_file(file);
    return valid;
}
bool physics_sim_job_file_finish(PhysicsSimJobFile *file, FILE *stream) {
    if (!physics_sim_job_file_prepare(file, stream)) return false;
    return physics_sim_job_file_publish(file, stream);
}
bool physics_sim_job_file_text(const char *path, const char *text) {
    if (!text || strlen(text) > 4096) return false;
    PhysicsSimJobFile file;
    FILE *stream = physics_sim_job_file_begin(path, false, &file);
    if (!stream) return false;
    fputs(text, stream);
    return physics_sim_job_file_finish(&file, stream);
}

/* Purpose-specific retained publication of the fixed operational metadata pair. */
static bool pair_write(int fd, const char *bytes, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = write(fd, bytes + offset, length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        offset += (size_t)count;
    }
    return true;
}
static bool pair_equal(int a, int b, off_t length) {
    char left[8192], right[8192];
    if (length < 0 || length > 1024 * 1024) return false;
    for (off_t offset = 0; offset < length;) {
        size_t wanted = length - offset < (off_t)sizeof(left) ? (size_t)(length - offset) : sizeof(left);
        for (int side = 0; side < 2; ++side) {
            size_t read_count = 0;
            while (read_count < wanted) {
                ssize_t count = pread(side ? b : a, (side ? right : left) + read_count,
                    wanted - read_count, offset + (off_t)read_count);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0) return false;
                read_count += (size_t)count;
            }
        }
        if (memcmp(left, right, wanted) != 0) return false;
        offset += (off_t)wanted;
    }
    char extra;
    return pread(a, &extra, 1, length) == 0 && pread(b, &extra, 1, length) == 0;
}
static bool pair_entry(int directory, const char *name, const struct stat *expected) {
    struct stat named;
    return fstatat(directory, name, &named, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(named.st_mode) && named.st_nlink == 1 && same_stage(&named, expected);
}
static bool pair_copy(int directory, const char *name, int source,
                      const struct stat *expected, struct stat *saved) {
    if (expected->st_size < 0 || expected->st_size > 1024 * 1024) return false;
    struct stat current;
    if (fstat(source, &current) != 0 || !same_stage(&current, expected)) return false;
    int fd = openat(directory, name, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    bool valid = true;
    char bytes[8192];
    for (off_t offset = 0; valid && offset < expected->st_size;) {
        size_t wanted = expected->st_size - offset < (off_t)sizeof(bytes) ?
            (size_t)(expected->st_size - offset) : sizeof(bytes);
        ssize_t count = pread(source, bytes, wanted, offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { valid = false; break; }
        valid = pair_write(fd, bytes, (size_t)count);
        offset += count;
    }
    if (valid) valid = fsync(fd) == 0 && fstat(fd, saved) == 0 &&
        S_ISREG(saved->st_mode) && saved->st_nlink == 1 && saved->st_size == expected->st_size &&
        pair_equal(source, fd, expected->st_size) && fstat(source, &current) == 0 &&
        same_stage(&current, expected) && pair_entry(directory, name, saved);
    if (close(fd) != 0) valid = false;
    return valid;
}
static bool pair_digest(int directory, const char *name, const struct stat *expected,
                        char hex[CORE_SCENE_COMPILE_SHA256_HEX_SIZE]) {
    if (expected->st_size < 0 || expected->st_size > 1024 * 1024) return false;
    size_t length = (size_t)expected->st_size;
    unsigned char *bytes = malloc(length ? length : 1u);
    if (!bytes) return false;
    int fd = openat(directory, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    struct stat opened;
    bool valid = fd >= 0 && fstat(fd, &opened) == 0 && same_stage(&opened, expected) &&
        pair_entry(directory, name, expected);
    size_t offset = 0;
    while (valid && offset < length) {
        ssize_t count = pread(fd, bytes + offset, length - offset, (off_t)offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { valid = false; break; }
        offset += (size_t)count;
    }
    unsigned char extra;
    if (valid) valid = pread(fd, &extra, 1, (off_t)length) == 0 &&
        fstat(fd, &opened) == 0 && same_stage(&opened, expected) && pair_entry(directory, name, expected) &&
        core_scene_compile_sha256(bytes, length, hex).code == CORE_OK;
    if (fd >= 0 && close(fd) != 0) valid = false;
    free(bytes);
    return valid;
}
static bool pair_record(int directory, const char *name, const char *text, struct stat *saved) {
    size_t length = strlen(text);
    int fd = openat(directory, name, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    char readback[2048];
    bool valid = length < sizeof(readback) && pair_write(fd, text, length) && fsync(fd) == 0 &&
        fstat(fd, saved) == 0 && saved->st_size == (off_t)length &&
        pread(fd, readback, length, 0) == (ssize_t)length && memcmp(text, readback, length) == 0 &&
        pair_entry(directory, name, saved);
    if (close(fd) != 0) valid = false;
    return valid;
}
static bool pair_directory(int parent, const char *name, int fd) {
    struct stat named, opened;
    return fstatat(parent, name, &named, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(named.st_mode) &&
        fstat(fd, &opened) == 0 && named.st_dev == opened.st_dev && named.st_ino == opened.st_ino;
}
static bool pair_target_equal(int parent, const char *name, int capsule,
                               const char *snapshot, const struct stat *saved) {
    int target = openat(parent, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    int copy = openat(capsule, snapshot, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    struct stat before, after, retained;
    bool valid = target >= 0 && copy >= 0 && fstat(target, &before) == 0 &&
        S_ISREG(before.st_mode) && before.st_nlink == 1 && before.st_size == saved->st_size &&
        fstat(copy, &retained) == 0 && same_stage(&retained, saved) &&
        pair_entry(capsule, snapshot, saved) && pair_equal(target, copy, saved->st_size) &&
        fstat(target, &after) == 0 && same_stage(&before, &after) && pair_entry(parent, name, &after);
    if (target >= 0 && close(target) != 0) valid = false;
    if (copy >= 0 && close(copy) != 0) valid = false;
    return valid;
}
bool physics_sim_job_file_publish_pair(PhysicsSimJobFile *status, FILE *status_stream,
                                        PhysicsSimJobFile *report, FILE *report_stream) {
    bool valid = false;
    int root = -1, report_parent = -1, capsule = -1;
    char attempt[96], expected_report_parent[1200], hold[256], plan[1024];
    struct stat snapshots[4], plan_witness, hold_witness, completed_witness, root_identity;
    char digests[4][CORE_SCENE_COMPILE_SHA256_HEX_SIZE], encoded[4][68], plan_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    const char *names[4] = {"old-status.json", "old-report.json", "new-status.json", "new-report.json"};
    bool present[4] = {!status->sidecar.missing, !report->sidecar.missing, true, true};
    int n = snprintf(expected_report_parent, sizeof(expected_report_parent), "%s/output", status->sidecar.parent);
    const char *status_name = strrchr(status->sidecar.path, '/');
    const char *report_name = strrchr(report->sidecar.path, '/');
    if (n < 0 || n >= (int)sizeof(expected_report_parent) || !status_name || !report_name ||
        strcmp(status_name + 1, "job_status.json") != 0 || strcmp(report_name + 1, "report.json") != 0 ||
        strcmp(expected_report_parent, report->sidecar.parent) != 0 ||
        !physics_sim_job_guard_owns(status->sidecar.parent) ||
        !physics_sim_job_file_ready(status, status_stream) || !physics_sim_job_file_ready(report, report_stream)) goto finish;
    const struct stat *expected[4] = {&status->sidecar.predecessor, &report->sidecar.predecessor,
        &status->prepared_stage, &report->prepared_stage};
    for (int i = 0; i < 4; ++i) {
        if (present[i] && (expected[i]->st_size < 0 || expected[i]->st_size > 1024 * 1024)) goto finish;
    }
    root = fcntl(status->sidecar.parent_descriptor, F_DUPFD_CLOEXEC, 0);
    report_parent = fcntl(report->sidecar.parent_descriptor, F_DUPFD_CLOEXEC, 0);
    if (root < 0 || report_parent < 0 || fstat(root, &root_identity) != 0 ||
        !pair_directory(root, "output", report_parent)) goto finish;
    struct stat pending;
    if (fstatat(root, PHYSICS_SIM_JOB_PAIR_PENDING, &pending, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) goto finish;
    static unsigned long sequence;
    bool created = false;
    for (int tries = 0; tries < 64; ++tries) {
        if (++sequence == 0) goto finish;
        snprintf(attempt, sizeof(attempt), ".job-pair-%ld-%lu", (long)getpid(), sequence);
        if (mkdirat(root, attempt, 0700) == 0) { created = true; break; }
        if (errno != EEXIST) goto finish;
    }
    if (!created) goto finish;
    capsule = openat(root, attempt, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (capsule < 0 || !pair_directory(root, attempt, capsule)) goto finish;
    int sources[4] = {status->sidecar.descriptor, report->sidecar.descriptor,
        status->sidecar.pending_descriptor, report->sidecar.pending_descriptor};

    for (int i = 0; i < 4; ++i) {
        if (present[i]) {
            if (!pair_copy(capsule, names[i], sources[i], expected[i], &snapshots[i]) ||
                !pair_digest(capsule, names[i], &snapshots[i], digests[i])) goto finish;
            snprintf(encoded[i], sizeof(encoded[i]), "\"%s\"", digests[i]);
        } else snprintf(encoded[i], sizeof(encoded[i]), "null");
    }
    n = snprintf(plan, sizeof(plan),
        "{\"schema\":\"physics_sim_job_pair_v2\",\"root_device\":%llu,\"root_inode\":%llu,\"status_target\":\"job_status.json\","
        "\"report_target\":\"output/report.json\",\"old_status_present\":%s,\"old_report_present\":%s,"
        "\"old_status\":\"old-status.json\",\"old_report\":\"old-report.json\","
        "\"new_status\":\"new-status.json\",\"new_report\":\"new-report.json\",\"retained_sha256\":{\"old-status.json\":%s,\"old-report.json\":%s,\"new-status.json\":%s,\"new-report.json\":%s}}\n",
        (unsigned long long)root_identity.st_dev, (unsigned long long)root_identity.st_ino,
        present[0] ? "true" : "false", present[1] ? "true" : "false",
        encoded[0], encoded[1], encoded[2], encoded[3]);
    if (n < 0 || n >= (int)sizeof(plan) || !pair_record(capsule, "plan.json", plan, &plan_witness) ||
        fsync(capsule) != 0 || fsync(root) != 0 || !pair_directory(root, attempt, capsule)) goto finish;
    if (core_scene_compile_sha256(plan, (size_t)n, plan_digest).code != CORE_OK) goto finish;
    n = snprintf(hold, sizeof(hold), "{\"schema\":\"physics_sim_job_pair_pending_v2\",\"attempt\":\"%s\",\"plan_sha256\":\"%s\"}\n", attempt, plan_digest);
    if (n < 0 || n >= (int)sizeof(hold) ||
        !pair_record(root, PHYSICS_SIM_JOB_PAIR_PENDING, hold, &hold_witness) || fsync(root) != 0) goto finish;
    for (int i = 0; i < 4; ++i) if (present[i] && !pair_entry(capsule, names[i], &snapshots[i])) goto finish;
    if (!pair_entry(capsule, "plan.json", &plan_witness) || !pair_entry(root, PHYSICS_SIM_JOB_PAIR_PENDING, &hold_witness) ||
        !physics_sim_job_file_ready(status, status_stream) || !physics_sim_job_file_ready(report, report_stream)) goto finish;
    bool published = physics_sim_job_file_publish(status, status_stream);
    status_stream = NULL;
    if (!published || !pair_entry(root, PHYSICS_SIM_JOB_PAIR_PENDING, &hold_witness) ||
        !pair_directory(root, attempt, capsule)) goto finish;
    for (int i = 0; i < 4; ++i) if (present[i] && !pair_entry(capsule, names[i], &snapshots[i])) goto finish;
    if (!pair_entry(capsule, "plan.json", &plan_witness) ||
        !pair_directory(root, "output", report_parent) || !physics_sim_job_file_ready(report, report_stream)) goto finish;
    published = physics_sim_job_file_publish(report, report_stream);
    report_stream = NULL;
    if (!published || !physics_sim_job_guard_owns(status->sidecar.parent) ||
        !pair_directory(root, attempt, capsule) || !pair_entry(capsule, "plan.json", &plan_witness)) goto finish;
    for (int i = 0; i < 4; ++i) if (present[i] && !pair_entry(capsule, names[i], &snapshots[i])) goto finish;
    if (!pair_target_equal(root, "job_status.json", capsule, names[2], &snapshots[2]) ||
        !pair_target_equal(report_parent, "report.json", capsule, names[3], &snapshots[3]) ||
        !pair_record(capsule, "completed.json", "{\"schema\":\"physics_sim_job_pair_complete_v1\"}\n", &completed_witness) ||
        fsync(capsule) != 0 || !pair_entry(capsule, "completed.json", &completed_witness) ||
        !pair_entry(root, PHYSICS_SIM_JOB_PAIR_PENDING, &hold_witness) ||
        !physics_sim_job_guard_owns(status->sidecar.parent) || !pair_directory(root, attempt, capsule)) goto finish;
    for (int i = 0; i < 4; ++i) if (present[i] && !pair_entry(capsule, names[i], &snapshots[i])) goto finish;
    if (!pair_directory(root, "output", report_parent) || !pair_entry(capsule, "plan.json", &plan_witness) ||
        !pair_target_equal(root, "job_status.json", capsule, names[2], &snapshots[2]) ||
        !pair_target_equal(report_parent, "report.json", capsule, names[3], &snapshots[3]) ||
        !pair_entry(root, PHYSICS_SIM_JOB_PAIR_PENDING, &hold_witness) ||
        !physics_sim_job_guard_owns(status->sidecar.parent)) goto finish;
    if (unlinkat(root, PHYSICS_SIM_JOB_PAIR_PENDING, 0) != 0 || fsync(root) != 0) goto finish;
    valid = true;
finish:
    if (status_stream) physics_sim_job_file_discard(status, status_stream);
    if (report_stream) physics_sim_job_file_discard(report, report_stream);
    if (capsule >= 0 && close(capsule) != 0) valid = false;
    if (report_parent >= 0 && close(report_parent) != 0) valid = false;
    if (root >= 0 && close(root) != 0) valid = false;
    return valid;
}
