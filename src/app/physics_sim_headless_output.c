#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "app/physics_sim_headless_output.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define OWNER ".physics-sim-headless-owner"

static bool within(const char *path, const char *parent) {
    size_t n = strlen(parent);
    return strncmp(path, parent, n) == 0 && (path[n] == '/' || path[n] == '\0');
}

static bool identity(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static bool normalized(const char *input, char *out, size_t size) {
    char absolute[1024], cursor[1024] = "", copy[1024], *part, *save = NULL;
    struct stat st;
    if (!input || !input[0]) return false;
    if (input[0] == '/') {
        if (snprintf(absolute, sizeof(absolute), "%s", input) >= (int)sizeof(absolute)) return false;
    } else {
        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd)) || snprintf(absolute, sizeof(absolute), "%s/%s", cwd, input) >= (int)sizeof(absolute)) return false;
    }
    snprintf(copy, sizeof(copy), "%s", absolute);
    int depth = 0;
    for (part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (++depth > 128 || strcmp(part, "..") == 0) return false;
        if (strcmp(part, ".") == 0) continue;
        size_t n = strlen(part);
        if (strcmp(part, ".git") == 0 || strcmp(part, ".aws") == 0 || strcmp(part, ".ssh") == 0 ||
            (n >= 4 && strcmp(part + n - 4, ".app") == 0)) return false;
        size_t length = strlen(cursor);
        if (length + n + 2 > sizeof(cursor)) return false;
        cursor[length] = '/'; memcpy(cursor + length + 1, part, n + 1);
        if (lstat(cursor, &st) == 0) {
            if (S_ISLNK(st.st_mode)) {
                char target[1024];
                if (!realpath(cursor, target) ||
                    !((strcmp(cursor, "/tmp") == 0 && strcmp(target, "/private/tmp") == 0) ||
                      (strcmp(cursor, "/var") == 0 && strcmp(target, "/private/var") == 0))) return false;
                snprintf(cursor, sizeof(cursor), "%s", target);
            } else if (!S_ISDIR(st.st_mode)) return false;
        } else if (errno != ENOENT) return false;
    }
    if (!cursor[0] || strlen(cursor) >= size) return false;
    strcpy(out, cursor); return true;
}

static bool admitted(const char *path) {
    const char *protected[] = {"/System", "/usr", "/bin", "/sbin", "/etc", "/private/etc", "/Library", "/Applications", "/dev", "/proc", "/sys"};
    for (size_t i = 0; i < sizeof(protected)/sizeof(protected[0]); ++i)
        if (within(path, protected[i])) return false;
    const char *home = getenv("HOME");
    if (strcmp(path, "/") == 0 || strcmp(path, "/tmp") == 0 || strcmp(path, "/private/tmp") == 0 ||
        strcmp(path, "/private/var") == 0 || strcmp(path, "/Users") == 0 || strcmp(path, "/home") == 0 ||
        (home && within(home, path))) return false;
    char ancestor[1024]; snprintf(ancestor, sizeof(ancestor), "%s", path);
    for (;;) {
        char git[1100]; struct stat st;
        snprintf(git, sizeof(git), "%s/.git", ancestor);
        if (lstat(git, &st) == 0) {
            const char *names[] = {"build", "tmp", "data/experiments", "data/runtime", "visual_artifacts", "export", "dist"};
            bool allowed = false;
            for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
                char generated[1100]; snprintf(generated, sizeof(generated), "%s/%s", ancestor, names[i]);
                if (within(path, generated) && strcmp(path, generated) != 0) allowed = true;
            }
            if (!allowed) return false;
        }
        char *slash = strrchr(ancestor, '/');
        if (!slash || slash == ancestor) break;
        *slash = '\0';
    }
    return true;
}

bool physics_sim_headless_storage_directory(const char *path, char *normalized_path, size_t size) {
    return normalized(path, normalized_path, size) && admitted(normalized_path);
}

static bool flush_parent(const char *path) {
    char parent[1024]; snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) return false;
    *slash = '\0';
    int fd = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    bool valid = fsync(fd) == 0; close(fd); return valid;
}

static bool empty_directory(const char *path) {
    DIR *dir = opendir(path); struct dirent *entry; bool empty = true;
    if (!dir) return false;
    while ((entry = readdir(dir)))
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) { empty = false; break; }
    closedir(dir); return empty;
}

