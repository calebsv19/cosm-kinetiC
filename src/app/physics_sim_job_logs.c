#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_logs.h"
#include "app/physics_sim_headless_output.h"
#include "app/physics_sim_job_guard.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
static int above_stdio(int descriptor) {
    if (descriptor < 0) return -1;
    int selected = fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
    close(descriptor); return selected;
}
static bool file_current(const PhysicsSimJobLogs *logs, const char *name, int descriptor) {
    struct stat path, witness;
    return descriptor >= 3 && fstat(descriptor, &witness) == 0 &&
        fstatat(logs->root_descriptor, name, &path, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(path.st_mode) && path.st_nlink == 1 && path.st_size == 0 &&
        path.st_dev == witness.st_dev && path.st_ino == witness.st_ino;
}
bool physics_sim_job_logs_check(const PhysicsSimJobLogs *logs) {
    struct stat path, witness; char normalized[1024];
    return logs->root_descriptor >= 3 &&
        physics_sim_headless_storage_directory(logs->root, normalized, sizeof(normalized)) && strcmp(normalized, logs->root) == 0 &&
        lstat(logs->root, &path) == 0 && fstat(logs->root_descriptor, &witness) == 0 &&
        S_ISDIR(path.st_mode) && path.st_dev == witness.st_dev && path.st_ino == witness.st_ino &&
        file_current(logs, "stdout.log", logs->stdout_descriptor) && file_current(logs, "stderr.log", logs->stderr_descriptor);
}
void physics_sim_job_logs_close(PhysicsSimJobLogs *logs) {
    if (logs->stdout_descriptor >= 0) close(logs->stdout_descriptor);
    if (logs->stderr_descriptor >= 0) close(logs->stderr_descriptor);
    if (logs->root_descriptor >= 0) close(logs->root_descriptor);
    logs->root_descriptor = logs->stdout_descriptor = logs->stderr_descriptor = -1;
}
bool physics_sim_job_logs_prepare(const char *root, PhysicsSimJobLogs *logs) {
    memset(logs, 0, sizeof(*logs));
    logs->root_descriptor = logs->stdout_descriptor = logs->stderr_descriptor = -1;
    if (!physics_sim_job_guard_current() || !physics_sim_headless_storage_directory(root, logs->root, sizeof(logs->root))) return false;
    char slot[1200];
    if (snprintf(slot, sizeof(slot), "%s/stdout.log", logs->root) >= (int)sizeof(slot) || !physics_sim_job_guard_covers(slot)) return false;
    logs->root_descriptor = above_stdio(open(logs->root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (logs->root_descriptor < 0) return false;
    struct stat existing, witness;
    if (lstat(logs->root, &existing) != 0 || fstat(logs->root_descriptor, &witness) != 0 ||
        existing.st_dev != witness.st_dev || existing.st_ino != witness.st_ino || !physics_sim_job_guard_current()) goto held;
    const char *names[] = {"stdout.log", "stderr.log"};
    for (int i = 0; i < 2; ++i) {
        if (fstatat(logs->root_descriptor, names[i], &existing, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) goto held;
    }
    logs->stdout_descriptor = above_stdio(openat(logs->root_descriptor, "stdout.log", O_WRONLY | O_APPEND | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600));
    if (logs->stdout_descriptor < 0) goto held;
    logs->stderr_descriptor = above_stdio(openat(logs->root_descriptor, "stderr.log", O_WRONLY | O_APPEND | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600));
    if (logs->stderr_descriptor < 0 || !physics_sim_job_guard_current() || !physics_sim_job_logs_check(logs) ||
        fsync(logs->stdout_descriptor) != 0 || fsync(logs->stderr_descriptor) != 0 || fsync(logs->root_descriptor) != 0) goto held;
    return true;
held:
    physics_sim_job_logs_close(logs); return false;
}
bool physics_sim_job_logs_redirect(PhysicsSimJobLogs *logs) {
    if (!physics_sim_job_logs_check(logs)) return false;
    int input = above_stdio(open("/dev/null", O_RDONLY | O_CLOEXEC));
    if (input < 0) return false;
    bool valid = dup2(logs->stdout_descriptor, STDOUT_FILENO) >= 0 &&
        dup2(logs->stderr_descriptor, STDERR_FILENO) >= 0 && dup2(input, STDIN_FILENO) >= 0;
    close(input);
    physics_sim_job_logs_close(logs);
    return valid;
}
