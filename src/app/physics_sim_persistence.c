#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_persistence.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#define LOCK ".physics-sim-persistence.lock"
#define MAX_BYTES (16 * 1024 * 1024)
#define MAX_CLASS_BYTES (UINT64_C(8) * 1024 * 1024 * 1024)

static void release(PhysicsSimPersistence *save) {
    if (save->lock_descriptor >= 0) close(save->lock_descriptor);
    save->lock_descriptor = -1;
    physics_sim_headless_sidecar_close(&save->sidecar);
}

static bool current(PhysicsSimPersistence *save) {
    struct stat path, witness;
    return save->lock_descriptor >= 0 &&
        fstat(save->lock_descriptor, &witness) == 0 &&
        fstatat(save->sidecar.parent_descriptor, LOCK, &path, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(path.st_mode) && path.st_nlink == 1 && path.st_size == 0 &&
        path.st_dev == witness.st_dev && path.st_ino == witness.st_ino &&
        physics_sim_headless_sidecar_check(&save->sidecar);
}

FILE *physics_sim_persistence_begin(const char *path, PhysicsSimPersistence *save) {
    return physics_sim_persistence_begin_bounded(path, MAX_BYTES, save);
}

FILE *physics_sim_persistence_begin_bounded(const char *path, uint64_t byte_limit, PhysicsSimPersistence *save) {
    if (!save) return NULL;
    save->lock_descriptor = -1;
    if (!byte_limit || byte_limit > MAX_CLASS_BYTES) return NULL;
    save->byte_limit = byte_limit;
    if (!physics_sim_headless_sidecar_replace(path, &save->sidecar)) return NULL;
    const char *name = strrchr(save->sidecar.path, '/');
    if (!name || strcmp(name + 1, LOCK) == 0 ||
        strncmp(name + 1, ".headless-sidecar-", 18) == 0 ||
        (!save->sidecar.missing && (save->sidecar.predecessor.st_size < 0 || (uint64_t)save->sidecar.predecessor.st_size > byte_limit))) {
        release(save); return NULL;
    }
    save->lock_descriptor = openat(save->sidecar.parent_descriptor, LOCK,
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (save->lock_descriptor < 0 && errno == EEXIST)
        save->lock_descriptor = openat(save->sidecar.parent_descriptor, LOCK,
            O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (!current(save) || flock(save->lock_descriptor, LOCK_EX | LOCK_NB) != 0 ||
        !current(save) || fsync(save->lock_descriptor) != 0 ||
        fsync(save->sidecar.parent_descriptor) != 0) {
        release(save); return NULL;
    }
    FILE *stream = physics_sim_headless_sidecar_stream(&save->sidecar);
    if (!stream) release(save);
    return stream;
}

void physics_sim_persistence_abort(PhysicsSimPersistence *save, FILE *stream) {
    if (stream) fclose(stream);
    if (save) release(save);
}

bool physics_sim_persistence_finish(PhysicsSimPersistence *save, FILE *stream) {
    if (!save || !stream) return false;
    struct stat stage;
    bool valid = !ferror(stream) && fflush(stream) == 0 && !ferror(stream) &&
        fstat(fileno(stream), &stage) == 0 && stage.st_size >= 0 && (uint64_t)stage.st_size <= save->byte_limit && current(save);
    if (valid) valid = physics_sim_headless_sidecar_publish(&save->sidecar, stream);
    else fclose(stream);
    release(save);
    return valid;
}

bool physics_sim_persistence_text(const char *path, const char *text) {
    if (!text || strlen(text) > MAX_BYTES) return false;
    PhysicsSimPersistence save;
    FILE *stream = physics_sim_persistence_begin(path, &save);
    if (!stream) return false;
    fputs(text, stream);
    return physics_sim_persistence_finish(&save, stream);
}

bool physics_sim_persistence_runtime_directory(void) {
    char admitted[1024];
    if (!physics_sim_headless_storage_directory("data/runtime/.persistence-admission", admitted, sizeof(admitted))) return false;
    int root = open(".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root < 0) return false;
    bool valid = mkdirat(root, "data", 0700) == 0 || errno == EEXIST;
    int data = valid ? openat(root, "data", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) : -1;
    valid = data >= 0 && (mkdirat(data, "runtime", 0700) == 0 || errno == EEXIST);
    int runtime = valid ? openat(data, "runtime", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) : -1;
    valid = runtime >= 0 && fsync(runtime) == 0 && fsync(data) == 0 && fsync(root) == 0;
    if (runtime >= 0) close(runtime);
    if (data >= 0) close(data);
    close(root);
    return valid;
}
