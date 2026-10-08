#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_guard.h"
#include "app/physics_sim_headless_output.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#define LOCK ".physics-sim-job-operation.lock"
static _Thread_local const PhysicsSimJobGuard *active;
static bool same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}
bool physics_sim_job_guard_check(const PhysicsSimJobGuard *guard) {
    struct stat root, parent, lock, witness; char normalized[1024];
    if (guard->process != getpid() || guard->root_descriptor < 0 || guard->parent_descriptor < 0 || guard->lock_descriptor < 0 ||
        !physics_sim_headless_storage_directory(guard->root, normalized, sizeof(normalized)) || strcmp(normalized, guard->root) != 0 ||
        lstat(guard->root, &root) != 0 || fstat(guard->root_descriptor, &witness) != 0 || !S_ISDIR(root.st_mode) || !same(&root, &witness) ||
        lstat(guard->parent, &parent) != 0 || fstat(guard->parent_descriptor, &witness) != 0 || !S_ISDIR(parent.st_mode) || !same(&parent, &witness) ||
        fstatat(guard->root_descriptor, LOCK, &lock, AT_SYMLINK_NOFOLLOW) != 0 ||
        fstat(guard->lock_descriptor, &witness) != 0 || !S_ISREG(lock.st_mode) || lock.st_nlink != 1 || lock.st_size != 0 || !same(&lock, &witness)) return false;
    return true;
}
bool physics_sim_job_guard_owns(const char *root) {
    return active && root && strcmp(root, active->root) == 0 && physics_sim_job_guard_check(active);
}
bool physics_sim_job_guard_current(void) { return !active || physics_sim_job_guard_check(active); }
bool physics_sim_job_guard_covers(const char *path) {
    if (!active) return true;
    size_t size = strlen(active->root);
    return physics_sim_job_guard_current() && strncmp(active->root, path, size) == 0 && path[size] == '/';
}
void physics_sim_job_guard_end(PhysicsSimJobGuard *guard) {
    if (active == guard) active = NULL;
    if (guard->lock_descriptor >= 0) close(guard->lock_descriptor);
    if (guard->root_descriptor >= 0) close(guard->root_descriptor);
    if (guard->parent_descriptor >= 0) close(guard->parent_descriptor);
    guard->lock_descriptor = guard->root_descriptor = guard->parent_descriptor = -1;
}
bool physics_sim_job_guard_begin(const char *root, PhysicsSimJobGuard *guard) {
    if (active) return false;
    memset(guard, 0, sizeof(*guard));
    guard->root_descriptor = guard->parent_descriptor = guard->lock_descriptor = -1;
    guard->process = getpid();
    if (!physics_sim_headless_storage_directory(root, guard->root, sizeof(guard->root))) return false;
    snprintf(guard->parent, sizeof(guard->parent), "%s", guard->root);
    char *slash = strrchr(guard->parent, '/');
    if (!slash || slash == guard->parent) return false;
    *slash = '\0';
    guard->parent_descriptor = open(guard->parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    guard->root_descriptor = open(guard->root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (guard->parent_descriptor < 0 || guard->root_descriptor < 0) goto held;
    guard->lock_descriptor = openat(guard->root_descriptor, LOCK, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (guard->lock_descriptor < 0 && errno == EEXIST)
        guard->lock_descriptor = openat(guard->root_descriptor, LOCK, O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (!physics_sim_job_guard_check(guard) || flock(guard->lock_descriptor, LOCK_EX | LOCK_NB) != 0 ||
        !physics_sim_job_guard_check(guard) || fsync(guard->lock_descriptor) != 0 || fsync(guard->root_descriptor) != 0 || fsync(guard->parent_descriptor) != 0) goto held;
    struct stat pending;
    if (fstatat(guard->root_descriptor, PHYSICS_SIM_JOB_PAIR_PENDING, &pending, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) goto held;
    active = guard; return true;
held:
    physics_sim_job_guard_end(guard); return false;
}