static bool marker_identity(const struct stat *a, const struct stat *b) {
    return identity(a, b) && a->st_mode == b->st_mode && a->st_nlink == b->st_nlink && a->st_size == b->st_size &&
#ifdef __APPLE__
        a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
        a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static bool owner_marker(const char *path, const struct stat *root, const char *state, struct stat *out_witness) {
    char marker[1100], expected[256], text[256]; struct stat before, after, named;
    if (snprintf(marker, sizeof(marker), "%s/%s", path, OWNER) >= (int)sizeof(marker)) return false;
    int length = snprintf(expected, sizeof(expected), "physics_sim_headless_output_v1 %s %ju %ju\n",
        state, (uintmax_t)root->st_dev, (uintmax_t)root->st_ino);
    if (length <= 0 || length >= (int)sizeof(expected)) return false;
    int fd = open(marker, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    bool valid = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 && before.st_size == length;
    size_t received = 0;
    while (valid && received < (size_t)length) {
        ssize_t count = pread(fd, text + received, (size_t)length - received, (off_t)received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) valid = false; else received += (size_t)count;
    }
    char extra;
    valid = valid && pread(fd, &extra, 1, length) == 0 && memcmp(text, expected, (size_t)length) == 0 &&
        fstat(fd, &after) == 0 && marker_identity(&before, &after) && lstat(marker, &named) == 0 &&
        marker_identity(&before, &named);
    if (close(fd) != 0) valid = false;
    if (valid && out_witness) *out_witness = before;
    return valid;
}

static bool running_marker_current(PhysicsSimHeadlessOutputOwner *owner, const struct stat *root) {
    struct stat current;
    return owner_marker(owner->root, root, "running", &current) && marker_identity(&owner->running_marker, &current);
}
static bool write_owner(PhysicsSimHeadlessOutputOwner *owner, bool completed) {
    struct stat selected, witness, written, named; char temporary[80], text[256];
    if (lstat(owner->root, &selected) != 0 || fstat(owner->descriptor, &witness) != 0 ||
        !S_ISDIR(selected.st_mode) || !identity(&selected, &witness) ||
        (completed && !running_marker_current(owner, &selected))) return false;
    snprintf(temporary, sizeof(temporary), ".headless-owner-%ld.pending", (long)getpid());
    const char *name = completed ? temporary : OWNER;
    int fd = openat(owner->descriptor, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    int length = snprintf(text, sizeof(text), "physics_sim_headless_output_v1 %s %ju %ju\n",
        completed ? "completed" : "running", (uintmax_t)selected.st_dev, (uintmax_t)selected.st_ino);
    bool valid = length > 0 && length < (int)sizeof(text) && write(fd, text, (size_t)length) == length && fsync(fd) == 0 &&
        fstat(fd, &written) == 0 && S_ISREG(written.st_mode) && written.st_nlink == 1 && written.st_size == length;
    if (close(fd) != 0) valid = false;
    valid = valid && fstatat(owner->descriptor, name, &named, AT_SYMLINK_NOFOLLOW) == 0 && marker_identity(&written, &named) &&
        lstat(owner->root, &named) == 0 && identity(&selected, &named);
    if (valid && completed) {
        valid = running_marker_current(owner, &selected) && renameat(owner->descriptor, temporary, owner->descriptor, OWNER) == 0;
        /* Rename can update ctime. Check the new receipt's bytes and identity,
         * without mistaking legitimate rename metadata for external drift. */
        valid = valid && owner_marker(owner->root, &selected, "completed", &named) && identity(&written, &named);
    }
    if (valid && !completed) valid = owner_marker(owner->root, &selected, "running", &named) && marker_identity(&written, &named);
    valid = valid && fsync(owner->descriptor) == 0;
    if (valid && !completed) owner->running_marker = written;
    return valid;
}

bool physics_sim_headless_output_prepare(const char *root, const char *input, bool overwrite,
    PhysicsSimHeadlessOutputOwner *owner, char *error, size_t error_size) {
    struct stat st;
    owner->descriptor = -1; owner->root[0] = '\0';
    memset(&owner->running_marker, 0, sizeof(owner->running_marker));
    if (!normalized(root, owner->root, sizeof(owner->root)) || !admitted(owner->root)) {
        snprintf(error, error_size, "output root is linked, protected or outside admitted generated storage"); return false;
    }
    if (input && input[0]) {
        char resolved[1024];
        if (realpath(input, resolved) && within(resolved, owner->root)) {
            snprintf(error, error_size, "output root contains the runtime scene input"); return false;
        }
    }
    if (lstat(owner->root, &st) == 0 && !empty_directory(owner->root)) {
        if (!overwrite) { snprintf(error, error_size, "output root already exists and is not empty"); return false; }
        struct stat marker_before;
        if (!owner_marker(owner->root, &st, "completed", &marker_before)) {
            snprintf(error, error_size, "unknown or incomplete output root held; choose a fresh output root"); return false;
        }
        char archive[1100], previous[1200]; struct stat current;
        if (snprintf(archive, sizeof(archive), "%s.retained-XXXXXX", owner->root) >= (int)sizeof(archive) ||
            !mkdtemp(archive)) { snprintf(error, error_size, "cannot allocate retained predecessor slot"); return false; }
        snprintf(previous, sizeof(previous), "%s/previous", archive);
        struct stat marker_now;
        if (lstat(owner->root, &current) != 0 || !identity(&st, &current) ||
            !owner_marker(owner->root, &current, "completed", &marker_now) || !marker_identity(&marker_before, &marker_now) ||
            rename(owner->root, previous) != 0) {
            snprintf(error, error_size, "output root changed or predecessor retention failed"); return false;
        }
        int fd = open(archive, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0 || fsync(fd) != 0) { if (fd >= 0) close(fd); snprintf(error, error_size, "retained predecessor flush failed"); return false; }
        close(fd);
        if (!flush_parent(owner->root)) { snprintf(error, error_size, "predecessor parent flush failed; evidence retained"); return false; }
        fprintf(stderr, "[physics_sim_headless] retained predecessor: %s\n", previous);
    }
    char components[1024]; snprintf(components, sizeof(components), "%s", owner->root);
    for (char *p = components + 1; ; ++p) {
        if (*p != '/' && *p != '\0') continue;
        char saved = *p; *p = '\0';
        if (mkdir(components, 0700) != 0 && errno != EEXIST) {
            snprintf(error, error_size, "cannot create fresh output root; predecessor retained"); return false;
        }
        *p = saved; if (!saved) break;
    }
    owner->descriptor = open(owner->root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (owner->descriptor < 0 || !write_owner(owner, false)) {
        snprintf(error, error_size, "cannot claim fresh output root; existing evidence held");
        physics_sim_headless_output_close(owner); return false;
    }
    if (!flush_parent(owner->root)) { snprintf(error, error_size, "fresh output parent flush failed"); physics_sim_headless_output_close(owner); return false; }
    return true;
}

bool physics_sim_headless_output_completed(const char *root) {
    char path[1024]; struct stat before, after;
    return physics_sim_headless_storage_directory(root, path, sizeof(path)) &&
        lstat(path, &before) == 0 && S_ISDIR(before.st_mode) &&
        owner_marker(path, &before, "completed", NULL) && lstat(path, &after) == 0 && identity(&before, &after);
}

bool physics_sim_headless_output_finish(PhysicsSimHeadlessOutputOwner *owner) {
    return owner->descriptor >= 0 && write_owner(owner, true);
}

void physics_sim_headless_output_close(PhysicsSimHeadlessOutputOwner *owner) {
    if (owner->descriptor >= 0) close(owner->descriptor);
    owner->descriptor = -1;
}


bool physics_sim_headless_sidecar_plan(const char *output, const char *input,
    const char *selected, const char *fallback, bool overwrite,
    char *path, size_t size, char *error, size_t error_size) {
    (void)overwrite; /* Output-root admission decides whether existing internal files may be retained. */
    char output_root[1024], requested[1024], parent[1024], resolved[1024]; struct stat st;
    if (!normalized(output, output_root, sizeof(output_root)) || !admitted(output_root)) goto held;
    if (selected && selected[0]) {
        if (snprintf(requested, sizeof(requested), "%s", selected) >= (int)sizeof(requested)) goto held;
    } else if (snprintf(requested, sizeof(requested), "%s/%s", output_root, fallback) >= (int)sizeof(requested)) goto held;
    char *slash = strrchr(requested, '/');
    const char *name = slash ? slash + 1 : requested;
    if (!name[0] || name[0] == '.' || strcmp(name, "wind_shot_manifest.json") == 0 ||
        strcmp(name, "wind_analysis_timeseries.jsonl") == 0) goto held;
    if (slash) {
        size_t length = (size_t)(slash - requested);
        if (length == 0) snprintf(parent, sizeof(parent), "/");
        else { memcpy(parent, requested, length); parent[length] = '\0'; }
    } else snprintf(parent, sizeof(parent), ".");
    char normalized_parent[1024];
    if (!normalized(parent, normalized_parent, sizeof(normalized_parent)) ||
        snprintf(path, size, "%s/%s", normalized_parent, name) >= (int)size || !admitted(path)) goto held;
    if (input && input[0] && realpath(input, resolved) && strcmp(resolved, path) == 0) goto held;
    bool internal = within(path, output_root);
    /* Native cache/manifests occupy subtrees; sidecars inside the root are direct children. */
    if (internal && strcmp(normalized_parent, output_root) != 0) goto held;
    if (!internal && (lstat(normalized_parent, &st) != 0 || !S_ISDIR(st.st_mode))) goto held;
    if (lstat(path, &st) == 0) {
        if (!internal || !S_ISREG(st.st_mode) || st.st_nlink != 1) goto held;
    } else if (errno != ENOENT) goto held;
    return true;
held:
    snprintf(error, error_size, "summary/progress destination is linked, protected, overlapping or already held");
    return false;
}

static bool predecessor_unchanged(const struct stat *a, const struct stat *b) {
    if (!identity(a, b) || a->st_mode != b->st_mode || a->st_size != b->st_size || a->st_nlink != b->st_nlink) return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}

bool physics_sim_headless_sidecar_check(const PhysicsSimHeadlessSidecar *sidecar) {
    struct stat parent, parent_witness, file, file_witness; char current[1024];
    if (sidecar->held || sidecar->parent_descriptor < 0 ||
        !normalized(sidecar->parent, current, sizeof(current)) || strcmp(current, sidecar->parent) != 0 ||
        lstat(sidecar->parent, &parent) != 0 || fstat(sidecar->parent_descriptor, &parent_witness) != 0 ||
        !S_ISDIR(parent.st_mode) || !identity(&parent, &parent_witness)) return false;
    if (sidecar->missing) return lstat(sidecar->path, &file) != 0 && errno == ENOENT;
    if (sidecar->descriptor < 0 || lstat(sidecar->path, &file) != 0 || fstat(sidecar->descriptor, &file_witness) != 0 ||
        !S_ISREG(file.st_mode) || file.st_nlink != 1 || !identity(&file, &file_witness)) return false;
    return !sidecar->track_predecessor || predecessor_unchanged(&file, &sidecar->predecessor);
}

bool physics_sim_headless_sidecar_replace(const char *path, PhysicsSimHeadlessSidecar *sidecar) {
    memset(sidecar, 0, sizeof(*sidecar));
    sidecar->descriptor = sidecar->parent_descriptor = -1;
    sidecar->pending_descriptor = sidecar->pending_stream_descriptor = -1;
    sidecar->track_predecessor = true;
    char selected[1024], cwd[1024];
    if (!path || !path[0]) return false;
    if (path[0] == '/') {
        if (snprintf(selected, sizeof(selected), "%s", path) >= (int)sizeof(selected)) return false;
    } else if (!getcwd(cwd, sizeof(cwd)) || snprintf(selected, sizeof(selected), "%s/%s", cwd, path) >= (int)sizeof(selected)) return false;
    char *slash = strrchr(selected, '/');
    if (!slash || slash == selected || !slash[1] || strcmp(slash+1, ".") == 0 || strcmp(slash+1, "..") == 0 ||
        strcmp(slash+1, ".git") == 0 || strcmp(slash+1, ".ssh") == 0 || strcmp(slash+1, ".aws") == 0) return false;
    *slash = '\0';
    if (!normalized(selected, sidecar->parent, sizeof(sidecar->parent)) ||
        snprintf(sidecar->path, sizeof(sidecar->path), "%s/%s", sidecar->parent, slash+1) >= (int)sizeof(sidecar->path) ||
        !admitted(sidecar->path)) return false;
    sidecar->parent_descriptor = open(sidecar->parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (sidecar->parent_descriptor < 0) return false;
    sidecar->descriptor = openat(sidecar->parent_descriptor, slash + 1, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (sidecar->descriptor < 0 && errno == ENOENT) sidecar->missing = true;
    else if (sidecar->descriptor < 0 || fstat(sidecar->descriptor, &sidecar->predecessor) != 0) {
        physics_sim_headless_sidecar_close(sidecar); return false;
    }
    if (!physics_sim_headless_sidecar_check(sidecar)) { physics_sim_headless_sidecar_close(sidecar); return false; }
    return true;
}

bool physics_sim_headless_sidecar_claim(const char *path, PhysicsSimHeadlessSidecar *sidecar) {
    sidecar->descriptor = -1; sidecar->parent_descriptor = -1;
    sidecar->pending_descriptor = -1; sidecar->pending_stream_descriptor = -1;
    sidecar->pending_name[0] = '\0'; sidecar->sequence = 0; sidecar->held = false;
    sidecar->missing = false; sidecar->track_predecessor = false;
    if (snprintf(sidecar->path, sizeof(sidecar->path), "%s", path) >= (int)sizeof(sidecar->path)) return false;
    snprintf(sidecar->parent, sizeof(sidecar->parent), "%s", path);
    char *slash = strrchr(sidecar->parent, '/');
    if (!slash || slash == sidecar->parent) return false;
    *slash = '\0';
    char current[1024];
    if (!normalized(sidecar->parent, current, sizeof(current)) || strcmp(current, sidecar->parent) != 0 || !admitted(path)) return false;
    sidecar->parent_descriptor = open(sidecar->parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (sidecar->parent_descriptor < 0) return false;
    sidecar->descriptor = openat(sidecar->parent_descriptor, slash + 1, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (sidecar->descriptor < 0 || !physics_sim_headless_sidecar_check(sidecar) ||
        fsync(sidecar->descriptor) != 0 || fsync(sidecar->parent_descriptor) != 0) {
        physics_sim_headless_sidecar_close(sidecar); return false;
    }
    return true;
}

FILE *physics_sim_headless_sidecar_stream(PhysicsSimHeadlessSidecar *sidecar) {
    if (!physics_sim_headless_sidecar_check(sidecar) || sidecar->pending_name[0]) return NULL;
    int fd = -1;
    for (int attempt = 0; attempt < 64; ++attempt) {
        if (++sidecar->sequence == 0) { sidecar->held = true; return NULL; }
        snprintf(sidecar->pending_name, sizeof(sidecar->pending_name),
                 ".headless-sidecar-%ld-%lu.pending", (long)getpid(), sidecar->sequence);
        fd = openat(sidecar->parent_descriptor, sidecar->pending_name,
                    O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) { sidecar->held = true; return NULL; }
    sidecar->pending_descriptor = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    sidecar->pending_stream_descriptor = fd;
    if (sidecar->pending_descriptor < 0 || fsync(fd) != 0 || fsync(sidecar->parent_descriptor) != 0) {
        close(fd); sidecar->pending_stream_descriptor = -1; sidecar->held = true; return NULL;
    }
    FILE *stream = fdopen(fd, "wb");
    if (!stream) { close(fd); sidecar->pending_stream_descriptor = -1; sidecar->held = true; }
    return stream;
}

bool physics_sim_headless_sidecar_publish(PhysicsSimHeadlessSidecar *sidecar, FILE *stream) {
    struct stat staged, witness, stream_identity;
    bool valid = sidecar->pending_descriptor >= 0 &&
        fileno(stream) == sidecar->pending_stream_descriptor &&
        fstat(sidecar->pending_descriptor, &witness) == 0 &&
        fstat(fileno(stream), &stream_identity) == 0 && identity(&witness, &stream_identity) &&
        S_ISREG(witness.st_mode) && witness.st_nlink == 1;
    if (valid) valid = !ferror(stream) && fflush(stream) == 0 && !ferror(stream) && fsync(fileno(stream)) == 0;
    if (fclose(stream) != 0) valid = false;
    sidecar->pending_stream_descriptor = -1;
    if (valid) valid = physics_sim_headless_sidecar_check(sidecar) &&
        fstatat(sidecar->parent_descriptor, sidecar->pending_name, &staged, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(staged.st_mode) && staged.st_nlink == 1 && identity(&staged, &witness);
    const char *name = strrchr(sidecar->path, '/');
    if (valid) valid = name && renameat(sidecar->parent_descriptor, sidecar->pending_name,
                                      sidecar->parent_descriptor, name + 1) == 0;
    if (valid) {
        if (sidecar->descriptor >= 0) close(sidecar->descriptor);
        sidecar->descriptor = sidecar->pending_descriptor;
        sidecar->missing = false;
        if (fstat(sidecar->descriptor, &sidecar->predecessor) != 0) valid = false;
        sidecar->pending_descriptor = -1; sidecar->pending_name[0] = '\0';
        valid = valid && fsync(sidecar->parent_descriptor) == 0 && physics_sim_headless_sidecar_check(sidecar);
    }
    if (!valid) sidecar->held = true;
    return valid;
}

void physics_sim_headless_sidecar_close(PhysicsSimHeadlessSidecar *sidecar) {
    if (sidecar->descriptor >= 0) close(sidecar->descriptor);
    if (sidecar->parent_descriptor >= 0) close(sidecar->parent_descriptor);
    if (sidecar->pending_descriptor >= 0) close(sidecar->pending_descriptor);
    sidecar->descriptor = -1; sidecar->parent_descriptor = -1;
    sidecar->pending_descriptor = -1;
}
